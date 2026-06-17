#ifndef APPS_H
#define APPS_H

#include "app_types.h"
#include "display.h"
#include "buttons.h"
#include "wifi_utils.h"
#include "ble_utils.h"
#include "shared_pager.h"
#include "ui/radar_ui.h"
#include "ui/probe_rain_ui.h"

enum class AppsPage {
  Menu,
  Stopwatch,
  WiFiRadar,
  ProbeRain,
  BleInspector,
  WiFiChannelOccupancy,
  DeviceIdentity,
  Uptime,
  SignalMeter,
  ClockSaver
};

class AppsUI {
public:
  AppsUI(DisplayWrapper* disp, WifiScanner* ws, BleScanner* bs);
  void enter(AppsPage target = AppsPage::Menu);
  bool update(ButtonEvent btn); 
  uint8_t getInspectDeviceIdx() const { return selectedDeviceIdx; }

private:
  DisplayWrapper* display;
  WifiScanner* wifiScanner;
  BleScanner* bleScanner;
  AppsPage currentPage;
  
  int menuSelectedIndex;
  int menuTopIndex;
  
  static const int APPS_MENU_COUNT = 9;
  const char* appsMenuItems[9] = {
    "Stopwatch",
    "WiFi Radar",
    "Probe Rain",
    "BLE Packet Inspector",
    "WiFi Chan Occupancy",
    "Device Identity",
    "Uptime Monitor",
    "Signal Meter",
    "Clock Saver"
  };

  void drawMenu();
  void handleStopwatch(ButtonEvent btn);
  void handleWiFiRadar(ButtonEvent btn);
  void handleProbeRain(ButtonEvent btn);
  void handleBleInspector(ButtonEvent btn);
  void handleWiFiChannelOccupancy(ButtonEvent btn);
  void handleDeviceIdentity(ButtonEvent btn);
  void handleUptime(ButtonEvent btn);
  void handleSignalMeter(ButtonEvent btn);
  void handleClockSaver(ButtonEvent btn);
  
  // Stopwatch states
  bool swRunning;
  unsigned long swStartTime;
  unsigned long swElapsed;
  
  // BLE Inspector states
  Pager inspectPager;
  uint8_t selectedDeviceIdx;
  
  // Uptime state
  unsigned long lastUptimeUpdate;

  // Radar & Rain UI
  RadarUI* radarUI;
  ProbeRainUI* probeRainUI;
};

#endif
