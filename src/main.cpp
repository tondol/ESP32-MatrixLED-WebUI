// By mzashh https://github.com/mzashh

#include <Arduino.h>

#include <LittleFS.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <AnimatedGIF.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "ESP32_LittleFS_ShinonomeFNT.h"
#include "ESP32_LittleFS_UTF8toSJIS.h"

#include "main.h"
#include "webserver.h"
#include "webpages.h"
#include "wifiinfo.h"

#define R1 25
#define G1 26
#define BL1 27
#define R2 14
#define G2 12
#define BL2 13
#define E_PIN 32

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 2

// 
MatrixPanel_I2S_DMA *dma_display = nullptr;

String sliderValue = "100"; // default brightness value

Config config;             // configuration
AsyncWebServer *server;    // initialise webserver

Mode mode = PLAY_GIF_INITIAL; // default mode
// PLAY_GIF
File gifFile;
AnimatedGIF gif;
String nextGifFileName = "";
// PLAY_TEXT
String playText1 = "";
String playText2 = "";
uint8_t fontBuf1[MAX_TEXT_LENGTH * 2][16] = {0};
uint8_t fontBuf2[MAX_TEXT_LENGTH * 2][16] = {0};
uint16_t sjLength1 = 0;
uint16_t sjLength2 = 0;
int16_t scrollMinX1 = 0;
int16_t scrollMinX2 = 0;
int16_t scrollMaxX1 = 0;
int16_t scrollMaxX2 = 0;
int16_t scrollX1 = 0;
int16_t scrollX2 = 0;

//
uint16_t myBLACK = dma_display->color565(0, 0, 0);
uint16_t myWHITE = dma_display->color565(255, 255, 255);
uint16_t myRED = dma_display->color565(255, 0, 0);
uint16_t myGREEN = dma_display->color565(0, 255, 0);
uint16_t myBLUE = dma_display->color565(0, 0, 255);

File f;

//
const char* UTF8SJIS_file = "/Utf8Sjis.tbl";
const char* Shino_Zen_Font_file = "/shnmk16.bdf";
const char* Shino_Half_Font_file = "/shnm8x16.bdf";

ESP32_LittleFS_ShinonomeFNT SFR;

void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > MATRIX_WIDTH * PANEL_CHAIN)
    iWidth = MATRIX_WIDTH * PANEL_CHAIN;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < pDraw->iWidth)
    {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        }
        else // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      } // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        for (int xOffset = 0; xOffset < iCount; xOffset++)
        {
          dma_display->drawPixel(x + xOffset, y, usTemp[xOffset]); // 565 Color Format
        }
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount)
      {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  }
  else // does not have transparency
  {
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < pDraw->iWidth; x++)
    {
      dma_display->drawPixel(x, y, usPalette[*s++]); // color 565
    }
  }
} /* GIFDraw() */

