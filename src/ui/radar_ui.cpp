#include "radar_ui.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265f
#endif

RadarUI::RadarUI(DisplayWrapper* disp)
  : display(disp), frozen(false), scanning(false),
    lastScanMs(0), lastFrameMs(0), blipCount(0) {}

void RadarUI::enter() {
  frozen = false;
  scanning = false;
  blipCount = 0;
  lastScanMs = 0;
  lastFrameMs = 0;

  // Start WiFi in STA mode for scanning
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);

  // Trigger initial async scan
  WiFi.scanNetworks(true);
  scanning = true;
  lastScanMs = millis();
}

void RadarUI::refreshScan() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;

  if (n > 0) {
    blipCount = (n > MAX_BLIPS) ? MAX_BLIPS : n;
    for (int i = 0; i < blipCount; i++) {
      strncpy(blips[i].ssid, WiFi.SSID(i).c_str(), 32);
      blips[i].ssid[32] = '\0';
      blips[i].rssi = WiFi.RSSI(i);
      blips[i].channel = WiFi.channel(i);
      blips[i].active = true;

      // Assign angle: spread by index + channel offset
      blips[i].angle = (float)i * (2.0f * PI / (float)blipCount) +
                        (float)(blips[i].channel % 13) * 0.3f;
      // Wrap angle
      while (blips[i].angle >= 2.0f * PI) blips[i].angle -= 2.0f * PI;

      // Assign radius based on RSSI
      if (blips[i].rssi > -50) {
        blips[i].radius = 8.0f + (float)(blips[i].rssi + 50) * 0.1f;
        if (blips[i].radius < 8.0f) blips[i].radius = 8.0f;
        if (blips[i].radius > 12.0f) blips[i].radius = 12.0f;
      } else if (blips[i].rssi > -70) {
        blips[i].radius = 14.0f + (float)(blips[i].rssi + 70) * -0.3f;
        if (blips[i].radius < 14.0f) blips[i].radius = 14.0f;
        if (blips[i].radius > 20.0f) blips[i].radius = 20.0f;
      } else {
        blips[i].radius = 22.0f + (float)(blips[i].rssi + 90) * -0.25f;
        if (blips[i].radius < 22.0f) blips[i].radius = 22.0f;
        if (blips[i].radius > 27.0f) blips[i].radius = 27.0f;
      }

      blips[i].hitMs = 0; // Will be set when sweep passes
    }
  }

  WiFi.scanDelete();
  scanning = false;
}

bool RadarUI::update(ButtonEvent btn) {
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    WiFi.scanDelete();
    return false;
  }
  if (btn == BTN_SELECT) {
    frozen = !frozen;
  }

  // Check if async scan completed
  if (scanning) {
    refreshScan();
  }

  // Trigger new scan every 5 seconds
  if (!frozen && !scanning && (millis() - lastScanMs > 5000)) {
    WiFi.scanNetworks(true);
    scanning = true;
    lastScanMs = millis();
  }

  // Render at ~20fps (50ms per frame)
  if (millis() - lastFrameMs >= 50) {
    lastFrameMs = millis();
    drawRadar();
  }

  return true;
}

