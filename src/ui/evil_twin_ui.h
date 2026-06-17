#ifndef EVIL_TWIN_UI_H
#define EVIL_TWIN_UI_H

#include "display.h"
#include "buttons.h"
#include "attacks/evil_twin.h"
#include <WiFi.h>

enum class EvilTwinPage {
  Scanning,
  APPicker,
  Confirm,
  Running,
  Captures
};

class EvilTwinUI {
public:
  EvilTwinUI(DisplayWrapper* disp);
  ~EvilTwinUI();

  void enter();
  bool update(ButtonEvent btn);

private:
  DisplayWrapper* display;
  EvilTwin twin;
  EvilTwinPage currentPage;

  // AP scan results
  static const int MAX_SCAN = 20;
  struct ScanAP { char ssid[33]; uint8_t channel; int rssi; };
  ScanAP scanAPs[MAX_SCAN];
  int scanCount;
  int scanSel, scanTop;

  // Captures viewer
  int capSel, capTop;
  int capCount;

  unsigned long lastStatusUpdate;

  void startScan();
  void drawAPList();
  void drawConfirm();
  void drawRunning();
  void drawCaptures();
};

#endif
