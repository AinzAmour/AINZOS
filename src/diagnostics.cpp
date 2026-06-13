#include "diagnostics.h"
#include "i2c_names.h"
#include <esp_system.h>
#include <esp_wifi.h>
#include <Wire.h>

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 2
  float safeTemperatureRead() { return temperatureRead(); }
#else
  float safeTemperatureRead() { return -999.0f; }
#endif

Diagnostics::Diagnostics(DisplayWrapper* disp) : display(disp) {
  currentPage = DiagPage::Menu;
  menuSelectedIndex = 0;
  menuTopIndex = 0;
  lastRefreshTime = 0;
}

void Diagnostics::enter() {
  currentPage = DiagPage::Menu;
  menuSelectedIndex = 0;
  menuTopIndex = 0;
  drawMenu();
}

bool Diagnostics::update(ButtonEvent btn) {
  if (currentPage == DiagPage::Menu) {
    bool redraw = false;
    if (btn == BTN_DOWN) {
      if (menuSelectedIndex < DIAG_MENU_COUNT - 1) {
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
      switch (menuSelectedIndex) {
        case 0: currentPage = DiagPage::I2CScanner; runI2CScanner(); break;
        case 1: currentPage = DiagPage::ButtonTest; drawButtonTest(BTN_NONE); break;
        case 2: 
          currentPage = DiagPage::SysMonitor; 
          sysMonitorPager.reset();
          lastRefreshTime = 0; 
          drawSysMonitor();
          break;
        case 3: currentPage = DiagPage::OledTest; drawOledTest(); break;
        case 4: currentPage = DiagPage::ResetReason; drawResetReason(); break;
      }
      return true;
    } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      return false; // Exit diagnostics
    }
    
    if (redraw) drawMenu();
    return true;
  }
  
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = DiagPage::Menu;
    drawMenu();
    return true;
  }
  
  if (currentPage == DiagPage::ButtonTest) {
    drawButtonTest(btn);
  } else if (currentPage == DiagPage::SysMonitor) {
    handleSysMonitor(btn);
  }
  return true;
}

void Diagnostics::drawMenu() {
  display->drawMenu("Diagnostics", diagMenuItems, DIAG_MENU_COUNT, menuSelectedIndex, menuTopIndex);
}

void Diagnostics::runI2CScanner() {
  display->clear();
  display->drawStatusBar("I2C Scanner", RadioState::RADIO_OFF);
  display->getDriver()->setCursor(2, 12);
  display->getDriver()->print(F("Scanning..."));
  display->getDriver()->display();
  
  Serial.println("--- I2C Scan ---");
  int foundCount = 0;
  String results = "";
  
  for(byte address = 0x08; address <= 0x77; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      foundCount++;
      Serial.printf("Found device at 0x%02X\n", address);
      const char* name = i2cDeviceName(address);
      char buf[40];
      if (name) {
        snprintf(buf, sizeof(buf), "0x%02X: %s [OK]\n", address, name);
      } else {
        snprintf(buf, sizeof(buf), "0x%02X: Found\n", address);
      }
      results += buf;
    }
  }
  
  display->clear();
  display->drawStatusBar("I2C Scanner", RadioState::RADIO_OFF);
  display->getDriver()->setCursor(2, 12);
  if (foundCount == 0) {
    display->getDriver()->print(F("No I2C devices found"));
    Serial.println("No I2C devices found.");
  } else {
    display->getDriver()->print(results);
  }
  display->getDriver()->display();
}

void Diagnostics::drawButtonTest(ButtonEvent btn) {
  display->clear();
  display->drawStatusBar("Button Test", RadioState::RADIO_OFF);
  
  auto d = display->getDriver();
  d->setCursor(2, 12);
  d->printf("UP (0): %d\n", digitalRead(0) == LOW);
  d->setCursor(2, 22);
  d->printf("DOWN (1): %d\n", digitalRead(1) == LOW);
  d->setCursor(64, 12);
  d->printf("SEL (3): %d\n", digitalRead(3) == LOW);
  d->setCursor(64, 22);
  d->printf("BACK(10): %d\n", digitalRead(10) == LOW);
  
  d->setCursor(2, 40);
  d->print(F("Last Event: "));
  switch(btn) {
    case BTN_UP: d->print(F("UP")); break;
    case BTN_DOWN: d->print(F("DOWN")); break;
    case BTN_SELECT: d->print(F("SELECT")); break;
    case BTN_BACK: d->print(F("BACK")); break;
    case BTN_UP_LONG: d->print(F("UP_LONG")); break;
    case BTN_DOWN_LONG: d->print(F("DOWN_LONG")); break;
    case BTN_SELECT_LONG: d->print(F("SELECT_LONG")); break;
    case BTN_BACK_LONG: d->print(F("BACK_LONG")); break;
    case BTN_NONE: d->print(F("NONE")); break;
  }
  d->display();
}

void Diagnostics::handleSysMonitor(ButtonEvent btn) {
  bool redraw = false;
  if (btn == BTN_DOWN) {
    sysMonitorPager.moveDown();
    redraw = true;
  } else if (btn == BTN_UP) {
    sysMonitorPager.moveUp();
    redraw = true;
  }
  
  if (redraw || millis() - lastRefreshTime > 1000 || lastRefreshTime == 0) {
    lastRefreshTime = millis();
    drawSysMonitor();
  }
}

