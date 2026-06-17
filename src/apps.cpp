#include "apps.h"
#include "mainwindow.h"
#include "clock_saver.h"
#include <esp_chip_info.h>
#include <esp_system.h>

extern void sanitizeSSID(char* out, const uint8_t* raw, uint8_t rawLen, uint8_t outMax);

AppsUI::AppsUI(DisplayWrapper* disp, WifiScanner* ws, BleScanner* bs) 
  : display(disp), wifiScanner(ws), bleScanner(bs) {
  currentPage = AppsPage::Menu;
  menuSelectedIndex = 0;
  menuTopIndex = 0;
  
  swRunning = false;
  swStartTime = 0;
  swElapsed = 0;
  selectedDeviceIdx = 255;
  lastUptimeUpdate = 0;
}

void AppsUI::enter(AppsPage target) {
  currentPage = target;
  if (currentPage == AppsPage::Menu) {
    menuSelectedIndex = 0;
    menuTopIndex = 0;
    selectedDeviceIdx = 255;
    drawMenu();
  } else {
    if (currentPage == AppsPage::Stopwatch) {
      handleStopwatch(BTN_NONE);
    } else if (currentPage == AppsPage::BleInspector) {
      selectedDeviceIdx = 255;
      inspectPager.reset();
      handleBleInspector(BTN_NONE);
    } else if (currentPage == AppsPage::WiFiChannelOccupancy) {
      handleWiFiChannelOccupancy(BTN_NONE);
    } else if (currentPage == AppsPage::DeviceIdentity) {
      handleDeviceIdentity(BTN_NONE);
    } else if (currentPage == AppsPage::Uptime) {
      handleUptime(BTN_NONE);
    } else if (currentPage == AppsPage::SignalMeter) {
      handleSignalMeter(BTN_NONE);
    } else if (currentPage == AppsPage::ClockSaver) {
      handleClockSaver(BTN_NONE);
    }
  }
}

bool AppsUI::update(ButtonEvent btn) {
  if (currentPage == AppsPage::Menu) {
    bool redraw = false;
    if (btn == BTN_DOWN) {
      if (menuSelectedIndex < APPS_MENU_COUNT - 1) {
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
        case 0: currentPage = AppsPage::Stopwatch; handleStopwatch(BTN_NONE); break;
        case 1: 
          navStack.push(Page::AppsMenu);
          window.changePage(Page::BLEPacketInspector); 
          break;
        case 2: 
          navStack.push(Page::AppsMenu);
          window.changePage(Page::WiFiChannelOccupancy); 
          break;
        case 3: 
          navStack.push(Page::AppsMenu);
          window.changePage(Page::DeviceIdentity); 
          break;
        case 4: currentPage = AppsPage::Uptime; handleUptime(BTN_NONE); break;
        case 5: currentPage = AppsPage::SignalMeter; handleSignalMeter(BTN_NONE); break;
        case 6: 
          currentPage = AppsPage::ClockSaver; 
          startClockSaver(CS_ENTRY_MANUAL);
          handleClockSaver(BTN_NONE); 
          break;
      }
      return true;
    } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      return false; // Exit Apps
    }
    
    if (redraw) drawMenu();
    return true;
  }
  
  switch(currentPage) {
    case AppsPage::Stopwatch: handleStopwatch(btn); break;
    case AppsPage::BleInspector: handleBleInspector(btn); break;
    case AppsPage::WiFiChannelOccupancy: handleWiFiChannelOccupancy(btn); break;
    case AppsPage::DeviceIdentity: handleDeviceIdentity(btn); break;
    case AppsPage::Uptime: handleUptime(btn); break;
    case AppsPage::SignalMeter: handleSignalMeter(btn); break;
    case AppsPage::ClockSaver: handleClockSaver(btn); break;
    default: break;
  }
  
  return true;
}

void AppsUI::drawMenu() {
  display->drawMenu("Apps", appsMenuItems, APPS_MENU_COUNT, menuSelectedIndex, menuTopIndex);
}

