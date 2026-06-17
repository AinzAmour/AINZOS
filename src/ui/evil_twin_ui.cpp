#include "evil_twin_ui.h"

EvilTwinUI::EvilTwinUI(DisplayWrapper* disp)
  : display(disp), currentPage(EvilTwinPage::Scanning),
    scanCount(0), scanSel(0), scanTop(0),
    capSel(0), capTop(0), capCount(0),
    lastStatusUpdate(0) {}

EvilTwinUI::~EvilTwinUI() {
  if (twin.isRunning()) twin.stop();
}

void EvilTwinUI::enter() {
  if (twin.isRunning()) {
    currentPage = EvilTwinPage::Running;
    lastStatusUpdate = 0;
    drawRunning();
  } else {
    startScan();
  }
}

void EvilTwinUI::startScan() {
  currentPage = EvilTwinPage::Scanning;
  display->clear();
  display->drawHeader("Evil Twin");
  auto d = display->getDriver();
  d->setCursor(2, 28);
  d->print(F("Scanning WiFi..."));
  d->display();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  int n = WiFi.scanNetworks(false, true);
  scanCount = (n <= 0) ? 0 : ((n > MAX_SCAN) ? MAX_SCAN : n);
  for (int i = 0; i < scanCount; i++) {
    strncpy(scanAPs[i].ssid, WiFi.SSID(i).c_str(), 32);
    scanAPs[i].ssid[32] = '\0';
    scanAPs[i].channel = WiFi.channel(i);
    scanAPs[i].rssi = WiFi.RSSI(i);
  }
  WiFi.scanDelete();

  scanSel = 0; scanTop = 0;
  currentPage = EvilTwinPage::APPicker;

  if (scanCount == 0) {
    display->clear();
    display->drawHeader("Evil Twin");
    auto d2 = display->getDriver();
    d2->setCursor(2, 28);
    d2->print(F("No APs found."));
    d2->setCursor(2, 42);
    d2->print(F("Press BACK"));
    d2->display();
  } else {
    drawAPList();
  }
}

bool EvilTwinUI::update(ButtonEvent btn) {
  switch (currentPage) {
    case EvilTwinPage::Scanning:
      // Scan is blocking, should transition to APPicker
      if (btn == BTN_BACK) return false;
      break;

    case EvilTwinPage::APPicker:
      if (scanCount == 0) {
        if (btn == BTN_BACK) return false;
        return true;
      }
      if (btn == BTN_DOWN && scanSel < scanCount - 1) {
        scanSel++;
        if (scanSel >= scanTop + 4) scanTop++;
        drawAPList();
      } else if (btn == BTN_UP && scanSel > 0) {
        scanSel--;
        if (scanSel < scanTop) scanTop--;
        drawAPList();
      } else if (btn == BTN_SELECT) {
        currentPage = EvilTwinPage::Confirm;
        drawConfirm();
      } else if (btn == BTN_BACK) {
        return false;
      }
      break;

    case EvilTwinPage::Confirm:
      if (btn == BTN_SELECT) {
        // Start Evil Twin
        twin.start(scanAPs[scanSel].ssid, scanAPs[scanSel].channel);
        currentPage = EvilTwinPage::Running;
        lastStatusUpdate = 0;
        drawRunning();
      } else if (btn == BTN_BACK) {
        currentPage = EvilTwinPage::APPicker;
        drawAPList();
      }
      break;

    case EvilTwinPage::Running:
      if (btn == BTN_SELECT) {
        twin.stop();
        return false;
      } else if (btn == BTN_DOWN) {
        // View captures
        capSel = 0; capTop = 0;
        capCount = twin.getStoredCaptureCount();
        currentPage = EvilTwinPage::Captures;
        drawCaptures();
      } else if (btn == BTN_BACK) {
        twin.stop();
        return false;
      }
      // Auto-refresh status every 500ms
      if (millis() - lastStatusUpdate > 500) {
        lastStatusUpdate = millis();
        drawRunning();
      }
      break;

    case EvilTwinPage::Captures:
      if (btn == BTN_DOWN && capSel < capCount - 1) {
        capSel++;
        if (capSel >= capTop + 4) capTop++;
        drawCaptures();
      } else if (btn == BTN_UP && capSel > 0) {
        capSel--;
        if (capSel < capTop) capTop--;
        drawCaptures();
      } else if (btn == BTN_BACK) {
        currentPage = EvilTwinPage::Running;
        lastStatusUpdate = 0;
        drawRunning();
      }
      break;
  }
  return true;
}

