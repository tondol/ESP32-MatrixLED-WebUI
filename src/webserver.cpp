#include <Arduino.h>

#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "main.h"
#include "webserver.h"
#include "webpages.h"

extern MatrixPanel_I2S_DMA *dma_display;

extern int sliderValue;

extern Config config;
extern AsyncWebServer *server;

extern Mode mode;
extern String nextGifFileName;
extern String playText1;
extern String playText2;

// parses and processes webpages
// if the webpage has %SOMETHING% or %SOMETHINGELSE% it will replace those strings with the ones defined
String processor(const String& var) {
  if (var == "FIRMWARE") {
    return FIRMWARE_VERSION;
  }
  if (var == "FREELittleFS") {
    return humanReadableSize((LittleFS.totalBytes() - LittleFS.usedBytes()));
  }
  if (var == "USEDLittleFS") {
    return humanReadableSize(LittleFS.usedBytes());
  }
  if (var == "TOTALLittleFS") {
    return humanReadableSize(LittleFS.totalBytes());
  }
  if (var == "SLIDERVALUE"){
    return String(sliderValue);
  }
  return "";
}

// used by server.on functions to discern whether a user has the correct httpapitoken OR is authenticated by username and password
bool checkUserWebAuth(AsyncWebServerRequest * request) {
  bool isAuthenticated = false; // replace false with true if you want to disable authentication

  if (request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
    Serial.println("is authenticated via username and password");
    isAuthenticated = true;
  }
  return isAuthenticated;
}

// handles uploads to the filserver
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  // make sure authenticated before allowing upload
  if (checkUserWebAuth(request)) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);

    if (!index) {
      logmessage = "Upload Start: " + String(filename);
      // open the file on first call and store the file handle in the request object
      request->_tempFile = LittleFS.open("/" + filename, "w");
      Serial.println(logmessage);
    }

    if (len) {
      // stream the incoming chunk to the opened file
      request->_tempFile.write(data, len);
      logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
      Serial.println(logmessage);
    }

    if (final) {
      logmessage = "Upload Complete: " + String(filename) + " size=" + String(index + len);
      // close the file handle as the upload is now done
      request->_tempFile.close();
      Serial.println(logmessage);
      request->redirect("/");
    }
  } else {
    Serial.println("Auth: Failed");
    return request->requestAuthentication();
  }
}

void notFound(AsyncWebServerRequest *request) {
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  Serial.println(logmessage);
  request->send(404, "text/plain", "Not found");
}