void AppsUI::handleStopwatch(ButtonEvent btn) {
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = AppsPage::Menu;
    drawMenu();
    return;
  }
  
  if (btn == BTN_SELECT) {
    if (swRunning) {
      swElapsed += (millis() - swStartTime);
      swRunning = false;
    } else {
      swStartTime = millis();
      swRunning = true;
    }
  } else if (btn == BTN_SELECT_LONG) {
    swRunning = false;
    swElapsed = 0;
  }
  
  unsigned long currentElapsed = swElapsed;
  if (swRunning) {
    currentElapsed += (millis() - swStartTime);
  }
  
  display->clear();
  display->drawStatusBar("Stopwatch", RadioState::RADIO_OFF);
  auto d = display->getDriver();
  
  int ms = (currentElapsed) % 1000;
  int s = (currentElapsed / 1000) % 60;
  int m = (currentElapsed / 60000);
  
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d.%03d", m, s, ms);
  
  d->setTextSize(2);
  d->setCursor(5, 25);
  d->print(buf);
  d->setTextSize(1);
  d->setCursor(2, 52);
  d->print(F("SEL:Pause LONG:Rst"));
  d->display();
}

void AppsUI::handleBleInspector(ButtonEvent btn) {
  if (selectedDeviceIdx == 255) {
    if (bleScanner->deviceCount == 0) {
      display->clear();
      display->drawStatusBar("BLE Inspect", RadioState::RADIO_OFF);
      auto d = display->getDriver();
      d->setCursor(2, 25);
      d->print(F("No BLE devices saved."));
      d->setCursor(2, 35);
      d->print(F("Run BLE scan first."));
      d->display();
      
      if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
        currentPage = AppsPage::Menu;
        drawMenu();
      }
      return;
    }
    
    inspectPager.totalItems = bleScanner->deviceCount;
    inspectPager.itemsPerPage = 4;
    
    bool redraw = false;
    if (btn == BTN_DOWN) {
      inspectPager.moveDown();
      redraw = true;
    } else if (btn == BTN_UP) {
      inspectPager.moveUp();
      redraw = true;
    } else if (btn == BTN_DOWN_LONG) {
      if (inspectPager.hasNext()) {
        inspectPager.nextPage();
        inspectPager.selectedIdx = inspectPager.pageStart();
        redraw = true;
      }
    } else if (btn == BTN_UP_LONG) {
      if (inspectPager.hasPrev()) {
        inspectPager.prevPage();
        inspectPager.selectedIdx = inspectPager.pageStart();
        redraw = true;
      }
    } else if (btn == BTN_SELECT) {
      selectedDeviceIdx = inspectPager.selectedIdx;
      inspectPager.reset();
      redraw = true;
    } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      currentPage = AppsPage::Menu;
      drawMenu();
      return;
    }
    
    display->clear();
    display->drawStatusBar("Select Device", RadioState::RADIO_OFF);
    auto d = display->getDriver();
    
    uint8_t start = inspectPager.pageStart();
    uint8_t end = inspectPager.pageEnd();
    for (uint8_t i = start; i < end; i++) {
      uint8_t row = i - start;
      uint8_t y = 10 + (row * 10);
      if (i == inspectPager.selectedIdx) {
        d->fillRect(0, y, 128, 10, SSD1306_WHITE);
        d->setTextColor(SSD1306_BLACK);
      } else {
        d->setTextColor(SSD1306_WHITE);
      }
      d->setCursor(2, y + 1);
      d->print(bleScanner->devices[i].name);
    }
    d->setTextColor(SSD1306_WHITE);
    
    if (inspectPager.hasPrev()) { d->setCursor(2, 52); d->print(F("<")); }
    if (inspectPager.hasNext()) { d->setCursor(120, 52); d->print(F(">")); }
    char countBuf[10];
    snprintf(countBuf, sizeof(countBuf), "[%d/%d]", inspectPager.selectedIdx + 1, inspectPager.totalItems);
    d->setCursor(45, 52);
    d->print(countBuf);
    d->display();
  } else {
    BleDevice* dev = &bleScanner->devices[selectedDeviceIdx];
    uint8_t payloadLen = dev->advPayloadLen;
    uint8_t* payload = dev->advPayload;
    
    uint8_t totalLines = (payloadLen + 7) / 8;
    if (totalLines == 0) totalLines = 1;
    
    inspectPager.totalItems = totalLines;
    inspectPager.itemsPerPage = 4;
    
    bool redraw = false;
    if (btn == BTN_DOWN) {
      inspectPager.moveDown();
      redraw = true;
    } else if (btn == BTN_UP) {
      inspectPager.moveUp();
      redraw = true;
    } else if (btn == BTN_DOWN_LONG) {
      if (inspectPager.hasNext()) {
        inspectPager.nextPage();
        inspectPager.selectedIdx = inspectPager.pageStart();
        redraw = true;
      }
    } else if (btn == BTN_UP_LONG) {
      if (inspectPager.hasPrev()) {
        inspectPager.prevPage();
        inspectPager.selectedIdx = inspectPager.pageStart();
        redraw = true;
      }
    } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      selectedDeviceIdx = 255;
      inspectPager.reset();
      inspectPager.selectedIdx = 0;
      redraw = true;
      return;
    }
    
    display->clear();
    display->drawStatusBar("BLE Inspect", RadioState::RADIO_OFF);
    auto d = display->getDriver();
    
    uint8_t start = inspectPager.pageStart();
    uint8_t end = inspectPager.pageEnd();
    for (uint8_t i = start; i < end; i++) {
      uint8_t row = i - start;
      uint8_t y = 10 + (row * 10);
      
      d->setCursor(2, y + 1);
      d->setTextColor(SSD1306_WHITE);
      
      uint8_t startByte = i * 8;
      for (uint8_t b = 0; b < 8; b++) {
        uint8_t idx = startByte + b;
        if (idx < payloadLen) {
          d->printf("%02X ", payload[idx]);
        } else {
          d->print(F("   "));
        }
      }
    }
    
    d->setTextColor(SSD1306_WHITE);
    if (inspectPager.hasPrev()) { d->setCursor(2, 52); d->print(F("<")); }
    if (inspectPager.hasNext()) { d->setCursor(120, 52); d->print(F(">")); }
    char countBuf[20];
    snprintf(countBuf, sizeof(countBuf), "[%d/%d] Len:%d", inspectPager.selectedIdx + 1, totalLines, payloadLen);
    d->setCursor(35, 52);
    d->print(countBuf);
    d->display();
  }
}

