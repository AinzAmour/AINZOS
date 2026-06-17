#ifndef RADAR_UI_H
#define RADAR_UI_H

#include "display.h"
#include "buttons.h"
#include <WiFi.h>

// AP blip data for radar display
struct RadarBlip {
  char ssid[33];
  int8_t rssi;
  uint8_t channel;
  float angle;      // assigned angle in radians
  float radius;     // distance from center based on RSSI
  uint32_t hitMs;   // millis() when sweep last passed this blip
  bool active;
};

class RadarUI {
public:
  RadarUI(DisplayWrapper* disp);
  void enter();
  bool update(ButtonEvent btn);

private:
  DisplayWrapper* display;
  bool frozen;
  bool scanning;
  uint32_t lastScanMs;
  uint32_t lastFrameMs;

  static const int MAX_BLIPS = 16;
  RadarBlip blips[MAX_BLIPS];
  int blipCount;

  void refreshScan();
  void drawRadar();
  void drawBlip(Adafruit_SSD1306* d, int cx, int cy, const RadarBlip& blip, float sweepAngle);
  void drawSidePanel(Adafruit_SSD1306* d);
};

#endif
