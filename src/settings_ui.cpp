#include "settings_ui.h"

SettingsUI::SettingsUI(DisplayWrapper* disp, SettingsManager* sm) : display(disp), settings(sm) {
  currentPage = SetPage::Menu;
  menuSelectedIndex = 0;
  menuTopIndex = 0;
}

void SettingsUI::enter() {
  currentPage = SetPage::Menu;
  menuSelectedIndex = 0;
  menuTopIndex = 0;
  drawMenu();
}

bool SettingsUI::update(ButtonEvent btn) {
  if (currentPage == SetPage::ClockSaverMenu) {
    bool redraw = false;
    static const int CS_SET_COUNT = 14;
    if (btn == BTN_DOWN) {
      if (menuSelectedIndex < CS_SET_COUNT - 1) {
        menuSelectedIndex++;
        if (menuSelectedIndex >= menuTopIndex + 5) menuTopIndex++;
        redraw = true;
      }
    } else if (btn == BTN_UP) {
      if (menuSelectedIndex > 0) {
        menuSelectedIndex--;
        if (menuSelectedIndex < menuTopIndex) menuTopIndex--;
        redraw = true;
      }
    } else if (btn == BTN_SELECT) {
      handleClockSaverSelection();
      settings->save();
      redraw = true;
    } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      settings->save();
      currentPage = SetPage::Menu;
      menuSelectedIndex = 10; // select Clock Saver in parent menu
      menuTopIndex = 6;
      redraw = true;
    }
    
    if (redraw) drawClockSaverMenu();
    return true;
  }

  if (currentPage == SetPage::Menu) {
    bool redraw = false;
    if (btn == BTN_DOWN) {
      if (menuSelectedIndex < SET_MENU_COUNT - 1) {
        menuSelectedIndex++;
        if (menuSelectedIndex >= menuTopIndex + 5) menuTopIndex++;
        redraw = true;
      }
    } else if (btn == BTN_UP) {
      if (menuSelectedIndex > 0) {
        menuSelectedIndex--;
        if (menuSelectedIndex < menuTopIndex) menuTopIndex--;
        redraw = true;
      }
    } else if (btn == BTN_SELECT) {
      handleSelection();
      settings->save(); // Save to NVS on change
      return true;
    } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      settings->save();
      return false; // Exit settings
    }
    
    if (redraw) drawMenu();
    return true;
  }
  
  if (currentPage == SetPage::ConfirmReset) {
    if (btn == BTN_SELECT || btn == BTN_SELECT_LONG) {
      settings->factoryReset();
      currentPage = SetPage::Menu;
      menuSelectedIndex = 0;
      menuTopIndex = 0;
      drawMenu();
    } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      currentPage = SetPage::Menu;
      drawMenu();
    }
    return true;
  }
  
  return true;
}

void SettingsUI::handleSelection() {
  switch (menuSelectedIndex) {
    case 0: // Contrast
      if (settings->contrast < 63) settings->contrast = 63;
      else if (settings->contrast < 127) settings->contrast = 127;
      else if (settings->contrast < 191) settings->contrast = 191;
      else if (settings->contrast < 255) settings->contrast = 255;
      else settings->contrast = 1;
      display->setContrast(settings->contrast);
      break;
    case 1: // Animation
      settings->anim_speed = (settings->anim_speed + 1) % 3;
      break;
    case 2: // Wi-Fi Region
      settings->wifi_region = (settings->wifi_region + 1) % 3;
      break;
    case 3: // BLE Scan Duration
      if (settings->ble_duration == 3) settings->ble_duration = 5;
      else if (settings->ble_duration == 5) settings->ble_duration = 10;
      else if (settings->ble_duration == 10) settings->ble_duration = 15;
      else if (settings->ble_duration == 15) settings->ble_duration = 3;
      break;
    case 4: // Auto-Dim (merged dim_time)
      if (settings->dim_time == 0) settings->dim_time = 15;
      else if (settings->dim_time == 15) settings->dim_time = 30;
      else if (settings->dim_time == 30) settings->dim_time = 60;
      else if (settings->dim_time == 60) settings->dim_time = 0;
      break;
    case 5: // Boot Logo
      settings->boot_logo = !settings->boot_logo;
      break;
    case 6: // Scroll Speed
      settings->scroll_speed = (settings->scroll_speed + 1) % 3;
      break;
    case 7: // Probe Hop Speed
      if (settings->probe_hop == 50) settings->probe_hop = 100;
      else if (settings->probe_hop == 100) settings->probe_hop = 200;
      else if (settings->probe_hop == 200) settings->probe_hop = 500;
      else if (settings->probe_hop == 500) settings->probe_hop = 50;
      break;
    case 8: // BLE Filter
      settings->ble_filter = (settings->ble_filter + 1) % 4;
      break;
    case 9: // Temp Unit
      settings->temp_unit = (settings->temp_unit + 1) % 2;
      break;
    case 10: // Clock Saver settings sub-menu
      currentPage = SetPage::ClockSaverMenu;
      menuSelectedIndex = 0;
      menuTopIndex = 0;
      drawClockSaverMenu();
      return;
    case 11: // Factory Reset
      currentPage = SetPage::ConfirmReset;
      drawConfirm();
      return;
  }
  drawMenu();
}

