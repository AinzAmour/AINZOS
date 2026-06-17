#ifndef PROBE_RAIN_UI_H
#define PROBE_RAIN_UI_H

#include "display.h"
#include "buttons.h"
#include "wifi_utils.h"

struct RainColumn {
  char ssid[33];
  float y;           // current y position (top of string)
  float speed;       // fall speed in px/frame
  bool active;
  uint8_t len;       // cached strlen(ssid)
};

class ProbeRainUI {
public:
  ProbeRainUI(DisplayWrapper* disp);
  void enter();
  void stop();
  bool update(ButtonEvent btn);

private:
  DisplayWrapper* display;
  bool paused;
  bool running;
  uint32_t lastFrameMs;
  uint32_t capturedCount;
  uint8_t lastProbeCount;  // track when new probes arrive

  static const int NUM_COLUMNS = 8;
  static const int COL_WIDTH = 16;  // 128 / 8
  RainColumn columns[NUM_COLUMNS];

  void assignProbe(const char* ssid);
  void drawFrame();
};

#endif
