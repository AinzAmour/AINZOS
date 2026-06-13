#include "clock_saver.h"
#include "clock_saver_config.h"
#include "display.h"
#include "settings.h"
#include "pins.h"
#include <WiFi.h>
#include <time.h>
#include <esp_sleep.h>
#include "mainwindow.h"
#include "nav_stack.h"
#include "robot_eyes.h"

ClockSaverEntryMode currentClockSaverMode = CS_ENTRY_MANUAL;

static ClockSaverState csState = CS_BOOT_ANIM;
static unsigned long stateTimer = 0;
static uint32_t wifiConnectStartMs = 0;
static unsigned long ntpSyncStart = 0;
static unsigned long lastActivityMs = 0;
static unsigned long modeSwitchTime = 0;
static unsigned long lastClockUpdate = 0;
static uint32_t clockStartMs = 0;
static uint32_t animStartMs = 0;

static bool wifiConnectStarted = false;
static bool wifiConnectFinished = false;
static bool wifiCleanupDone = false;
static bool wifiTimedOut = false;
static bool ntpFinished = false;
static bool wifiConnected = false;
static bool ntpSynced = false;

static int burnInX = 0;
static int burnInY = 0;
static unsigned long lastBurnInShift = 0;

static RobotMood currentMood = MOOD_IDLE;
static uint32_t lastMoodChangeMs = 0;

void cleanupWiFiOnce() {
  if (wifiCleanupDone) return;
  wifiCleanupDone = true;
  Serial.println("[ClockSaver] WiFi cleanup");
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(false);
    WiFi.mode(WIFI_OFF);
  }
}

void initClockSaver() {
  uint32_t now = millis();
  lastActivityMs = now;
  clockStartMs = now;
  animStartMs = now;
  modeSwitchTime = now;
  lastClockUpdate = 0;
  
  wifiConnectStarted = false;
  wifiConnectFinished = false;
  wifiCleanupDone = false;
  wifiTimedOut = false;
  ntpFinished = false;
  wifiConnectStartMs = 0;
  
  csState = CS_BOOT_ANIM;
  stateTimer = now;
  lastBurnInShift = now;
  burnInX = 0;
  burnInY = 0;
  wifiConnected = false;
  ntpSynced = false;
  
  currentMood = MOOD_IDLE;
  lastMoodChangeMs = now;
  
  window.setClockSaverActive(true);
  
  Serial.print("[ClockSaver] Timers initialized at: ");
  Serial.println(now);
}

void startClockSaver(int mode) {
  currentClockSaverMode = static_cast<ClockSaverEntryMode>(mode);
  initClockSaver();
}

void exitClockSaverToMainMenu() {
  Serial.println("[ClockSaver] Exit to Main Menu");
  navStack.clear();
  window.markClockSaverExited();
}

