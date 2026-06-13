#ifndef WIFI_UI_H
#define WIFI_UI_H

#include "app_types.h"
#include "display.h"
#include "buttons.h"
#include "wifi_utils.h"
#include "shared_pager.h"

enum class WifiPage {
  Menu,
  Scanning,
  ResultsList,
  Details, // Note: kept as local tracker, but we also transition globally
  ChannelAnalyzer,
  OpenNetworks,
  StrongestNetwork,
  DeviceInfo,
  ProbeMonitor // NEW
};

class WifiUI {
public:
  WifiUI(DisplayWrapper* disp);
  void begin();
  void enter(WifiPage target = WifiPage::Menu);
  bool update(ButtonEvent btn); 
  WifiScanner* getScanner() { return &scanner; }
  void stopProbeMonitorSniffer();
  WifiPage getCurrentPage() const { return currentPage; }

private:
  DisplayWrapper* display;
  WifiScanner scanner;
  WifiPage currentPage;
  
  int menuSelectedIndex;
  int menuTopIndex;
  
  // Custom paginated list offsets
  Pager resultsPager;
  Pager probePager;
  
  static const int WIFI_MENU_COUNT = 8;
  const char* wifiMenuItems[8] = {
    "Scan Networks",
    "Saved Results",
    "Probe Monitor",
    "Channel Analyzer",
    "Open Networks",
    "Strongest Network",
    "Device Info",
    "Clear Results"
  };

  unsigned long scanAnimTime;
  int scanAnimFrame;

  void drawMenu();
  void drawScanning();
  void drawResultsList();
  void drawDetails();
  void drawChannelAnalyzer();
  void drawOpenNetworks();
  void drawStrongestNetwork();
  void drawDeviceInfo();
  void drawProbeMonitor();
  
  const char* getEncBadge(uint8_t enc);
  void handleResultsList(ButtonEvent btn);
  void handleProbeMonitor(ButtonEvent btn);
};

#endif