void AppsUI::handleWiFiChannelOccupancy(ButtonEvent btn) {
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = AppsPage::Menu;
    drawMenu();
    return;
  }
  
  uint8_t chCounts[14] = {0};
  for (int i = 0; i < wifiScanner->networkCount; i++) {
    uint8_t ch = wifiScanner->networks[i].channel;
    if (ch >= 1 && ch <= 13) chCounts[ch]++;
  }
  
  uint8_t maxCount = 1;
  for (uint8_t ch = 1; ch <= 13; ch++) {
    if (chCounts[ch] > maxCount) maxCount = chCounts[ch];
  }
  
  uint8_t barMaxH = 34;  // available height for bars
  uint8_t barW    = 9;   // pixels per channel bar
  
  display->clear();
  display->drawStatusBar("CH Occupancy", RadioState::RADIO_OFF);
  auto d = display->getDriver();
  
  for (uint8_t ch = 1; ch <= 13; ch++) {
    uint8_t x = 4 + (ch - 1) * barW;
    uint8_t barH = (chCounts[ch] * barMaxH) / maxCount;
    uint8_t y = 14 + (barMaxH - barH);
    
    if (chCounts[ch] > 0) {
      d->fillRect(x, y, barW - 2, barH, SSD1306_WHITE);
    } else {
      d->drawPixel(x + 3, 48, SSD1306_WHITE);
    }
    
    d->setCursor(x + (ch >= 10 ? 0 : 2), 52);
    d->print(ch);
  }
  
  d->display();
}

