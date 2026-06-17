#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>
#include "clock_saver.h"

class SettingsManager {
public:
  uint8_t contrast;      // 0 / 63 / 126 / 189 / 252 / 255
  uint8_t anim_speed;    // 0=Slow, 1=Normal, 2=Fast
  uint8_t wifi_region;   // 0=IN, 1=US, 2=JP
  uint8_t ble_duration;  // 3, 5, 10, 15
  uint8_t dim_time;      // 15, 30, 60, 0 (off)
  bool boot_logo;        // true=ON, false=OFF
  uint8_t scroll_speed;  // 0=Slow, 1=Med, 2=Fast
  uint16_t probe_hop;    // 50, 100, 200, 500 (ms)
  uint8_t ble_filter;    // 0=All, 1=Trackers, 2=UART, 3=Unknown
  uint8_t temp_unit;     // 0=Celsius, 1=Fahrenheit
  
  ClockSaverSettings cs;

  void begin();
  void load();
  void save();
  void factoryReset();

private:
  Preferences prefs;
  void loadDefaults();
};

#endif