void SettingsUI::drawMenu() {
  display->clear();
  display->drawStatusBar("Settings", RadioState::RADIO_OFF);
  
  auto d = display->getDriver();
  
  char items[12][32];
  snprintf(items[0], sizeof(items[0]), "Contrast: %d", settings->contrast);
  
  const char* anim = "Normal";
  if (settings->anim_speed == 0) anim = "Slow";
  else if (settings->anim_speed == 2) anim = "Fast";
  snprintf(items[1], sizeof(items[1]), "Anim: %s", anim);
  
  const char* reg = "IN (1-13)";
  if (settings->wifi_region == 1) reg = "US (1-11)";
  else if (settings->wifi_region == 2) reg = "JP (1-14)";
  snprintf(items[2], sizeof(items[2]), "Region: %s", reg);
  
  snprintf(items[3], sizeof(items[3]), "BLE Scan: %ds", settings->ble_duration);
  
  if (settings->dim_time == 0) {
    snprintf(items[4], sizeof(items[4]), "Auto-Dim: Off");
  } else {
    snprintf(items[4], sizeof(items[4]), "Auto-Dim: %ds", settings->dim_time);
  }
  
  snprintf(items[5], sizeof(items[5]), "Boot Logo: %s", settings->boot_logo ? "ON" : "OFF");
  
  const char* scr = "Med";
  if (settings->scroll_speed == 0) scr = "Slow";
  else if (settings->scroll_speed == 2) scr = "Fast";
  snprintf(items[6], sizeof(items[6]), "Scroll Spd: %s", scr);
  
  snprintf(items[7], sizeof(items[7]), "Hop Speed: %dms", settings->probe_hop);
  
  const char* flt = "All";
  if (settings->ble_filter == 1) flt = "Trackers";
  else if (settings->ble_filter == 2) flt = "UART";
  else if (settings->ble_filter == 3) flt = "Unknown";
  snprintf(items[8], sizeof(items[8]), "BLE Filter: %s", flt);
  
  snprintf(items[9], sizeof(items[9]), "Temp Unit: %s", settings->temp_unit == 0 ? "C" : "F");
  snprintf(items[10], sizeof(items[10]), "Clock Saver Settings");
  snprintf(items[11], sizeof(items[11]), "Factory Reset");

  for (int i = 0; i < 5; i++) {
    int itemIdx = menuTopIndex + i;
    if (itemIdx >= SET_MENU_COUNT) break;
    
    int y = 10 + (i * 10);
    if (itemIdx == menuSelectedIndex) {
      d->fillRect(0, y, 128, 10, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    d->setCursor(2, y + 1);
    d->print(items[itemIdx]);
  }
  
  d->setTextColor(SSD1306_WHITE);
  d->display();
}

void SettingsUI::drawClockSaverMenu() {
  display->clear();
  display->drawStatusBar("Clock Saver", RadioState::RADIO_OFF);
  
  auto d = display->getDriver();
  
  char items[14][32];
  snprintf(items[0], sizeof(items[0]), "Clock Saver: %s", settings->cs.enabled ? "ON" : "OFF");
  snprintf(items[1], sizeof(items[1]), "Boot Clock: %s", settings->cs.bootClockEnabled ? "ON" : "OFF");
  snprintf(items[2], sizeof(items[2]), "Auto Idle: %s", settings->cs.autoStartOnIdle ? "ON" : "OFF");
  
  uint32_t gIdleMin = settings->cs.globalIdleTimeoutMs / 60000;
  if (settings->cs.globalIdleTimeoutMs == 30000) snprintf(items[3], sizeof(items[3]), "Glbl Idle: 30s");
  else snprintf(items[3], sizeof(items[3]), "Glbl Idle: %dm", gIdleMin);
  
  snprintf(items[4], sizeof(items[4]), "WiFi Clock: %s", settings->cs.wifiClock ? "ON" : "OFF");
  snprintf(items[5], sizeof(items[5]), "NTP Sync: %s", settings->cs.ntpSync ? "ON" : "OFF");
  snprintf(items[6], sizeof(items[6]), "Robot Eyes: %s", settings->cs.animationEnabled ? "ON" : "OFF");
  snprintf(items[7], sizeof(items[7]), "Deep Sleep: %s", settings->cs.deepSleepEnabled ? "ON" : "OFF");
  
  snprintf(items[8], sizeof(items[8]), "Clock Time: %ds", settings->cs.clockDurationMs / 1000);
  snprintf(items[9], sizeof(items[9]), "Robot Time: %ds", settings->cs.animationDurationMs / 1000);
  snprintf(items[10], sizeof(items[10]), "Idle TO: %ds", settings->cs.idleAnimationTimeoutMs / 1000);
  
  uint32_t toMin = settings->cs.sleepTimeoutMs / 60000;
  snprintf(items[11], sizeof(items[11]), "Sleep Time: %dm", toMin);
  
  const char* spdName = "Norm";
  if (settings->cs.animationSpeedMs <= 45) spdName = "Fast";
  else if (settings->cs.animationSpeedMs <= 75) spdName = "Norm";
  else if (settings->cs.animationSpeedMs <= 90) spdName = "Slow";
  else spdName = "Chill";
  snprintf(items[12], sizeof(items[12]), "Anim Speed: %s", spdName);
  
  snprintf(items[13], sizeof(items[13]), "Time Format: %s", settings->cs.use24h ? "24h" : "12h");

  for (int i = 0; i < 5; i++) {
    int itemIdx = menuTopIndex + i;
    if (itemIdx >= 14) break;
    
    int y = 10 + (i * 10);
    if (itemIdx == menuSelectedIndex) {
      d->fillRect(0, y, 128, 10, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    d->setCursor(2, y + 1);
    d->print(items[itemIdx]);
  }
  
  d->setTextColor(SSD1306_WHITE);
  d->display();
}

void SettingsUI::handleClockSaverSelection() {
  switch (menuSelectedIndex) {
    case 0: settings->cs.enabled = !settings->cs.enabled; break;
    case 1: settings->cs.bootClockEnabled = !settings->cs.bootClockEnabled; break;
    case 2: settings->cs.autoStartOnIdle = !settings->cs.autoStartOnIdle; break;
    case 3:
      if (settings->cs.globalIdleTimeoutMs == 30000) settings->cs.globalIdleTimeoutMs = 60000;
      else if (settings->cs.globalIdleTimeoutMs == 60000) settings->cs.globalIdleTimeoutMs = 120000;
      else if (settings->cs.globalIdleTimeoutMs == 120000) settings->cs.globalIdleTimeoutMs = 300000;
      else settings->cs.globalIdleTimeoutMs = 30000;
      break;
    case 4: settings->cs.wifiClock = !settings->cs.wifiClock; break;
    case 5: settings->cs.ntpSync = !settings->cs.ntpSync; break;
    case 6: settings->cs.animationEnabled = !settings->cs.animationEnabled; break;
    case 7: settings->cs.deepSleepEnabled = !settings->cs.deepSleepEnabled; break;
    case 8:
      if (settings->cs.clockDurationMs == 8000) settings->cs.clockDurationMs = 15000;
      else if (settings->cs.clockDurationMs == 15000) settings->cs.clockDurationMs = 30000;
      else settings->cs.clockDurationMs = 8000;
      break;
    case 9:
      if (settings->cs.animationDurationMs == 8000) settings->cs.animationDurationMs = 12000;
      else if (settings->cs.animationDurationMs == 12000) settings->cs.animationDurationMs = 20000;
      else settings->cs.animationDurationMs = 8000;
      break;
    case 10:
      if (settings->cs.idleAnimationTimeoutMs == 15000) settings->cs.idleAnimationTimeoutMs = 30000;
      else if (settings->cs.idleAnimationTimeoutMs == 30000) settings->cs.idleAnimationTimeoutMs = 60000;
      else if (settings->cs.idleAnimationTimeoutMs == 60000) settings->cs.idleAnimationTimeoutMs = 120000;
      else if (settings->cs.idleAnimationTimeoutMs == 120000) settings->cs.idleAnimationTimeoutMs = 300000;
      else settings->cs.idleAnimationTimeoutMs = 15000;
      break;
    case 11:
      if (settings->cs.sleepTimeoutMs == 300000) settings->cs.sleepTimeoutMs = 600000;
      else if (settings->cs.sleepTimeoutMs == 600000) settings->cs.sleepTimeoutMs = 1800000; // 30m? Or off
      else settings->cs.sleepTimeoutMs = 300000;
      break;
    case 12:
      if (settings->cs.animationSpeedMs == 45) settings->cs.animationSpeedMs = 75;
      else if (settings->cs.animationSpeedMs == 75) settings->cs.animationSpeedMs = 90;
      else if (settings->cs.animationSpeedMs == 90) settings->cs.animationSpeedMs = 120;
      else settings->cs.animationSpeedMs = 45;
      break;
    case 13: settings->cs.use24h = !settings->cs.use24h; break;
  }
}

void SettingsUI::drawConfirm() {
  display->clear();
  display->drawStatusBar("Factory Reset?", RadioState::RADIO_OFF);
  auto d = display->getDriver();
  d->setCursor(2, 20);
  d->print(F("SELECT to Reset"));
  d->setCursor(2, 40);
  d->print(F("BACK to Cancel"));
  d->display();
}
