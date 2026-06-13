#ifndef BLE_UI_H
#define BLE_UI_H

#include "app_types.h"
#include "display.h"
#include "buttons.h"
#include "ble_utils.h"
#include "shared_pager.h"

enum class BlePage {
  Menu,
  Scanning,
  ResultsList,
  Details,
  StrongestDevice,
  DeviceInfo
};

enum class BleFilter {
  ALL = 0,
  TRACKERS,
  UART,
  UNKNOWN
};

class SettingsManager;

class BleUI {
public:
  BleUI(DisplayWrapper* disp, SettingsManager* sm = nullptr);
  void begin();
  void enter(BlePage target = BlePage::Menu);
  bool update(ButtonEvent btn);
  BleScanner* getScanner() { return &scanner; }
  BlePage getCurrentPage() const { return currentPage; }

private:
  DisplayWrapper* display;
  SettingsManager* settings;
  BleScanner scanner;
  BlePage currentPage;
  
  int menuSelectedIndex;
  int menuTopIndex;
  
  Pager resultsPager;
  BleFilter filterMode;
  
  static const int BLE_MENU_COUNT = 6;
  char bleFilterMenuItem[24];
  const char* bleMenuItems[6] = {
    "Scan BLE Devices",
    "Saved BLE Results",
    bleFilterMenuItem,
    "Strongest Device",
    "BLE Device Info",
    "Clear Results"
  };

  unsigned long scanAnimTime;
  int scanAnimFrame;

  void drawMenu();
  void drawScanning();
  void drawResultsList();
  void drawDetails();
  void drawStrongestDevice();
  void drawDeviceInfo();
  
  void handleResultsList(ButtonEvent btn);
  const char* getBLETypeTag(uint8_t t);
  bool matchesFilter(const BleDevice& dev);
};

#endif
