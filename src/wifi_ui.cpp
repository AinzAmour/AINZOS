#include "wifi_ui.h"
#include "mainwindow.h"
#include "oui_lite.h"

extern void sanitizeSSID(char* out, const uint8_t* raw, uint8_t rawLen, uint8_t outMax);

WifiUI::WifiUI(DisplayWrapper* disp) : display(disp) {
  currentPage = WifiPage::Menu;
  menuSelectedIndex = 0;
  menuTopIndex = 0;
}

void WifiUI::begin() {
  scanner.begin();
}

void WifiUI::enter(WifiPage target) {
  currentPage = target;
  if (currentPage == WifiPage::Menu) {
    menuSelectedIndex = 0;
    menuTopIndex = 0;
    drawMenu();
  } else if (currentPage == WifiPage::ResultsList) {
    // Keep pager selected index and redraw
    drawResultsList();
  } else if (currentPage == WifiPage::Details) {
    drawDetails();
  } else if (currentPage == WifiPage::ProbeMonitor) {
    drawProbeMonitor();
  } else if (currentPage == WifiPage::ChannelAnalyzer) {
    drawChannelAnalyzer();
  }
}

bool WifiUI::update(ButtonEvent btn) {
  if (currentPage == WifiPage::Menu) {
    bool redraw = false;
    if (btn == BTN_DOWN) {
      if (menuSelectedIndex < WIFI_MENU_COUNT - 1) {
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
        case 0: // Scan
          if (scanner.startScanAsync()) {
            currentPage = WifiPage::Scanning;
            scanAnimTime = millis();
            scanAnimFrame = 0;
            drawScanning();
          }
          break;
        case 1: // Saved Results
          currentPage = WifiPage::ResultsList;
          resultsPager.reset();
          resultsPager.totalItems = scanner.networkCount;
          resultsPager.itemsPerPage = 4;
          drawResultsList();
          break;
        case 2: // Probe Monitor
          navStack.push(Page::WiFiMenu);
          startProbeMonitor();
          probePager.reset();
          window.changePage(Page::WiFiProbeMonitor);
          break;
        case 3: // Channel Analyzer
          currentPage = WifiPage::ChannelAnalyzer;
          drawChannelAnalyzer();
          break;
        case 4: // Open Networks
          currentPage = WifiPage::OpenNetworks;
          drawOpenNetworks();
          break;
        case 5: // Strongest Network
          currentPage = WifiPage::StrongestNetwork;
          drawStrongestNetwork();
          break;
        case 6: // Device Info
          currentPage = WifiPage::DeviceInfo;
          drawDeviceInfo();
          break;
        case 7: // Clear Results
          scanner.clearResults();
          display->clear();
          display->getDriver()->setCursor(10, 30);
          display->getDriver()->print(F("Results Cleared!"));
          display->getDriver()->display();
          delay(800); // Safe hold to show toast, then redraw
          drawMenu();
          break;
      }
      return true;
    } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      return false; // Exit WiFi tools
    }
    
    if (redraw) drawMenu();
    return true;
  }
  
  if (currentPage == WifiPage::Scanning) {
    if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      scanner.cancelScan();
      currentPage = WifiPage::Menu;
      drawMenu();
      return true;
    }
    
    ScanState status = scanner.checkScanStatus();
    if (status == ScanState::Running || status == ScanState::Starting) {
      if (millis() - scanAnimTime > 300) {
        scanAnimTime = millis();
        scanAnimFrame = (scanAnimFrame + 1) % 4;
        drawScanning();
      }
    } else if (status == ScanState::Failed) {
      display->clear();
      display->getDriver()->setCursor(10, 30);
      display->getDriver()->print(F("Scan Failed!"));
      display->getDriver()->display();
      delay(1000);
      scanner.currentState = ScanState::Idle;
      scanner.lastScanEndTime = millis();
      currentPage = WifiPage::Menu;
      drawMenu();
    } else if (status == ScanState::Complete) {
      scanner.storeResults();
      currentPage = WifiPage::ResultsList;
      resultsPager.reset();
      resultsPager.totalItems = scanner.networkCount;
      resultsPager.itemsPerPage = 4;
      drawResultsList();
    }
    return true;
  }
  
  if (currentPage == WifiPage::ResultsList) {
    if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      currentPage = WifiPage::Menu;
      drawMenu();
      return true;
    }
    handleResultsList(btn);
    return true;
  }
  
  if (currentPage == WifiPage::ProbeMonitor) {
    // Fallback: MainWindow handles BACK, but we handle scrolling/drawing here
    handleProbeMonitor(btn);
    return true;
  }
  
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = WifiPage::Menu;
    drawMenu();
  }
  
  return true;
}