void AppsUI::handleDeviceIdentity(ButtonEvent btn) {
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = AppsPage::Menu;
    drawMenu();
    return;
  }
  
  display->clear();
  display->drawStatusBar("Device Identity", RadioState::RADIO_OFF);
  auto d = display->getDriver();
  
  uint8_t wifiMac[6], bleMac[6];
  esp_read_mac(wifiMac, ESP_MAC_WIFI_STA);
  esp_read_mac(bleMac,  ESP_MAC_BT);
  
  esp_chip_info_t info;
  esp_chip_info(&info);
  
  d->setCursor(2, 12);
  d->printf("WiFi MAC:%02X:%02X:%02X..", wifiMac[0], wifiMac[1], wifiMac[2]);
  
  d->setCursor(2, 22);
  d->printf("BLE  MAC:%02X:%02X:%02X..", bleMac[0], bleMac[1], bleMac[2]);
  
  d->setCursor(2, 32);
  d->printf("Chip: ESP32-C3 rev%d", info.revision);
  
  d->setCursor(2, 42);
  d->printf("Cores: %d  Speed: %dMHz", info.cores, ESP.getCpuFreqMHz());
  
  uint32_t flash_size = ESP.getFlashChipSize() / (1024 * 1024);
  d->setCursor(2, 52);
  d->printf("Flash: %uMB  SDK: %s", flash_size, ESP.getSdkVersion());
  
  d->display();
}

void AppsUI::handleUptime(ButtonEvent btn) {
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = AppsPage::Menu;
    drawMenu();
    return;
  }
  
  if (millis() - lastUptimeUpdate > 250 || btn != BTN_NONE) {
    lastUptimeUpdate = millis();
    display->clear();
    display->drawStatusBar("Uptime Monitor", RadioState::RADIO_OFF);
    auto d = display->getDriver();
    
    unsigned long secs = millis() / 1000;
    int h = secs / 3600;
    int m = (secs % 3600) / 60;
    int s = secs % 60;
    
    d->setCursor(2, 16);
    d->printf("Up: %02d:%02d:%02d", h, m, s);
    d->setCursor(2, 28);
    d->printf("Heap: %u B", ESP.getFreeHeap());
    d->setCursor(2, 40);
    d->printf("CPU: %d MHz", ESP.getCpuFreqMHz());
    
    d->setCursor(2, 52);
    extern float safeTemperatureRead();
    float temp = safeTemperatureRead();
    if (temp > -100.0f) {
      d->printf("Chip Temp: %.1f C", temp);
    } else {
      d->print(F("Chip Temp: Unsupported"));
    }
    
    d->display();
  }
}

void AppsUI::handleSignalMeter(ButtonEvent btn) {
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = AppsPage::Menu;
    drawMenu();
    return;
  }
  
  display->clear();
  display->drawStatusBar("Signal Meter", RadioState::RADIO_OFF);
  auto d = display->getDriver();
  
  if (wifiScanner->networkCount == 0) {
    d->setCursor(2, 30);
    d->print(F("Run Wi-Fi scan first."));
  } else {
    WifiNetwork* net = &wifiScanner->networks[0];
    char ssid[15];
    sanitizeSSID(ssid, (const uint8_t*)net->ssid, strlen(net->ssid), sizeof(ssid));
    
    d->setCursor(2, 20);
    d->print(ssid);
    
    d->setTextSize(2);
    d->setCursor(2, 35);
    d->printf("%d dBm", net->rssi);
    d->setTextSize(1);
  }
  d->display();
}

void AppsUI::handleClockSaver(ButtonEvent btn) {
  extern ClockSaverEntryMode currentClockSaverMode;
  if (currentClockSaverMode == CS_ENTRY_GLOBAL_IDLE && btn != BTN_NONE) {
    extern NavStack navStack;
    extern MainWindow window;
    window.changePage(navStack.pop());
    return;
  }
  
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = AppsPage::Menu;
    initClockSaver();
    drawMenu();
    return;
  }
  handleClockSaverApp(display, window.getSettings(), btn);
}