void RadarUI::drawRadar() {
  display->clear();
  auto d = display->getDriver();

  const int cx = 54;
  const int cy = 32;
  const int outerR = 28;
  const int innerR = 14;

  // Draw outer ring (solid circle)
  d->drawCircle(cx, cy, outerR, SSD1306_WHITE);

  // Draw inner ring (dotted)
  for (int a = 0; a < 360; a += 10) {
    float rad = (float)a * PI / 180.0f;
    int px = cx + (int)(innerR * cos(rad));
    int py = cy + (int)(innerR * sin(rad));
    d->drawPixel(px, py, SSD1306_WHITE);
  }

  // Draw crosshairs (faint)
  for (int i = cx - outerR; i <= cx + outerR; i += 3) {
    d->drawPixel(i, cy, SSD1306_WHITE);
  }
  for (int i = cy - outerR; i <= cy + outerR; i += 3) {
    d->drawPixel(cx, i, SSD1306_WHITE);
  }

  // Sweep angle
  float sweepAngle;
  if (frozen) {
    sweepAngle = 0; // Frozen state
  } else {
    sweepAngle = (float)(millis() % 2000) / 2000.0f * 2.0f * PI;
  }

  // Draw sweep line + fading trail
  for (int t = 0; t < 4; t++) {
    float trailAngle = sweepAngle - (float)t * 0.15f;
    if (trailAngle < 0) trailAngle += 2.0f * PI;
    int ex = cx + (int)(outerR * cos(trailAngle));
    int ey = cy + (int)(outerR * sin(trailAngle));

    if (t == 0) {
      // Main sweep line — solid
      d->drawLine(cx, cy, ex, ey, SSD1306_WHITE);
    } else {
      // Trail — dithered (draw every other pixel)
      int steps = outerR;
      for (int s = 0; s < steps; s += (t + 1)) {
        float frac = (float)s / (float)steps;
        int px = cx + (int)(frac * (ex - cx));
        int py = cy + (int)(frac * (ey - cy));
        d->drawPixel(px, py, SSD1306_WHITE);
      }
    }
  }

  // Draw blips
  for (int i = 0; i < blipCount; i++) {
    if (!blips[i].active) continue;

    // Check if sweep just passed this blip (within 0.3 radians)
    float angleDiff = sweepAngle - blips[i].angle;
    if (angleDiff < 0) angleDiff += 2.0f * PI;
    if (angleDiff < 0.3f) {
      blips[i].hitMs = millis();
    }

    drawBlip(d, cx, cy, blips[i], sweepAngle);
  }

  // Draw side panel
  drawSidePanel(d);

  // Status indicator
  if (frozen) {
    d->setCursor(85, 56);
    d->setTextSize(1);
    d->print(F("FROZEN"));
  }

  d->display();
}

void RadarUI::drawBlip(Adafruit_SSD1306* d, int cx, int cy, const RadarBlip& blip, float sweepAngle) {
  int bx = cx + (int)(blip.radius * cos(blip.angle));
  int by = cy + (int)(blip.radius * sin(blip.angle));

  // Brightness based on time since sweep hit
  uint32_t elapsed = millis() - blip.hitMs;
  if (elapsed < 300) {
    // Bright — filled 3x3
    d->fillRect(bx - 1, by - 1, 3, 3, SSD1306_WHITE);
  } else if (elapsed < 700) {
    // Medium — 2x2
    d->fillRect(bx, by, 2, 2, SSD1306_WHITE);
  } else if (elapsed < 1200) {
    // Dim — single pixel
    d->drawPixel(bx, by, SSD1306_WHITE);
  }
  // else: invisible until next sweep pass
}

void RadarUI::drawSidePanel(Adafruit_SSD1306* d) {
  // Right side panel: x=85..128
  d->drawFastVLine(83, 0, 64, SSD1306_WHITE);

  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);
  d->setCursor(85, 1);
  d->printf("APs:%d", blipCount);

  // Show top 3 strongest APs
  // Sort indices by RSSI (simple selection of top 3)
  int top3[3] = {-1, -1, -1};
  for (int t = 0; t < 3 && t < blipCount; t++) {
    int bestIdx = -1;
    int bestRSSI = -999;
    for (int i = 0; i < blipCount; i++) {
      if (!blips[i].active) continue;
      bool alreadyUsed = false;
      for (int k = 0; k < t; k++) {
        if (top3[k] == i) { alreadyUsed = true; break; }
      }
      if (alreadyUsed) continue;
      if (blips[i].rssi > bestRSSI) {
        bestRSSI = blips[i].rssi;
        bestIdx = i;
      }
    }
    top3[t] = bestIdx;
  }

  for (int t = 0; t < 3; t++) {
    if (top3[t] < 0) break;
    int y = 12 + t * 16;
    d->setCursor(85, y);
    // Truncate SSID to 6 chars
    char shortSSID[7];
    strncpy(shortSSID, blips[top3[t]].ssid, 6);
    shortSSID[6] = '\0';
    d->print(shortSSID);

    // Signal bars
    int rssi = blips[top3[t]].rssi;
    uint8_t bars = 0;
    if (rssi > -50) bars = 4;
    else if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else if (rssi > -80) bars = 1;
    display->drawSignalBars(108, y + 8, bars);
  }
}
