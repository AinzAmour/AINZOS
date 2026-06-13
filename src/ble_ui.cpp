#include "ble_ui.h"
#include "mainwindow.h"

// Helper to parse Company ID from manufacturer data in advertisement payload
uint16_t getCompanyIdFromPayload(const uint8_t* payload, uint8_t len) {
    uint8_t i = 0;
    while (i < len) {
        uint8_t adLen = payload[i];
        if (adLen == 0) break;
        if (i + 1 + adLen > len) break;
        uint8_t adType = payload[i+1];
        if (adType == 0xFF) { // Manufacturer Specific Data
            if (adLen >= 3) {
                return payload[i+2] | (payload[i+3] << 8);
            }
        }
        i += adLen + 1;
    }
    return 0;
}

BleUI::BleUI(DisplayWrapper* disp, SettingsManager* sm) : display(disp), settings(sm) {
  currentPage = BlePage::Menu;
  menuSelectedIndex = 0;
  menuTopIndex = 0;
  filterMode = BleFilter::ALL;
  snprintf(bleFilterMenuItem, sizeof(bleFilterMenuItem), "Filter: All");
}



void BleUI::begin() {
  scanner.begin();
}

void BleUI::enter(BlePage target) {
  currentPage = target;
  if (settings) {
    filterMode = (BleFilter)settings->ble_filter;
  }
  
  if (currentPage == BlePage::Menu) {
    menuSelectedIndex = 0;
    menuTopIndex = 0;
    drawMenu();
  } else if (currentPage == BlePage::ResultsList) {
    drawResultsList();
  } else if (currentPage == BlePage::Details) {
    drawDetails();
  }
}

bool BleUI::update(ButtonEvent btn) {
  if (currentPage == BlePage::Menu) {
    bool redraw = false;
    if (btn == BTN_DOWN) {
      if (menuSelectedIndex < BLE_MENU_COUNT - 1) {
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
          currentPage = BlePage::Scanning;
          // Load custom duration from settings
          if (settings) {
            scanner.scanDurationMs = settings->ble_duration * 1000;
          }
          scanner.startScanAsync();
          scanAnimTime = millis();
          scanAnimFrame = 0;
          drawScanning();
          break;
        case 1: // Saved Results
          currentPage = BlePage::ResultsList;
          resultsPager.reset();
          drawResultsList();
          break;
        case 2: // Filter
          if (filterMode == BleFilter::ALL) filterMode = BleFilter::TRACKERS;
          else if (filterMode == BleFilter::TRACKERS) filterMode = BleFilter::UART;
          else if (filterMode == BleFilter::UART) filterMode = BleFilter::UNKNOWN;
          else if (filterMode == BleFilter::UNKNOWN) filterMode = BleFilter::ALL;
          
          if (settings) {
            settings->ble_filter = (uint8_t)filterMode;
            settings->save();
          }
          redraw = true;
          break;
        case 3: // Strongest
          currentPage = BlePage::StrongestDevice;
          drawStrongestDevice();
          break;
        case 4: // Device Info
          currentPage = BlePage::DeviceInfo;
          drawDeviceInfo();
          break;
        case 5: // Clear Results
          scanner.clearResults();
          display->clear();
          display->getDriver()->setCursor(10,30);
          display->getDriver()->print(F("Results Cleared!"));
          display->getDriver()->display();
          delay(800);
          drawMenu();
          break;
      }
      return true;
    } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      return false; // Exit BLE tools
    }
    
    if (redraw) drawMenu();
    return true;
  }
  
  if (currentPage == BlePage::Scanning) {
    if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      scanner.cancelScan();
      currentPage = BlePage::Menu;
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
    } else if (status == ScanState::Complete) {
      scanner.stopScanAndStore();
      currentPage = BlePage::ResultsList;
      resultsPager.reset();
      drawResultsList();
    }
    return true;
  }
  
  if (currentPage == BlePage::ResultsList) {
    if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      currentPage = BlePage::Menu;
      drawMenu();
      return true;
    }
    handleResultsList(btn);
    return true;
  }
  
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = BlePage::Menu;
    drawMenu();
  }
  
  return true;
}