void handleClockSaverApp(DisplayWrapper* display, SettingsManager* settings, ButtonEvent btn) {
  uint32_t now = millis();
  
  if (btn != BTN_NONE) {
    lastActivityMs = now;
    if (currentClockSaverMode == CS_ENTRY_BOOT_CLOCK || currentClockSaverMode == CS_ENTRY_GLOBAL_IDLE) {
      Serial.println("[ClockSaver] Button pressed, entering Main Menu");
      exitClockSaverToMainMenu();
      return;
    }
  }

  if (settings->cs.deepSleepEnabled && (now - lastActivityMs >= settings->cs.sleepTimeoutMs)) {
    if (csState != CS_DEEP_SLEEP) {
      csState = CS_DEEP_SLEEP;
    }
  }

  if (btn == BTN_SELECT && currentClockSaverMode == CS_ENTRY_MANUAL) {
    if (csState == CS_SHOW_ANIMATION) {
      csState = CS_SHOW_CLOCK;
      modeSwitchTime = now;
      clockStartMs = now;
      lastClockUpdate = 0;
    } else if (csState == CS_SHOW_CLOCK) {
      csState = CS_SHOW_ANIMATION;
      modeSwitchTime = now;
      animStartMs = now;
      initRobotEyes();
    }
  }

  switch (csState) {
    case CS_BOOT_ANIM: {
      display->clear();
      auto d = display->getDriver();
      d->setTextSize(2);
      d->setTextColor(SSD1306_WHITE);
      d->setCursor(25, 24);
      d->print(F("HIZMOS"));
      d->display();

      if (now - stateTimer >= 1500) {
        if (settings->cs.wifiClock) {
          if (!hasClockSaverWifiConfig()) {
            Serial.println("[ClockSaver] WiFi not set");
            csState = CS_SHOW_CLOCK;
            clockStartMs = now;
          } else {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 0) && timeinfo.tm_year > 100) {
              Serial.println("[ClockSaver] Time already synced, skipping WiFi");
              csState = CS_SHOW_CLOCK;
              clockStartMs = now;
            } else {
              csState = CS_WIFI_CONNECT;
            }
          }
        } else {
          csState = CS_SHOW_CLOCK;
          clockStartMs = now;
        }
      }
      break;
    }

    case CS_WIFI_CONNECT: {
      if (!wifiConnectStarted) {
        wifiConnectStarted = true;
        wifiConnectStartMs = now;
        
        WiFi.mode(WIFI_STA);
        WiFi.persistent(false);
        WiFi.setSleep(true);
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false);
        delay(20);
        
        #if defined(WIFI_POWER_8_5dBm)
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
        #elif defined(WIFI_POWER_8dBm)
        WiFi.setTxPower(WIFI_POWER_8dBm);
        #elif defined(WIFI_POWER_11dBm)
        WiFi.setTxPower(WIFI_POWER_11dBm);
        #else
        WiFi.setTxPower(WIFI_POWER_5dBm);
        #endif
        
        Serial.println("[ClockSaver] WiFi connecting...");
        WiFi.begin(CS_WIFI_SSID, CS_WIFI_PASS);
      }

      display->clear();
      auto d = display->getDriver();
      d->setTextSize(1);
      d->setTextColor(SSD1306_WHITE);
      d->setCursor(20, 20);
      d->print(F("Connecting WiFi"));
      int dots = (now - wifiConnectStartMs) / 500 % 4;
      d->setCursor(55, 35);
      for (int i = 0; i < dots; i++) { d->print(F(".")); }
      d->display();

      if (!wifiConnectFinished) {
        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
          wifiConnected = true;
          wifiConnectFinished = true;
          Serial.print("[ClockSaver] WiFi connected IP=");
          Serial.println(WiFi.localIP());
          if (settings->cs.ntpSync) {
            configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
            csState = CS_NTP_SYNC;
            ntpSyncStart = now;
          } else {
            cleanupWiFiOnce();
            csState = CS_SHOW_CLOCK;
            clockStartMs = now;
          }
        } else if (now - wifiConnectStartMs >= 9000) {
          wifiConnected = false;
          wifiConnectFinished = true;
          wifiTimedOut = true;
          Serial.print("[ClockSaver] WiFi timeout status=");
          Serial.println((int)st);
          cleanupWiFiOnce();
          csState = CS_SHOW_CLOCK;
          clockStartMs = now;
        }
      }
      break;
    }

    case CS_NTP_SYNC: {
      display->clear();
      auto d = display->getDriver();
      d->setTextSize(1);
      d->setTextColor(SSD1306_WHITE);
      d->setCursor(30, 20);
      d->print(F("Syncing NTP"));
      
      int dots = (now - ntpSyncStart) / 500 % 4;
      d->setCursor(55, 35);
      for (int i = 0; i < dots; i++) { d->print(F(".")); }
      d->display();

      if (!ntpFinished) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0)) {
          Serial.println("[ClockSaver] NTP sync OK");
          ntpSynced = true;
          ntpFinished = true;
          cleanupWiFiOnce();
          csState = CS_SHOW_CLOCK;
          clockStartMs = now;
        } else if (now - ntpSyncStart >= 9000) {
          Serial.println("[ClockSaver] NTP sync failed");
          ntpFinished = true;
          cleanupWiFiOnce();
          csState = CS_SHOW_CLOCK;
          clockStartMs = now;
        }
      }
      break;
    }

    case CS_SHOW_CLOCK: {
      if (settings->cs.animationEnabled && 
          (now - clockStartMs >= settings->cs.clockDurationMs)) {
        csState = CS_SHOW_ANIMATION;
        modeSwitchTime = now;
        animStartMs = now;
        initRobotEyes();
        break;
      }

      if (now - lastBurnInShift >= 30000) {
        lastBurnInShift = now;
        burnInX = esp_random() % 3;
        burnInY = esp_random() % 3;
        lastClockUpdate = 0;
      }

      if (now - lastClockUpdate >= 250 || lastClockUpdate == 0) {
        lastClockUpdate = now;
        display->clear();
        auto d = display->getDriver();
        
        struct tm timeinfo;
        bool hasTime = getLocalTime(&timeinfo, 0);
        
        char hourStr[4];
        char minStr[4];
        char dateStr[24];
        bool blink = (now % 1000) < 500;
        
        if (hasTime && timeinfo.tm_year > 100) {
          if (settings->cs.use24h) {
            snprintf(hourStr, sizeof(hourStr), "%02d", timeinfo.tm_hour);
          } else {
            int hour12 = timeinfo.tm_hour % 12;
            if (hour12 == 0) hour12 = 12;
            snprintf(hourStr, sizeof(hourStr), "%02d", hour12);
          }
          snprintf(minStr, sizeof(minStr), "%02d", timeinfo.tm_min);
          
          const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
          const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
          snprintf(dateStr, sizeof(dateStr), "%s, %d %s", days[timeinfo.tm_wday], timeinfo.tm_mday, months[timeinfo.tm_mon]);
        } else {
          snprintf(hourStr, sizeof(hourStr), "--");
          snprintf(minStr, sizeof(minStr), "--");
          snprintf(dateStr, sizeof(dateStr), "NTP Pending");
        }
        
        d->setTextSize(1);
        d->setTextColor(SSD1306_WHITE);
        d->setCursor(0, 0);
        
        char ampmStr[4] = "";
        if (hasTime && timeinfo.tm_year > 100 && !settings->cs.use24h) {
          snprintf(ampmStr, sizeof(ampmStr), "%s", (timeinfo.tm_hour >= 12) ? "PM" : "AM");
        }
        
        char statusRow[32];
        snprintf(statusRow, sizeof(statusRow), "WIFI:%s NTP:%s %s", 
                 wifiConnected ? "OK" : "--", 
                 ntpSynced ? "OK" : "--",
                 ampmStr);
        d->print(statusRow);
        
        d->setTextSize(3);
        int timeY = 20 + burnInY;
        int baseX = 19 + burnInX;
        
        d->setCursor(baseX, timeY);
        d->print(hourStr);
        
        if (blink) {
          d->setCursor(baseX + 36, timeY);
          d->print(":");
        }
        
        d->setCursor(baseX + 54, timeY);
        d->print(minStr);
        
        d->setTextSize(1);
        uint8_t dateLen = strlen(dateStr);
        uint8_t dateX = (128 - (dateLen * 6)) / 2;
        d->setCursor(dateX + burnInX, 54 + burnInY);
        d->print(dateStr);
        d->display();
      }
      break;
    }

    case CS_SHOW_ANIMATION: {
      if (now - animStartMs >= settings->cs.animationDurationMs) {
        csState = CS_SHOW_CLOCK;
        modeSwitchTime = now;
        clockStartMs = now;
        lastClockUpdate = 0;
        break;
      }
      
      // Auto cycle moods
      if (now - lastMoodChangeMs > 1000) {
        if (currentMood == MOOD_IDLE) {
          int r = esp_random() % 100;
          if (r < 10) currentMood = MOOD_BLINK;
          else if (r < 25) currentMood = MOOD_LOOK_LEFT;
          else if (r < 40) currentMood = MOOD_LOOK_RIGHT;
          else if (r < 55) currentMood = MOOD_HAPPY;
          else if (r < 65) currentMood = MOOD_SURPRISED;
          else if (now - lastActivityMs > 60000) currentMood = MOOD_SLEEPY;
        } else {
          currentMood = MOOD_IDLE;
        }
        lastMoodChangeMs = now;
      }
      
      updateRobotEyes(display, settings, currentMood, now);
      break;
    }

    case CS_DEEP_SLEEP: {
      Serial.println("[ClockSaver] Entering deep sleep");
      display->clear();
      display->getDriver()->display();
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      delay(100);
      esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_SEL_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
      esp_deep_sleep_start();
      break;
    }

    default:
      break;
  }
}