void Diagnostics::drawSysMonitor() {
  display->clear();
  display->drawStatusBar("Sys Monitor", RadioState::RADIO_OFF);
  
  auto d = display->getDriver();
  
  sysMonitorPager.totalItems = 10;
  sysMonitorPager.itemsPerPage = 5;
  
  uint8_t start = sysMonitorPager.pageStart();
  uint8_t end = sysMonitorPager.pageEnd();
  
  char lines[10][32];
  
  // Line 0: Uptime
  unsigned long secs = millis() / 1000;
  snprintf(lines[0], sizeof(lines[0]), "Uptime: %lu s", secs);
  
  // Line 1: Free Heap
  snprintf(lines[1], sizeof(lines[1]), "FreeHeap: %u B", ESP.getFreeHeap());
  
  // Line 2: Min Free Heap
  snprintf(lines[2], sizeof(lines[2]), "MinHeap: %u B", ESP.getMinFreeHeap());
  
  // Line 3: Sketch Size
  snprintf(lines[3], sizeof(lines[3]), "Sketch: %u B", ESP.getSketchSize());
  
  // Line 4: Free Flash
  snprintf(lines[4], sizeof(lines[4]), "FreeFlash: %u B", ESP.getFreeSketchSpace());
  
  // Line 5: CPU Speed
  snprintf(lines[5], sizeof(lines[5]), "CPU: %u MHz", ESP.getCpuFreqMHz());
  
  // Line 6: Chip Temperature
  float temp = safeTemperatureRead();
  if (temp > -100.0f) {
    snprintf(lines[6], sizeof(lines[6]), "Temp: %.1f C", temp);
  } else {
    snprintf(lines[6], sizeof(lines[6]), "Temp: Unsupported");
  }
  
  // Line 7: WiFi MAC
  uint8_t wifiMac[6];
  esp_read_mac(wifiMac, ESP_MAC_WIFI_STA);
  snprintf(lines[7], sizeof(lines[7]), "WiFi:%02X:%02X:%02X:%02X:%02X:%02X", 
           wifiMac[0], wifiMac[1], wifiMac[2], wifiMac[3], wifiMac[4], wifiMac[5]);
  
  // Line 8: BLE MAC
  uint8_t bleMac[6];
  esp_read_mac(bleMac, ESP_MAC_BT);
  snprintf(lines[8], sizeof(lines[8]), "BLE :%02X:%02X:%02X:%02X:%02X:%02X", 
           bleMac[0], bleMac[1], bleMac[2], bleMac[3], bleMac[4], bleMac[5]);
  
  // Line 9: Reset Reason
  esp_reset_reason_t reason = esp_reset_reason();
  const char* rStr = "Unknown";
  switch(reason) {
    case ESP_RST_POWERON:   rStr = "Power On"; break;
    case ESP_RST_EXT:       rStr = "Ext Pin"; break;
    case ESP_RST_SW:        rStr = "Software"; break;
    case ESP_RST_PANIC:     rStr = "Panic/Crash"; break;
    case ESP_RST_INT_WDT:   rStr = "Int WDT"; break;
    case ESP_RST_TASK_WDT:  rStr = "Task WDT"; break;
    case ESP_RST_WDT:       rStr = "Other WDT"; break;
    case ESP_RST_DEEPSLEEP: rStr = "DeepSleep"; break;
    case ESP_RST_BROWNOUT:  rStr = "Brownout"; break;
    default: break;
  }
  snprintf(lines[9], sizeof(lines[9]), "Reset: %s", rStr);
  
  for (uint8_t i = start; i < end; i++) {
    uint8_t row = i - start;
    uint8_t y = 10 + (row * 8);
    
    if (i == sysMonitorPager.selectedIdx) {
      d->fillRect(0, y, 128, 8, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    
    d->setCursor(2, y);
    d->print(lines[i]);
  }
  
  d->setTextColor(SSD1306_WHITE);
  if (sysMonitorPager.hasPrev()) {
    d->setCursor(2, 53);
    d->print(F("<"));
  }
  if (sysMonitorPager.hasNext()) {
    d->setCursor(120, 53);
    d->print(F(">"));
  }
  
  char countBuf[12];
  snprintf(countBuf, sizeof(countBuf), "[%d/%d]", sysMonitorPager.selectedIdx + 1, sysMonitorPager.totalItems);
  d->setCursor(45, 53);
  d->print(countBuf);
  
  d->display();
}

void Diagnostics::drawOledTest() {
  display->clear();
  auto d = display->getDriver();
  d->drawRect(0, 0, 128, 64, SSD1306_WHITE);
  d->drawLine(0, 0, 128, 64, SSD1306_WHITE);
  d->drawLine(128, 0, 0, 64, SSD1306_WHITE);
  d->setCursor(10, 10);
  d->setTextSize(1);
  d->print(F("OLED TEST"));
  d->setCursor(10, 25);
  d->setTextSize(2);
  d->print(F("BIG TXT"));
  d->setTextSize(1);
  d->display();
}

void Diagnostics::drawResetReason() {
  display->clear();
  display->drawStatusBar("Reset Reason", RadioState::RADIO_OFF);
  
  auto d = display->getDriver();
  d->setCursor(2, 20);
  esp_reset_reason_t reason = esp_reset_reason();
  switch(reason) {
    case ESP_RST_POWERON: d->print(F("Power On")); break;
    case ESP_RST_EXT: d->print(F("External Reset")); break;
    case ESP_RST_SW: d->print(F("Software Reset")); break;
    case ESP_RST_PANIC: d->print(F("Panic / Crash")); break;
    case ESP_RST_INT_WDT: d->print(F("Interrupt WDT")); break;
    case ESP_RST_TASK_WDT: d->print(F("Task WDT")); break;
    case ESP_RST_WDT: d->print(F("Other WDT")); break;
    case ESP_RST_DEEPSLEEP: d->print(F("Deep Sleep Exit")); break;
    case ESP_RST_BROWNOUT: d->print(F("Brownout")); break;
    default: d->print(F("Unknown")); break;
  }
  
  d->display();
}
