#ifndef APP_TYPES_H
#define APP_TYPES_H

enum class Page {
  Boot,
  Main,
  WiFiMenu,
  BLEMenu,
  LabMenu,
  IRMenu,
  AppsMenu,
  GamesMenu,
  Diagnostics,
  Settings,
  About,
  NotImplemented,
  WiFiDetails,
  WiFiProbeMonitor,
  WiFiChannelOccupancy,
  BLEDetails,
  BLEPacketInspector,
  DeviceIdentity
};

enum ButtonEvent {
  BTN_NONE,
  BTN_UP,
  BTN_DOWN,
  BTN_SELECT,
  BTN_BACK,
  BTN_UP_LONG,
  BTN_DOWN_LONG,
  BTN_SELECT_LONG,
  BTN_BACK_LONG
};

enum class RadioState {
  RADIO_OFF,
  RADIO_WIFI,
  RADIO_BLE
};

#endif
