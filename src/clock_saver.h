/*
 * --- STEP 0 — CODEBASE AUDIT CONFIRMATION ---
 * Menu registration method found: In AppsUI (src/apps.h & src/apps.cpp), using appsMenuItems array and APPS_MENU_COUNT.
 * NVS namespace found: "ainzos" used with the ESP32 Preferences library.
 * u8g2 object name found: u8g2 is NOT used in AINZOS. Instead, Adafruit_SSD1306 display is wrapped in DisplayWrapper.
 * Button event type/enum found: ButtonEvent (enum, in include/app_types.h) containing BTN_UP, BTN_DOWN, BTN_SELECT, BTN_BACK, etc.
 * Wi-Fi credential source found: No Wi-Fi credential source existed in the original codebase; added static config in clock_saver_config.h.
 */

#ifndef CLOCK_SAVER_H
#define CLOCK_SAVER_H

#include <Arduino.h>
#include "app_types.h"
#include "display.h"

class SettingsManager;

struct ClockSaverSettings {
  bool enabled          = true;
  bool bootClockEnabled = true;
  bool autoStartOnIdle  = false;
  bool wifiClock        = true;
  bool ntpSync          = true;
  bool animationEnabled = true;
  bool deepSleepEnabled = false;   // OFF by default

  uint32_t clockDurationMs     = 8000;
  uint32_t animationDurationMs = 12000;
  uint32_t idleAnimationTimeoutMs = 30000;
  uint32_t sleepTimeoutMs      = 300000;
  uint32_t globalIdleTimeoutMs = 60000;
  uint8_t  animationSpeedMs    = 90;
  bool     use24h              = false;
};

enum ClockSaverEntryMode {
  CS_ENTRY_MANUAL = 0,
  CS_ENTRY_GLOBAL_IDLE = 1,
  CS_ENTRY_BOOT_CLOCK = 2
};

enum ClockSaverState {
  CS_BOOT_ANIM,
  CS_WIFI_CONNECT,
  CS_NTP_SYNC,
  CS_SHOW_CLOCK,
  CS_SHOW_ANIMATION,
  CS_SLEEP_COUNTDOWN,
  CS_DEEP_SLEEP,
  CS_ERROR
};

enum RobotMood {
  MOOD_IDLE,
  MOOD_HAPPY,
  MOOD_SLEEPY,
  MOOD_BLINK,
  MOOD_LOOK_LEFT,
  MOOD_LOOK_RIGHT,
  MOOD_SURPRISED
};

// Main execution function for Clock Saver App
void handleClockSaverApp(DisplayWrapper* display, SettingsManager* settings, ButtonEvent btn);
void initClockSaver();
void startClockSaver(int mode);
void exitClockSaverToMainMenu();

#endif // CLOCK_SAVER_H