void WifiUI::drawMenu() {
  display->drawMenu("WiFi Tools", wifiMenuItems, WIFI_MENU_COUNT, menuSelectedIndex, menuTopIndex);
}

void WifiUI::drawScanning() {
  display->clear();
  display->drawStatusBar("Scanning", RadioState::RADIO_WIFI);
  auto d = display->getDriver();
  d->setCursor(20, 30);
  d->print(F("Scanning"));
  for (int i = 0; i < scanAnimFrame; i++) d->print(F("."));
  d->display();
}

void WifiUI::handleResultsList(ButtonEvent btn) {
  if (scanner.networkCount == 0) return;
  
  bool redraw = false;
  if (btn == BTN_DOWN) {
    resultsPager.moveDown();
    redraw = true;
  } else if (btn == BTN_UP) {
    resultsPager.moveUp();
    redraw = true;
  } else if (btn == BTN_DOWN_LONG) {
    if (resultsPager.hasNext()) {
      resultsPager.nextPage();
      resultsPager.selectedIdx = resultsPager.pageStart();
      redraw = true;
    }
  } else if (btn == BTN_UP_LONG) {
    if (resultsPager.hasPrev()) {
      resultsPager.prevPage();
      resultsPager.selectedIdx = resultsPager.pageStart();
      redraw = true;
    }
  } else if (btn == BTN_SELECT) {
    navStack.push(Page::WiFiMenu);
    window.changePage(Page::WiFiDetails);
    return;
  }
  
  if (redraw) drawResultsList();
}

void WifiUI::handleProbeMonitor(ButtonEvent btn) {
  bool redraw = false;
  if (btn == BTN_DOWN) {
    probePager.moveDown();
    redraw = true;
  } else if (btn == BTN_UP) {
    probePager.moveUp();
    redraw = true;
  } else if (btn == BTN_DOWN_LONG) {
    if (probePager.hasNext()) {
      probePager.nextPage();
      probePager.selectedIdx = probePager.pageStart();
      redraw = true;
    }
  } else if (btn == BTN_UP_LONG) {
    if (probePager.hasPrev()) {
      probePager.prevPage();
      probePager.selectedIdx = probePager.pageStart();
      redraw = true;
    }
  }
  
  // Always redraw to update elapsed timers/counts
  drawProbeMonitor();
}

const char* WifiUI::getEncBadge(uint8_t enc) {
  switch (enc) {
    case WIFI_AUTH_OPEN:          return "[O]";
    case WIFI_AUTH_WEP:           return "[W]";
    case WIFI_AUTH_WPA2_PSK:      return "[2]";
    case WIFI_AUTH_WPA3_PSK:      return "[3]";
    case WIFI_AUTH_WPA_WPA2_PSK:  return "[M]";
    default:                      return "[?]";
  }
}

