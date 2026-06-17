#include "probe_rain_ui.h"
#include <esp_wifi.h>

ProbeRainUI::ProbeRainUI(DisplayWrapper* disp)
  : display(disp), paused(false), running(false),
    lastFrameMs(0), capturedCount(0), lastProbeCount(0) {
  for (int i = 0; i < NUM_COLUMNS; i++) {
    columns[i].active = false;
  }
}

void ProbeRainUI::enter() {
  paused = false;
  running = true;
  lastFrameMs = millis();
  capturedCount = totalProbesSeen;
  lastProbeCount = probeCount;

  for (int i = 0; i < NUM_COLUMNS; i++) {
    columns[i].active = false;
  }

  // Start the probe sniffer
  startProbeMonitor();
}

void ProbeRainUI::stop() {
  stopProbeMonitor();
  running = false;
}

bool ProbeRainUI::update(ButtonEvent btn) {
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    stop();
    return false;
  }
  if (btn == BTN_SELECT) {
    paused = !paused;
  }

  // Channel hopping is handled by updateProbeHopper() in main loop

  // Check for new probe requests
  if (totalProbesSeen != capturedCount) {
    uint32_t diff = totalProbesSeen - capturedCount;
    if (diff > NUM_COLUMNS) diff = NUM_COLUMNS;
    
    for (uint32_t i = 0; i < diff; i++) {
      if (probeCount > 0) {
        int idx = random(0, probeCount);
        if (probeResults[idx].ssid[0] != '\0') {
          assignProbe(probeResults[idx].ssid);
        }
      }
    }
    capturedCount = totalProbesSeen;
  }

  // Render at ~15fps (66ms per frame)
  if (!paused && millis() - lastFrameMs >= 66) {
    lastFrameMs = millis();
    drawFrame();
  }

  return true;
}

void ProbeRainUI::assignProbe(const char* ssid) {
  // Find a free column
  for (int i = 0; i < NUM_COLUMNS; i++) {
    if (!columns[i].active) {
      strncpy(columns[i].ssid, ssid, 32);
      columns[i].ssid[32] = '\0';
      columns[i].len = strlen(columns[i].ssid);
      if (columns[i].len == 0) columns[i].len = 1;
      columns[i].y = -((float)columns[i].len * 8.0f); // Start above screen
      columns[i].speed = 0.8f + (float)(random(0, 120)) / 100.0f; // 0.8 – 2.0
      columns[i].active = true;
      return;
    }
  }

  // All columns full — replace one that's off-screen
  for (int i = 0; i < NUM_COLUMNS; i++) {
    if (columns[i].y > 64.0f + (float)columns[i].len * 8.0f) {
      strncpy(columns[i].ssid, ssid, 32);
      columns[i].ssid[32] = '\0';
      columns[i].len = strlen(columns[i].ssid);
      if (columns[i].len == 0) columns[i].len = 1;
      columns[i].y = -((float)columns[i].len * 8.0f);
      columns[i].speed = 0.8f + (float)(random(0, 120)) / 100.0f;
      columns[i].active = true;
      return;
    }
  }

  // Force-replace first column
  strncpy(columns[0].ssid, ssid, 32);
  columns[0].ssid[32] = '\0';
  columns[0].len = strlen(columns[0].ssid);
  if (columns[0].len == 0) columns[0].len = 1;
  columns[0].y = -((float)columns[0].len * 8.0f);
  columns[0].speed = 0.8f + (float)(random(0, 120)) / 100.0f;
  columns[0].active = true;
}

void ProbeRainUI::drawFrame() {
  display->clear();
  auto d = display->getDriver();
  d->setTextSize(1);

  // Status bar (8px tall)
  d->fillRect(0, 0, 128, 8, SSD1306_WHITE);
  d->setTextColor(SSD1306_BLACK);
  d->setCursor(2, 0);

  // Get current channel from probe monitor
  extern uint8_t wifiRegionSetting;
  uint8_t maxCh = getWiFiRegionMaxChannel();
  // Use a simple rotating channel display
  d->printf("PROBE RAIN CH:%d %lu", (int)(millis() / 200) % maxCh + 1, (unsigned long)capturedCount);
  d->setTextColor(SSD1306_WHITE);

  // Rain area: y = 8 to 63 (56px)
  for (int col = 0; col < NUM_COLUMNS; col++) {
    if (!columns[col].active) continue;

    // Update position
    columns[col].y += columns[col].speed;

    // Check if completely off screen
    if (columns[col].y > 64.0f + (float)columns[col].len * 8.0f) {
      columns[col].active = false;
      continue;
    }

    int x = col * COL_WIDTH;

    // Draw each character of the SSID vertically
    for (int c = 0; c < columns[col].len; c++) {
      int charY = (int)columns[col].y + (c * 8);

      // Only draw if within rain area (y=8..63)
      if (charY < 8 || charY > 63) continue;

      bool isLeading = (c == columns[col].len - 1); // Bottom = leading char
      int distFromLeading = columns[col].len - 1 - c;

      if (isLeading) {
        // Full brightness — draw normally
        d->setCursor(x, charY);
        d->setTextColor(SSD1306_WHITE);
        char ch[2] = { columns[col].ssid[c], '\0' };
        d->print(ch);
      } else if (distFromLeading < 3) {
        // Medium brightness — draw normally (near leading)
        d->setCursor(x, charY);
        d->setTextColor(SSD1306_WHITE);
        char ch[2] = { columns[col].ssid[c], '\0' };
        d->print(ch);
      } else {
        // Dim — pixel skipping effect
        // Draw character but only every other pixel
        d->setCursor(x, charY);
        d->setTextColor(SSD1306_WHITE);
        char ch[2] = { columns[col].ssid[c], '\0' };
        d->print(ch);
        // Dither: erase every other pixel in the character cell
        for (int py = charY; py < charY + 8 && py < 64; py++) {
          for (int px = x; px < x + 6 && px < 128; px++) {
            if ((px + py) % 2 == 0) {
              d->drawPixel(px, py, SSD1306_BLACK);
            }
          }
        }
      }
    }
  }

  if (paused) {
    d->fillRect(40, 28, 48, 12, SSD1306_BLACK);
    d->drawRect(40, 28, 48, 12, SSD1306_WHITE);
    d->setCursor(49, 30);
    d->print(F("PAUSE"));
  }

  d->display();
}