void BleUI::drawMenu() {
  const char* filterStr = "All";
  if (filterMode == BleFilter::TRACKERS) filterStr = "Trackers";
  else if (filterMode == BleFilter::UART) filterStr = "UART";
  else if (filterMode == BleFilter::UNKNOWN) filterStr = "Unknown";
  snprintf(bleFilterMenuItem, sizeof(bleFilterMenuItem), "Filter: %s", filterStr);
  display->drawMenu("BLE Tools", bleMenuItems, BLE_MENU_COUNT, menuSelectedIndex, menuTopIndex);
}

void BleUI::drawScanning() {
  display->clear();
  display->drawStatusBar("Scanning", RadioState::RADIO_BLE);
  auto d = display->getDriver();
  d->setCursor(20, 30);
  d->print(F("Scanning"));
  for (int i=0; i<scanAnimFrame; i++) d->print(F("."));
  
  long elapsed = millis() - scanner.scanStartTime;
  int remaining = scanner.scanDurationMs - elapsed;
  if (remaining < 0) remaining = 0;
  int w = map(remaining, 0, scanner.scanDurationMs, 0, 100);
  d->drawRect(14, 45, 100, 6, SSD1306_WHITE);
  d->fillRect(14, 45, w, 6, SSD1306_WHITE);
  
  d->display();
}

bool BleUI::matchesFilter(const BleDevice& dev) {
  if (filterMode == BleFilter::ALL) return true;
  if (filterMode == BleFilter::TRACKERS) {
    return (dev.deviceType == BLE_TYPE_AIRTAG || 
            dev.deviceType == BLE_TYPE_FIND_MY || 
            dev.deviceType == BLE_TYPE_TILE || 
            dev.deviceType == BLE_TYPE_SAMSUNG_TAG);
  }
  if (filterMode == BleFilter::UART) {
    return (dev.deviceType == BLE_TYPE_NORDIC_UART);
  }
  if (filterMode == BleFilter::UNKNOWN) {
    return (dev.deviceType == BLE_TYPE_GENERIC);
  }
  return true;
}

const char* BleUI::getBLETypeTag(uint8_t t) {
  switch ((BleDeviceType)t) {
    case BLE_TYPE_AIRTAG:          return "[AT]";
    case BLE_TYPE_FIND_MY:         return "[FM]";
    case BLE_TYPE_FLIPPER:         return "[FZ]";
    case BLE_TYPE_TILE:            return "[TL]";
    case BLE_TYPE_NORDIC_UART:     return "[NU]";
    case BLE_TYPE_SAMSUNG_TAG:     return "[ST]";
    case BLE_TYPE_SKIMMER_SUSPECT: return "[SK]";
    default:                       return "    ";
  }
}

void BleUI::handleResultsList(ButtonEvent btn) {
  // Build matches
  uint8_t matches[BleScanner::MAX_DEVICES];
  uint8_t matchCount = 0;
  for (uint8_t i = 0; i < scanner.deviceCount; i++) {
    if (matchesFilter(scanner.devices[i])) {
      matches[matchCount++] = i;
    }
  }
  
  if (matchCount == 0) return;
  resultsPager.totalItems = matchCount;
  resultsPager.itemsPerPage = 4;
  
  bool redraw = false;
  if (btn == BTN_DOWN) {
    resultsPager.moveDown();
    redraw = true;
  } else if (btn == BTN_UP) {
    resultsPager.moveUp();
    redraw = true;
  } else if (btn == BTN_SELECT) {
    navStack.push(Page::BLEMenu);
    window.changePage(Page::BLEDetails);
    return;
  }
  
  if (redraw) drawResultsList();
}