void WifiUI::drawResultsList() {
  display->clear();
  display->drawStatusBar("Scan Results", RadioState::RADIO_WIFI);
  
  auto d = display->getDriver();
  if (scanner.networkCount == 0) {
    d->setCursor(2, 30);
    d->print(F("No networks found."));
    d->display();
    return;
  }
  
  uint8_t start = resultsPager.pageStart();
  uint8_t end = resultsPager.pageEnd();
  
  for (uint8_t i = start; i < end; i++) {
    uint8_t row = i - start;
    uint8_t y = 10 + (row * 10);
    
    if (i == resultsPager.selectedIdx) {
      d->fillRect(0, y, 128, 10, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    
    char sanitized[17];
    sanitizeSSID(sanitized, (const uint8_t*)scanner.networks[i].ssid, strlen(scanner.networks[i].ssid), sizeof(sanitized));
    
    // Convert RSSI to bars
    uint8_t bars = 0;
    int8_t rssi = scanner.networks[i].rssi;
    if (rssi >= -55) bars = 4;
    else if (rssi >= -67) bars = 3;
    else if (rssi >= -75) bars = 2;
    else if (rssi >= -85) bars = 1;
    
    d->setCursor(2, y + 1);
    d->print(sanitized);
    
    // Draw badge and signal bars at the right
    const char* badge = getEncBadge(scanner.networks[i].encryptionType);
    d->setCursor(90, y + 1);
    d->print(badge);
    
    display->drawSignalBars(110, y + 1, bars);
  }
  
  // Footer
  d->setTextColor(SSD1306_WHITE);
  if (resultsPager.hasPrev()) {
    d->setCursor(2, 52);
    d->print(F("<"));
  }
  if (resultsPager.hasNext()) {
    d->setCursor(120, 52);
    d->print(F(">"));
  }
  
  char countBuf[12];
  snprintf(countBuf, sizeof(countBuf), "[%d/%d]", resultsPager.selectedIdx + 1, resultsPager.totalItems);
  d->setCursor(45, 52);
  d->print(countBuf);
  
  d->display();
}

void WifiUI::drawDetails() {
  display->clear();
  display->drawStatusBar("AP Detail", RadioState::RADIO_WIFI);
  auto d = display->getDriver();
  
  WifiNetwork* net = &scanner.networks[resultsPager.selectedIdx];
  char ssid[17];
  sanitizeSSID(ssid, (const uint8_t*)net->ssid, strlen(net->ssid), sizeof(ssid));
  
  // Vendor OUI lookup
  uint8_t macBytes[3] = {0};
  unsigned int m0, m1, m2;
  const char* vendor = nullptr;
  if (sscanf(net->bssid, "%x:%x:%x", &m0, &m1, &m2) == 3) {
    macBytes[0] = m0;
    macBytes[1] = m1;
    macBytes[2] = m2;
    vendor = lookupOUI(macBytes);
  }
  
  d->setCursor(2, 12);
  d->print(F("SSID: "));
  d->print(ssid);
  
  d->setCursor(2, 22);
  d->print(F("BSSID: "));
  d->print(net->bssid);
  
  d->setCursor(2, 32);
  d->print(F("CH:"));
  d->print(net->channel);
  d->print(F(" RSSI:"));
  d->print(net->rssi);
  d->print(F("dBm"));
  
  d->setCursor(2, 42);
  d->print(F("ENC: "));
  switch(net->encryptionType) {
    case WIFI_AUTH_OPEN: d->print(F("Open")); break;
    case WIFI_AUTH_WEP: d->print(F("WEP")); break;
    case WIFI_AUTH_WPA_PSK: d->print(F("WPA")); break;
    case WIFI_AUTH_WPA2_PSK: d->print(F("WPA2")); break;
    case WIFI_AUTH_WPA_WPA2_PSK: d->print(F("WPA/WPA2")); break;
    case WIFI_AUTH_WPA3_PSK: d->print(F("WPA3")); break;
    default: d->print(F("Unknown")); break;
  }
  
  d->setCursor(2, 52);
  if (vendor) {
    d->print(F("Vendor: "));
    d->print(vendor);
  } else {
    d->print(F("[BACK] to return"));
  }
  
  d->display();
}

void WifiUI::drawChannelAnalyzer() {
  display->clear();
  display->drawStatusBar("Chan Analyzer", RadioState::RADIO_WIFI);
  auto d = display->getDriver();
  
  if (scanner.networkCount == 0) {
    d->setCursor(2, 30);
    d->print(F("Run Wi-Fi scan first."));
    d->display();
    return;
  }
  
  int chCounts[14] = {0};
  for (int i = 0; i < scanner.networkCount; i++) {
    uint8_t c = scanner.networks[i].channel;
    if (c >= 1 && c <= 13) chCounts[c]++;
  }
  
  int maxCount = 0;
  int busiestCh = 1;
  for (int i = 1; i <= 13; i++) {
    if (chCounts[i] > maxCount) {
      maxCount = chCounts[i];
      busiestCh = i;
    }
  }
  
  int bestCh = 1;
  int minCount = chCounts[1];
  if (chCounts[6] < minCount) { minCount = chCounts[6]; bestCh = 6; }
  if (chCounts[11] < minCount) { minCount = chCounts[11]; bestCh = 11; }
  
  d->setCursor(2, 12);
  d->print(F("Channels 1-13:"));
  
  for (int i = 1; i <= 13; i++) {
    int h = chCounts[i] * 2;
    if (h > 15) h = 15;
    if (h > 0) d->fillRect(5 + (i-1)*9, 40 - h, 7, h, SSD1306_WHITE);
    if (i == 1 || i == 6 || i == 11) {
      d->setCursor(5 + (i-1)*9, 42);
      d->print(i);
    }
  }
  
  d->setCursor(2, 52);
  d->printf("Busy:%d | Best(1/6/11):%d", busiestCh, bestCh);
  
  d->display();
}

void WifiUI::drawOpenNetworks() {
  display->clear();
  display->drawStatusBar("Open Nets", RadioState::RADIO_WIFI);
  auto d = display->getDriver();
  
  if (scanner.networkCount == 0) {
    d->setCursor(2, 30);
    d->print(F("Run Wi-Fi scan first."));
    d->display();
    return;
  }
  
  int openCount = 0;
  int y = 12;
  for (int i = 0; i < scanner.networkCount; i++) {
    if (scanner.networks[i].encryptionType == WIFI_AUTH_OPEN) {
      if (openCount < 5) {
        char sanitized[15];
        sanitizeSSID(sanitized, (const uint8_t*)scanner.networks[i].ssid, strlen(scanner.networks[i].ssid), sizeof(sanitized));
        d->setCursor(2, y);
        d->printf("%-14s %d", sanitized, scanner.networks[i].rssi);
        y += 10;
      }
      openCount++;
    }
  }
  
  if (openCount == 0) {
    d->setCursor(2, 30);
    d->print(F("No open networks found."));
  }
  
  d->display();
}

void WifiUI::drawStrongestNetwork() {
  display->clear();
  display->drawStatusBar("Strongest", RadioState::RADIO_WIFI);
  auto d = display->getDriver();
  
  if (scanner.networkCount == 0) {
    d->setCursor(2, 30);
    d->print(F("Run Wi-Fi scan first."));
    d->display();
    return;
  }
  
  WifiNetwork* net = &scanner.networks[0];
  char ssid[33];
  sanitizeSSID(ssid, (const uint8_t*)net->ssid, strlen(net->ssid), sizeof(ssid));
  
  d->setCursor(2, 20);
  d->print(ssid);
  d->setCursor(2, 30);
  d->printf("RSSI: %d", net->rssi);
  d->setCursor(2, 40);
  d->printf("Channel: %d", net->channel);
  d->setCursor(2, 50);
  d->print(F("Enc: "));
  switch(net->encryptionType) {
    case WIFI_AUTH_OPEN: d->print(F("Open")); break;
    case WIFI_AUTH_WEP: d->print(F("WEP")); break;
    case WIFI_AUTH_WPA_PSK: d->print(F("WPA")); break;
    case WIFI_AUTH_WPA2_PSK: d->print(F("WPA2")); break;
    default: d->print(F("WPA/WPA2 Mixed")); break;
  }
  
  d->display();
}

void WifiUI::drawDeviceInfo() {
  display->clear();
  display->drawStatusBar("Device Info", RadioState::RADIO_WIFI);
  auto d = display->getDriver();
  
  d->setCursor(2, 12);
  d->print(F("MAC: "));
  d->print(WiFi.macAddress());
  d->setCursor(2, 22);
  d->print(F("Mode: STA"));
  d->setCursor(2, 32);
  d->print(F("Rand MAC: Not Forced"));
  d->setCursor(2, 42);
  d->printf("Last Scan: %d nets", scanner.networkCount);
  d->setCursor(2, 52);
  d->printf("Heap: %u B", ESP.getFreeHeap());
  
  d->display();
}

void WifiUI::drawProbeMonitor() {
  display->clear();
  display->drawStatusBar("Probe Mon", RadioState::RADIO_WIFI);
  auto d = display->getDriver();
  
  if (probeCount == 0) {
    d->setCursor(2, 25);
    d->print(F("Listening for probes"));
    d->setCursor(2, 35);
    d->print(F("[Scanning...]"));
    d->display();
    return;
  }
  
  probePager.totalItems = probeCount;
  probePager.itemsPerPage = 2; // Each item takes 2 lines
  
  uint8_t start = probePager.pageStart();
  uint8_t end = probePager.pageEnd();
  
  uint8_t row = 0;
  for (uint8_t i = start; i < end; i++) {
    uint8_t y = 10 + (row * 20);
    
    if (i == probePager.selectedIdx) {
      d->fillRect(0, y, 128, 19, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    
    char macStr[20];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
             probeResults[i].mac[0], probeResults[i].mac[1], probeResults[i].mac[2],
             probeResults[i].mac[3], probeResults[i].mac[4], probeResults[i].mac[5]);
             
    d->setCursor(2, y + 1);
    d->printf("%s  %ddBm", macStr, probeResults[i].rssi);
    
    d->setCursor(8, y + 9);
    d->print(F("-> "));
    d->print(probeResults[i].ssid);
    
    row++;
  }
  
  d->setTextColor(SSD1306_WHITE);
  if (probePager.hasPrev()) {
    d->setCursor(2, 52);
    d->print(F("<"));
  }
  if (probePager.hasNext()) {
    d->setCursor(120, 52);
    d->print(F(">"));
  }
  
  char countBuf[20];
  snprintf(countBuf, sizeof(countBuf), "[%d/%d] Tot:%u", probePager.selectedIdx + 1, probeCount, totalProbesSeen);
  d->setCursor(35, 52);
  d->print(countBuf);
  
  d->display();
}

void WifiUI::stopProbeMonitorSniffer() {
  stopProbeMonitor();
}