void configureWebServer() {
  // configure web server

  // if url isn't found
  server->onNotFound(notFound);

  // run handleUpload function when any file is uploaded
  server->onFileUpload(handleUpload);

  // visiting this page will cause you to be logged out
  server->on("/logout", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->requestAuthentication();
    request->send(401);
  });

  // presents a "you are now logged out webpage
  server->on("/logged-out", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    request->send(401, "text/html", logout_html, processor);
  });

  server->on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Auth: Success";
      Serial.println(logmessage);
      request->send(200, "text/html", index_html, processor);
    } else {
      logmessage += " Auth: Failed";
      Serial.println(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/slider", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    // GET input1 value on <ESP_IP>/slider?value=<inputMessage>
    if (request->hasParam("value")) {
      sliderValue = request->getParam("value")->value().toInt();
      dma_display->setBrightness8(sliderValue);
      request->send(200, "text/html", "OK");
    } else {
      request->send(400, "text/plain", "ERROR: value param required");
      Serial.println(logmessage + " ERROR: value param required");
    }
  });

  server->on("/reboot", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      request->send(200, "text/html; charset=UTF-8", reboot_html);
      logmessage += " Auth: Success";
      Serial.println(logmessage);
      ESP.restart();
      mode = SHOULD_REBOOT;
    } else {
      logmessage += " Auth: Failed";
      Serial.println(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/listfiles", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Auth: Success";
      Serial.println(logmessage);
      request->send(200, "text/html; charset=UTF-8", listFiles(true));
    } else {
      logmessage += " Auth: Failed";
      Serial.println(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/playtext1", HTTP_POST, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Auth: Success";
      Serial.println(logmessage);

      if (!request->hasArg("text1") || !request->hasArg("text2")) {
        request->send(400, "text/plain", "ERROR: text1 or text2 param required");
        Serial.println(logmessage + " ERROR: text1 or text2 param required");
        return;
      }
      if (mode == INITIAL) {
        request->send(400, "text/plain", "ERROR: mode is INITIAL");
        Serial.println(logmessage + " ERROR: mode is INITIAL");
        return;
      }

      playText1 = String(request->arg("text1").substring(0, MAX_TEXT_LENGTH));
      playText2 = String(request->arg("text2").substring(0, MAX_TEXT_LENGTH));
      mode = PLAY_NEXT_TEXT_1;
      request->send(200, "text/plain", "OK");
    } else {
      logmessage += " Auth: Failed";
      Serial.println(logmessage);
      return request->requestAuthentication();
    }
  });
  server->on("/playtext2", HTTP_POST, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Auth: Success";
      Serial.println(logmessage);

      if (!request->hasArg("text1")) {
        request->send(400, "text/plain", "ERROR: text1 param required");
        Serial.println(logmessage + " ERROR: text1 param required");
        return;
      }
      if (mode == INITIAL) {
        request->send(400, "text/plain", "ERROR: mode is INITIAL");
        Serial.println(logmessage + " ERROR: mode is INITIAL");
        return;
      }

      playText1 = String(request->arg("text1").substring(0, MAX_TEXT_LENGTH));
      mode = PLAY_NEXT_TEXT_2;
      request->send(200, "text/plain", "OK");
    } else {
      logmessage += " Auth: Failed";
      Serial.println(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/file", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logHeader = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

    if (!checkUserWebAuth(request)) {
      Serial.println(logHeader + " Auth: Failed");
      return request->requestAuthentication();
    }

    if (!request->hasParam("name") || !request->hasParam("action")) {
      request->send(400, "text/plain", "ERROR: name and action params required");
      Serial.println(logHeader + " ERROR: name and action params required");
      return;
    }
    if (mode == INITIAL) {
      request->send(400, "text/plain", "ERROR: mode is INITIAL");
      Serial.println(logHeader + " ERROR: mode is INITIAL");
      return;
    }

    String fileName = "/" + String(request->getParam("name")->value());
    String fileAction = request->getParam("action")->value();
    logHeader = "Client:" + request->client()->remoteIP().toString() + " " + request->url() + "?name=" + String(fileName) + "&action=" + String(fileAction);

    if (!LittleFS.exists(fileName)) {
      // ファイルが存在しない場合
      request->send(400, "text/plain", "ERROR: file does not exist");
      Serial.println(logHeader + " ERROR: file does not exist");
      return;
    }
    if (!fileName.endsWith(".gif")) {
      // GIFファイル以外が要求された場合
      request->send(400, "text/plain", "ERROR: file is not a gif");
      Serial.println(logHeader + " ERROR: file is not a gif");
      return;
    }

    if (fileAction == "delete") {
      LittleFS.remove(fileName);
      request->send(200, "text/plain", "Deleted File: " + String(fileName));
      Serial.println(logHeader + " deleted");

    } else if (fileAction == "play") {
      nextGifFileName = fileName;
      mode = PLAY_NEXT_GIF;
      Serial.println(logHeader + " opening");

    } else if (fileAction == "show") {
      fs::File f = LittleFS.open(fileName, "r");
      // プレビュー用に冒頭5KBだけを返す
      size_t size = min((size_t)5*1024, f.size());
      AsyncWebServerResponse* response = request->beginResponse("image/gif", size, [f, size](uint8_t* buffer, size_t maxLen, size_t total) mutable -> size_t {
        int bytes = f.read(buffer, maxLen);
        if (bytes + total == size) {
          f.close();
        }
        return max(0, bytes);
      });
      delay(100); // 早すぎると後続のメモリ確保に失敗する??
      response->addHeader("Cache-Control", "max-age=3600");
      request->send(response);
      Serial.println(logHeader + " previewing");

    } else {
      request->send(400, "text/plain", "ERROR: invalid action param supplied");
      Serial.println(logHeader + " ERROR: invalid action param supplied");
    }
  });
}