void BleUI::drawResultsList() {
  display->clear();
  display->drawStatusBar("BLE Results", RadioState::RADIO_BLE);
  
  auto d = display->getDriver();
  
  uint8_t matches[BleScanner::MAX_DEVICES];
  uint8_t matchCount = 0;
  for (uint8_t i = 0; i < scanner.deviceCount; i++) {
    if (matchesFilter(scanner.devices[i])) {
      matches[matchCount++] = i;
    }
  }
  
  if (matchCount == 0) {
    d->setCursor(2, 30);
    d->print(F("No devices match filter."));
    d->display();
    return;
  }
  
  resultsPager.totalItems = matchCount;
  resultsPager.itemsPerPage = 4;
  
  uint8_t start = resultsPager.pageStart();
  uint8_t end = resultsPager.pageEnd();
  
  for (uint8_t i = start; i < end; i++) {
    uint8_t row = i - start;
    uint8_t y = 10 + (row * 10);
    uint8_t actualIdx = matches[i];
    
    bool isSkimmer = (scanner.devices[actualIdx].deviceType == BLE_TYPE_SKIMMER_SUSPECT);
    
    if (i == resultsPager.selectedIdx) {
      d->fillRect(0, y, 128, 10, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else if (isSkimmer) {
      d->fillRect(0, y, 128, 10, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    
    // Tag + Name
    const char* tag = getBLETypeTag(scanner.devices[actualIdx].deviceType);
    char rowBuf[32];
    snprintf(rowBuf, sizeof(rowBuf), "%s%s", tag, scanner.devices[actualIdx].name);
    rowBuf[14] = '\0'; // Truncate to avoid overlap
    
    d->setCursor(2, y + 1);
    d->printf("%-14s %d", rowBuf, scanner.devices[actualIdx].rssi);
  }
  
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

void BleUI::drawDetails() {
  display->clear();
  display->drawStatusBar("BLE Detail", RadioState::RADIO_BLE);
  auto d = display->getDriver();
  
  // Find matching list
  uint8_t matches[BleScanner::MAX_DEVICES];
  uint8_t matchCount = 0;
  for (uint8_t i = 0; i < scanner.deviceCount; i++) {
    if (matchesFilter(scanner.devices[i])) {
      matches[matchCount++] = i;
    }
  }
  
  if (matchCount == 0) return;
  uint8_t actualIdx = matches[resultsPager.selectedIdx];
  BleDevice* dev = &scanner.devices[actualIdx];
  
  uint16_t cid = getCompanyIdFromPayload(dev->advPayload, dev->advPayloadLen);
  
  d->setCursor(2, 12);
  d->print(F("Name: "));
  d->print(dev->name);
  
  d->setCursor(2, 22);
  d->print(F("MAC: "));
  d->print(dev->address);
  
  d->setCursor(2, 32);
  d->print(F("RSSI: "));
  d->print(dev->rssi);
  d->print(F(" dBm"));
  
  d->setCursor(2, 42);
  d->print(F("Type: "));
  d->print(getBLETypeTag(dev->deviceType));
  switch((BleDeviceType)dev->deviceType) {
    case BLE_TYPE_AIRTAG: d->print(F(" AirTag")); break;
    case BLE_TYPE_FIND_MY: d->print(F(" FindMy")); break;
    case BLE_TYPE_FLIPPER: d->print(F(" Flipper")); break;
    case BLE_TYPE_TILE: d->print(F(" Tile")); break;
    case BLE_TYPE_NORDIC_UART: d->print(F(" NordicUART")); break;
    case BLE_TYPE_SAMSUNG_TAG: d->print(F(" SamsungTag")); break;
    case BLE_TYPE_SKIMMER_SUSPECT: d->print(F(" Skimmer?")); break;
    default: d->print(F(" Generic")); break;
  }
  
  d->setCursor(2, 52);
  d->printf("CompID:0x%04X Svcs:%d", cid, dev->serviceCount);
  
  d->display();
}

void BleUI::drawStrongestDevice() {
  display->clear();
  display->drawStatusBar("Strongest", RadioState::RADIO_BLE);
  auto d = display->getDriver();
  
  if (scanner.deviceCount == 0) {
    d->setCursor(2, 30);
    d->print(F("Run BLE scan first."));
    d->display();
    return;
  }
  
  BleDevice* dev = &scanner.devices[0];
  
  d->setCursor(2, 20);
  d->print(dev->name);
  d->setCursor(2, 30);
  d->print(dev->address);
  d->setCursor(2, 40);
  d->printf("RSSI: %d", dev->rssi);
  d->setCursor(2, 50);
  d->printf("Type: %s Svcs:%d", getBLETypeTag(dev->deviceType), dev->serviceCount);
  
  d->display();
}

void BleUI::drawDeviceInfo() {
  display->clear();
  display->drawStatusBar("Device Info", RadioState::RADIO_BLE);
  auto d = display->getDriver();
  
  d->setCursor(2, 12);
  d->print(F("NimBLE: Enabled"));
  d->setCursor(2, 22);
  d->print(F("Mode: Scanner Only"));
  d->setCursor(2, 32);
  d->printf("Max Devices: %d", scanner.MAX_DEVICES);
  d->setCursor(2, 42);
  d->printf("Last Scan: %d devs", scanner.deviceCount);
  d->setCursor(2, 52);
  d->printf("Heap: %u B", ESP.getFreeHeap());
  
  d->display();
}
