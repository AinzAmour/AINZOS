#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <Arduino.h>
#include "app_types.h"
#include "display.h"
#include "buttons.h"
#include "settings.h"
#include "settings_ui.h"
#include "diagnostics.h"
#include "wifi_ui.h"
#include "ble_ui.h"
#include "apps.h"
#include "games.h"
#include "ir_ui.h"

#include "nav_stack.h"

extern RadioState currentRadioState;
extern NavStack navStack;

class MainWindow {
public:
  MainWindow();
  ~MainWindow();
  void begin();
  void update();
  void changePage(Page newPage);
  SettingsManager* getSettings() { return &settings; }
  void triggerClockSaverMode(int mode);
  bool canAutoStartClockSaver();

  void setClockSaverActive(bool active) { clockSaverActive = active; }
  void resetGlobalIdleTimer() { globalLastActivityMs = millis(); }
  void markClockSaverExited();

  bool clockSaverIsRunning() { return clockSaverActive; }

private:
  uint32_t globalLastActivityMs;
  uint32_t lastClockSaverExitMs = 0;
  bool clockSaverActive = false;
  DisplayWrapper display;
  Buttons buttons;
  SettingsManager settings;
  SettingsUI* settingsUI;
  Diagnostics* diagnosticsUI;
  WifiUI* wifiUI;
  BleUI* bleUI;
  AppsUI* appsUI;
  GamesUI* gamesUI;
  IrUI* irUI;
  
  Page currentPage;
  unsigned long stateStartTime;
  
  int mainSelectedIndex;
  int mainTopIndex;
  static const int MAIN_MENU_COUNT = 9;
  const char* mainMenuItems[9] = {
    "WiFi Tools",
    "BLE Tools",
    "Lab Tools",
    "IR Remote",
    "Apps",
    "Games",
    "Diagnostics",
    "Settings",
    "About"
  };

  void handleBoot(ButtonEvent btn);
  void handleMain(ButtonEvent btn);
  void handleNotImplemented(ButtonEvent btn);
  void handleAbout(ButtonEvent btn);
};

extern MainWindow window;

#endif