void *GIFOpenFile(const char *fname, int32_t *pSize)
{
  Serial.print("Playing gif: ");
  Serial.println(fname);
  f = LittleFS.open(fname);
  if (f)
  {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
    f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
  int32_t iBytesRead;
  iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  // Note: If you read a file all the way to the last byte, seek() stops working
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
  if (iBytesRead <= 0)
    return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  //  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

unsigned long start_tick = 0;

void ShowGIF(char *name)
{
  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    while (gif.playFrame(true, NULL))
    {
      if (mode != PLAY_GIF)
      {
        break;
      }
    }
    gif.close();
  }
} /* ShowGIF() */

void rebootESP(String message)
{
  Serial.print("Rebooting ESP32: ");
  Serial.println(message);
  ESP.restart();
}

String listFiles(bool ishtml)
{ // list all of the files, if ishtml=true, return html rather than simple text
  String returnText = "";
  Serial.println("Listing files stored on LittleFS");
  File root = LittleFS.open("/");
  File foundfile = root.openNextFile();
  if (ishtml)
  {
    returnText += "<table><tr><th align='left'>Name</th><th align='left'>Size</th><th align='justify'>GIF</th><th></th><th></th></tr>";
  }
  while (foundfile)
  {
    if (ishtml)
    {
      returnText += "<tr align='left'><td>" + String(foundfile.name()) + "</td><td>" + humanReadableSize(foundfile.size()) + "</td>";
      returnText += "<td>"
                    "<img src="
                    "file?name=" +
                    String(foundfile.name()) + "&action=show>";
      returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'play\')\">Play</button>";
      returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'download\')\">Download</button>";
      returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'delete\')\">Delete</button></tr>";
    }
    else
    {
      returnText += "File: " + String(foundfile.name()) + " Size: " + humanReadableSize(foundfile.size()) + "\n";
    }
    foundfile = root.openNextFile();
  }
  if (ishtml)
  {
    returnText += "</table>";
  }
  root.close();
  foundfile.close();
  return returnText;
}

String humanReadableSize(const size_t bytes)
{
  if (bytes < 1024)
    return String(bytes) + " B";
  else if (bytes < (1024 * 1024))
    return String(bytes / 1024.0) + " KB";
  else if (bytes < (1024 * 1024 * 1024))
    return String(bytes / 1024.0 / 1024.0) + " MB";
  else
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

#define FONT_PIXEL_8x16(b, j, i, k) (b[j][i] & (0x80 >> k))

void drawTextIn8x16Font(uint8_t font_buf[][16], int16_t sj_length, int16_t x, int16_t y, uint16_t color){
  for (int j=0; j<sj_length; j++) {
    for(int i=0; i<16; i++) {
      for (int k=0; k<8; k++) {
        int16_t xx = x + 8*j+k;
        int16_t yy = y + i;
        if (FONT_PIXEL_8x16(font_buf, j, i, k)) {
          if (VALID_X(xx)) {
            dma_display->drawPixel(xx, yy, color);
          }
        } else {
          if (VALID_X(xx)) {
            dma_display->drawPixel(xx, yy, myBLACK);
          }
        }
      }
    }
  }
  // 左にスクロールする前提で、残像が残らないように1列分消す
  int16_t maxX = x + 8*sj_length;
  if (VALID_X(maxX)) {
    dma_display->drawRect(maxX, y, 1, 16, myBLACK);
  }
}

void drawTextIn16x32Font(uint8_t font_buf[][16], int16_t sj_length, int16_t x, int16_t y, uint16_t color){
  for (int j=0; j<sj_length; j++) {
    for(int i=0; i<16; i++) {
      for (int k=0; k<8; k++) {
        int16_t xx = x + (8*j+k) * 2;
        int16_t yy = y + i * 2;
        if (FONT_PIXEL_8x16(font_buf, j, i, k)) {
          if (VALID_X(xx)) {
            dma_display->drawPixel(xx, yy, color);
            dma_display->drawPixel(xx, yy + 1, color);
          }
          if (VALID_X(xx + 1)) {
            dma_display->drawPixel(xx + 1, yy, color);
            dma_display->drawPixel(xx + 1, yy + 1, color);
          }
        } else {
          if (VALID_X(xx)) {
            dma_display->drawPixel(xx, yy, myBLACK);
            dma_display->drawPixel(xx, yy + 1, myBLACK);
          }
          if (VALID_X(xx + 1)) {
            dma_display->drawPixel(xx + 1, yy, myBLACK);
            dma_display->drawPixel(xx + 1, yy + 1, myBLACK);
          }
        }
      }
    }
  }
  // 左にスクロールする前提で、残像が残らないように1列分消す
  int16_t maxX = x + 8*sj_length * 2;
  if (VALID_X(maxX)) {
    dma_display->drawRect(maxX, y, 1, 32, myBLACK);
  }
}

/************************* Arduino Sketch Setup and Loop() *******************************/
void setup()
{
  HUB75_I2S_CFG mxconfig(
      PANEL_RES_X, // module width
      PANEL_RES_Y, // module height
      PANEL_CHAIN  // Chain length
  );

  mxconfig.gpio.r1 = BL1; // デフォルトの色の信号番号を実際の信号番号の割り当てに変更
  mxconfig.gpio.g1 = R1;
  mxconfig.gpio.b1 = G1;
  mxconfig.gpio.r2 = BL2;
  mxconfig.gpio.g2 = R2;
  mxconfig.gpio.b2 = G2;
  mxconfig.gpio.e = E_PIN;

  mxconfig.clkphase = false;

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(sliderValue.toInt()); // 0-255
  dma_display->clearScreen();

  Serial.begin(115200);

  Serial.print("Firmware: ");
  Serial.println(FIRMWARE_VERSION);

  Serial.println("Booting ...");

  Serial.println("Mounting LittleFS ...");
  if (!LittleFS.begin(true))
  {
    // if you have not used LittleFS before on a ESP32, it will show this error.
    // after a reboot LittleFS will be configured and will happily work.
    Serial.println("ERROR: Cannot mount LittleFS, Rebooting");
    rebootESP("ERROR: Cannot mount LittleFS, Rebooting");
    delay(1000);
  }

  Serial.print("LittleFS Free: ");
  Serial.println(humanReadableSize(LittleFS.totalBytes() - LittleFS.usedBytes()));
  Serial.print("LittleFS Used: ");
  Serial.println(humanReadableSize(LittleFS.usedBytes()));
  Serial.print("LittleFS Total: ");
  Serial.println(humanReadableSize(LittleFS.totalBytes()));

  Serial.println(listFiles());

  Serial.println("Loading Configuration ...");

  config.ssid = default_ssid;
  config.wifipassword = default_wifipassword;
  config.httpuser = default_httpuser;
  config.httppassword = default_httppassword;
  config.webserverporthttp = default_webserverporthttp;

  Serial.print("\nConnecting to Wifi: ");
  WiFi.begin(config.ssid.c_str(), config.wifipassword.c_str());
  while (WiFi.status() != WL_CONNECTED);
  {
    Serial.print("."); //Uncomment the 3 lines if you want the ESP32 to wait until the WIFI is connected
    delay(500);
  }

  Serial.println("\n\nNetwork Configuration:");
  Serial.println("----------------------");
  Serial.print("         SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("  Wifi Status: ");
  Serial.println(WiFi.status());
  Serial.print("Wifi Strength: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.print("          MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("           IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("       Subnet: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("      Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("        DNS 1: ");
  Serial.println(WiFi.dnsIP(0));
  Serial.print("        DNS 2: ");
  Serial.println(WiFi.dnsIP(1));
  Serial.print("        DNS 3: ");
  Serial.println(WiFi.dnsIP(2));
  Serial.println();

  // configure web server
  Serial.println("Configuring Webserver ...");
  server = new AsyncWebServer(config.webserverporthttp);
  configureWebServer();

  // startup web server
  Serial.println("Starting Webserver ...");
  server->begin();

  /* all other pixel drawing functions can only be called after .begin() */
  dma_display->fillScreen(myBLACK);
  dma_display->setTextSize(1);
  dma_display->setCursor(0, 0);
  dma_display->print("ID:");
  dma_display->print(FIRMWARE_VERSION);
  dma_display->setCursor(0, 16);
  dma_display->print("IP:");
  dma_display->print(WiFi.localIP());
  dma_display->setCursor(0, 38);
  dma_display->print("RSSI:");
  dma_display->println(WiFi.RSSI());
  dma_display->setCursor(0, 52);
  dma_display->print("SSID:");
  dma_display->println(WiFi.SSID());
  delay(3000);
  dma_display->fillScreen(myBLACK);

  gif.begin(LITTLE_ENDIAN_PIXELS);

  SFR.LittleFS_Shinonome_Init3F(UTF8SJIS_file, Shino_Half_Font_file, Shino_Zen_Font_file);
}

String gifDir = "/"; // play all GIFs in this directory on the SD card
char filePath[256] = {0};
File root;

void loop()
{
  while (1) // run forever
  {
    if (mode == SHOULD_REBOOT)
    {
      Serial.println("mode = SHOULD_REBOOT");
      rebootESP("Web Admin Initiated Reboot");
      delay(1000);

    } else if (mode == PLAY_TEXT) {
      Serial.println("mode = PLAY_TEXT");
      drawTextIn16x32Font(fontBuf1, sjLength1, 0 + scrollX1, 0, myRED);
      drawTextIn16x32Font(fontBuf2, sjLength2, 0 + scrollX2, 32, myGREEN);
      scrollX1 = (scrollX1 <= scrollMinX1) ? scrollMaxX1 : (scrollX1 - 1);
      scrollX2 = (scrollX2 <= scrollMinX2) ? scrollMaxX2 : (scrollX2 - 1);
      delay(15); // 15ms + 10ms = 25ms

    } else if (mode == PLAY_NEXT_TEXT) {
      Serial.println("mode = PLAY_NEXT_TEXT");
      Serial.println("playText1: " + playText1);
      Serial.println("playText2: " + playText2);
      sjLength1 = SFR.StrDirect_ShinoFNT_readALL(playText1, fontBuf1);
      sjLength2 = SFR.StrDirect_ShinoFNT_readALL(playText2, fontBuf2);
      bool isText1Fixed = sjLength1 * 16 <= PANEL_RES_X * PANEL_CHAIN;
      bool isText2Fixed = sjLength2 * 16 <= PANEL_RES_X * PANEL_CHAIN;
      scrollMinX1 = isText1Fixed ? (PANEL_RES_X * PANEL_CHAIN - sjLength1 * 16) / 2 : (-sjLength1 * 16);
      scrollMinX2 = isText2Fixed ? (PANEL_RES_X * PANEL_CHAIN - sjLength2 * 16) / 2 : (-sjLength2 * 16);
      scrollMaxX1 = isText1Fixed ? scrollMinX1 : PANEL_RES_X * PANEL_CHAIN;
      scrollMaxX2 = isText2Fixed ? scrollMinX2 : PANEL_RES_X * PANEL_CHAIN;
      scrollX1 = scrollMaxX1;
      scrollX2 = scrollMaxX2;
      dma_display->fillScreen(myBLACK);
      mode = PLAY_TEXT;

    } else if (mode == PLAY_GIF_INITIAL) {
      Serial.println("mode = PLAY_GIF_INITIAL");
      root = LittleFS.open(gifDir);
      gifFile = root.openNextFile();
      mode = PLAY_GIF;

    } else if (mode == PLAY_GIF) {
      Serial.println("mode = PLAY_GIF");
      if (gifFile.isDirectory() || !String(gifFile.name()).endsWith(".gif")) {
        // GIFファイル以外なら次のファイルに進む
        gifFile.close();
        gifFile = root.openNextFile();
      } else {
        // C-strings... urghh...
        memset(filePath, 0x0, sizeof(filePath));
        strcpy(filePath, gifFile.path());
        // Show it.
        ShowGIF(filePath);
      }

    } else if (mode == PLAY_NEXT_GIF) {
      Serial.println("mode = PLAY_NEXT_GIF");
      gifFile.close();
      gifFile = LittleFS.open(nextGifFileName);
      mode = PLAY_GIF;
      nextGifFileName = "";
    }

    delay(10);
  }
} // end loop