void EvilTwinUI::drawAPList() {
  display->clear();
  display->drawHeader("Select Target");
  auto d = display->getDriver();
  d->setTextSize(1);

  int maxVisible = 4;
  int startY = 14;
  for (int i = 0; i < maxVisible; i++) {
    int idx = scanTop + i;
    if (idx >= scanCount) break;
    int y = startY + (i * 12);
    if (idx == scanSel) {
      d->fillRect(0, y, 128, 12, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    d->setCursor(2, y + 2);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.15s %ddB", scanAPs[idx].ssid, scanAPs[idx].rssi);
    d->print(buf);
  }
  d->setTextColor(SSD1306_WHITE);
  d->setCursor(35, 56);
  char countBuf[12];
  snprintf(countBuf, sizeof(countBuf), "[%d/%d]", scanSel + 1, scanCount);
  d->print(countBuf);
  d->display();
}

void EvilTwinUI::drawConfirm() {
  display->clear();
  display->drawHeader("Evil Twin");
  auto d = display->getDriver();
  d->setCursor(2, 14);
  d->print(F("Clone AP:"));
  d->setCursor(2, 26);
  char ssidBuf[22];
  snprintf(ssidBuf, sizeof(ssidBuf), "%.20s", scanAPs[scanSel].ssid);
  d->print(ssidBuf);
  d->setCursor(2, 38);
  d->printf("Channel: %d", scanAPs[scanSel].channel);
  d->setCursor(2, 52);
  d->print(F("SEL=Start BACK=Cancel"));
  d->display();
}

void EvilTwinUI::drawRunning() {
  display->clear();
  display->drawHeader("Evil Twin");
  auto d = display->getDriver();

  d->setCursor(2, 12);
  d->print(F("Status: ACTIVE"));

  d->setCursor(2, 22);
  char ssidBuf[22];
  snprintf(ssidBuf, sizeof(ssidBuf), "SSID:%.17s", twin.isRunning() ? scanAPs[scanSel].ssid : "N/A");
  d->print(ssidBuf);

  d->setCursor(2, 32);
  d->printf("Clients: %d", twin.getClientCount());

  d->setCursor(2, 42);
  d->printf("Captures: %d", twin.getCaptureCount());

  String last = twin.getLastCapture();
  if (last.length() > 0) {
    d->setCursor(2, 52);
    char lastBuf[22];
    snprintf(lastBuf, sizeof(lastBuf), "%.20s", last.c_str());
    d->print(lastBuf);
  } else {
    d->setCursor(2, 52);
    d->print(F("SEL=Stop DOWN=Caps"));
  }

  d->display();
}

void EvilTwinUI::drawCaptures() {
  display->clear();
  display->drawHeader("Captures");
  auto d = display->getDriver();

  if (capCount == 0) {
    d->setCursor(2, 28);
    d->print(F("No captures stored."));
    d->setCursor(2, 42);
    d->print(F("Press BACK"));
    d->display();
    return;
  }

  int maxVisible = 4;
  int startY = 14;
  for (int i = 0; i < maxVisible; i++) {
    int idx = capTop + i;
    if (idx >= capCount) break;
    int y = startY + (i * 12);
    if (idx == capSel) {
      d->fillRect(0, y, 128, 12, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    d->setCursor(2, y + 2);
    String cap = twin.getStoredCapture(idx);
    char capBuf[22];
    snprintf(capBuf, sizeof(capBuf), "#%d:%.17s", idx + 1, cap.c_str());
    d->print(capBuf);
  }
  d->setTextColor(SSD1306_WHITE);
  d->display();
}
