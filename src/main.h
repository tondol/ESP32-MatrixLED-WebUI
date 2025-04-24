#include <Arduino.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <AnimatedGIF.h>
#include <ESPAsyncWebServer.h>

#define FIRMWARE_VERSION "v1.0.0 (original: v0.2.2a)"

struct Config
{
  String ssid;           // wifi ssid
  String wifipassword;   // wifi password
  String httpuser;       // username to access web admin
  String httppassword;   // password to access web admin
  int webserverporthttp; // http port number for web admin
};

// function defaults
String listFiles(bool ishtml = false);
String humanReadableSize(const size_t bytes);

enum Mode {
  INITIAL = 0,
  PLAY_GIF,
  PLAY_NEXT_GIF,
  PLAY_TEXT_1,
  PLAY_NEXT_TEXT_1,
  PLAY_TEXT_2,
  PLAY_NEXT_TEXT_2,
  SHOULD_REBOOT,
};

// 座標の値が有効かどうかをチェックするためのヘルパー
#define VALID_X(x) ((x) >= 0 && (x) < PANEL_RES_X * PANEL_CHAIN)
#define VALID_Y(y) ((y) >= 0 && (y) < PANEL_RES_Y)
#define MAX_TEXT_LENGTH 512
