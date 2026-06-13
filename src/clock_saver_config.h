#ifndef CLOCK_SAVER_CONFIG_H
#define CLOCK_SAVER_CONFIG_H

#include <Arduino.h>

static const char* CS_WIFI_SSID = "Habibamour";
static const char* CS_WIFI_PASS = "9841080909";

inline bool hasClockSaverWifiConfig() {
  if (CS_WIFI_SSID == nullptr || strlen(CS_WIFI_SSID) == 0 || strcmp(CS_WIFI_SSID, "YOUR_SSID") == 0) {
    return false;
  }
  return true;
}

#endif // CLOCK_SAVER_CONFIG_H
