#include "lab_ui.h"
#include <WiFi.h>
#include <string.h>
#include <stdio.h>
#include "text_input_ui.h"
#include "esp_system.h"

LabUI::LabUI(DisplayWrapper* disp)
  : display(disp), currentPage(LabPage::Menu),
    menuSel(0), menuTop(0),
    bleSel(0), bleTop(0), spamRunning(false),
    wifiSel(0), wifiTop(0), wifiAttackRunning(false),
    scanCount(0), scanSel(0), scanTop(0), showTargetConfirm(false), targetConfirmExpiresMs(0),
    textInput(nullptr), textInputActive(false) {
  selectedAP.valid = false;
  selectedAP.ssid[0] = '\0';
  evilTwinUI = new EvilTwinUI(disp);
}

void LabUI::stopAllWiFiAttacks() {
  if (beacon_spam_is_running()) beacon_spam_stop();
  if (deauth_attack_is_running()) deauth_attack_stop();
  if (channel_switch_attack_is_running()) channel_switch_attack_stop();
  if (dhcp_starvation_is_running()) dhcp_starvation_stop();
  if (eapol_logoff_is_running()) eapol_logoff_stop();
  if (gtk_abuse_is_running()) gtk_abuse_stop();
  wifiAttackRunning = false;
}

void LabUI::drawScanResultsInternal(int count, int sel, int top) {
  Adafruit_SSD1306* d = display->getDriver();
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);
  int maxVisible = 4;
  int startY = 22;
  for (int i = 0; i < maxVisible; i++) {
    int idx = top + i;
    if (idx >= count) break;
    int y = startY + (i * 10);
    if (idx == sel) {
      d->fillRect(0, y, 128, 10, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    d->setCursor(2, y + 1);
    char buf[40];
    snprintf(buf, sizeof(buf), "%s %ddB", scanResults[idx].ssid, scanResults[idx].rssi);
    d->print(buf);
  }
  d->display();
}

bool LabUI::startSelectedWiFiAttack() {
  // Ensure BLE spam is not running while starting WiFi attacks
  if (ble_spam_is_running()) {
    Serial.println("[LABUI] BLE active, stopping BLE for WiFi attack");
    ble_spam_stop();
    spamRunning = false;
  }
  // Returns true if an attack was started (wifiAttackRunning should be set)
  // wifiSel 0 is Select Target
  switch (wifiSel) {
    case 0:
      // Start scan UI instead of an attack
      // Show scanning info and perform blocking scan
      display->drawInfoPage("Scanning...", NULL, 0);
      WiFi.mode(WIFI_STA);
      WiFi.disconnect(false, true);
      {
        int n = WiFi.scanNetworks(false, true);
        scanCount = (n <= 0) ? 0 : ((n > WIFI_SCAN_MAX) ? WIFI_SCAN_MAX : n);
        for (int i = 0; i < scanCount; i++) {
          strncpy(scanResults[i].ssid, WiFi.SSID(i).c_str(), 32);
          scanResults[i].ssid[32] = '\0';
          memcpy(scanResults[i].bssid, WiFi.BSSID(i), 6);
          scanResults[i].channel = WiFi.channel(i);
          scanResults[i].rssi = WiFi.RSSI(i);
        }
        WiFi.scanDelete();
      }
      scanSel = 0; scanTop = 0;
      currentPage = LabPage::WiFiScan;
      // draw list
      display->clear();
      display->drawHeader("Select Target");
      if (scanCount == 0) {
        const char* lines[] = { "No APs found.", "Press BACK" };
        display->drawInfoPage("Scan", lines, 2);
      } else {
        drawScanResultsInternal(scanCount, scanSel, scanTop);
      }
      return false;
      break;
    case 1: // Beacon Spam — allow without selected AP
      Serial.printf("[LABUI] Starting Beacon Spam (index %d)\n", wifiSel);
      if (selectedAP.valid) {
        beacon_spam_start(selectedAP.ssid);
      } else {
        beacon_spam_start(NULL);
      }
      Serial.println("[LABUI] Beacon Spam started");
      return true;
      break;
    case 2: // Beacon list mode
      Serial.printf("[LABUI] Starting beacon list\n");
      beacon_spam_start_list();
      Serial.println("[LABUI] Beacon list started");
      return true;
      break;
    case 3: // Deauth Attack (requires target or defaults to auto/broadcast)
      Serial.println("[LABUI] Starting Deauth Attack");
      deauth_attack_start();
      return true;
      break;
    case 4: // Channel Switch
      Serial.println("[LABUI] Starting Channel Switch");
      channel_switch_attack_start();
      return true;
      break;
    case 5: // DHCP Starvation
      Serial.println("[LABUI] Starting DHCP Starvation");
      dhcp_starvation_start(1);
      return true;
      break;
    case 6: // EAPOL Logoff
      Serial.println("[LABUI] Starting EAPOL Logoff");
      eapol_logoff_start();
      return true;
      break;
    case 7: // GTK Abuse requires SSID and password
      if (!selectedAP.valid) {
        const char* lines[] = { "Select Target", "First", "", "Back" };
        display->drawInfoPage("Error", lines, 4);
        return false;
      }
      // Prompt for password (non-blocking)
      textInputBuf[0] = '\0';
      textInput = new TextInputUI(display, textInputBuf, sizeof(textInputBuf), "Password", true);
      textInput->enter();
      textInputActive = true;
      return false;
      break;
    default:
      return false;
    case 8: // Evil Twin
      currentPage = LabPage::EvilTwin;
      evilTwinUI->enter();
      return false;
  }
}

void LabUI::stopSelectedWiFiAttack() {
  switch (wifiSel) {
    case 1: // Beacon Spam
    case 2: // Beacon List
      Serial.println("[LABUI] Stopping Beacon Spam");
      beacon_spam_stop(); break;
    case 3: // Deauth
      Serial.println("[LABUI] Stopping Deauth");
      deauth_attack_stop(); break;
    case 4: // Channel Switch
      Serial.println("[LABUI] Stopping Channel Switch");
      channel_switch_attack_stop(); break;
    case 5: // DHCP Starvation
      Serial.println("[LABUI] Stopping DHCP Starvation");
      dhcp_starvation_stop(); break;
    case 6: // EAPOL Logoff
      Serial.println("[LABUI] Stopping EAPOL Logoff");
      eapol_logoff_stop(); break;
    case 7: // GTK Abuse
      Serial.println("[LABUI] Stopping GTK Abuse");
      gtk_abuse_stop(); break;
    default: break;
  }
}

void LabUI::enter() {
  if (ble_spam_is_running()) { ble_spam_stop(); spamRunning = false; }
  stopAllWiFiAttacks();
  if (evilTwinUI) { /* evil twin stopped in stopAllWiFiAttacks */ }
  currentPage = LabPage::Menu;
  menuSel = 0; menuTop = 0;
  drawMenu();
}

bool LabUI::update(ButtonEvent btn) {
  if (currentPage == LabPage::Menu && btn == BTN_BACK) {
    if (ble_spam_is_running()) { ble_spam_stop(); spamRunning = false; }
    stopAllWiFiAttacks();
    return false;
  }
  switch (currentPage) {
    case LabPage::Menu:       handleMenu(btn);       break;
    case LabPage::BLESpam:    handleBLESpam(btn);    break;
    case LabPage::WiFiAttacks: handleWiFiAttacks(btn); break;
    case LabPage::WiFiScan:
      // WiFi scan list navigation
      if (btn == BTN_BACK) {
        currentPage = LabPage::WiFiAttacks;
        drawWiFiAttackMenu();
        return true;
      }
      if (scanCount == 0) return true;
      if (btn == BTN_DOWN && scanSel < scanCount - 1) {
        scanSel++;
        if (scanSel >= scanTop + 4) scanTop++;
        display->clear();
        display->drawHeader("Select Target");
        drawScanResultsInternal(scanCount, scanSel, scanTop);
      } else if (btn == BTN_UP && scanSel > 0) {
        scanSel--;
        if (scanSel < scanTop) scanTop--;
        display->clear();
        display->drawHeader("Select Target");
        drawScanResultsInternal(scanCount, scanSel, scanTop);
      } else if (btn == BTN_SELECT) {
        // Set selected AP
        strncpy(selectedAP.ssid, scanResults[scanSel].ssid, sizeof(selectedAP.ssid)-1);
        memcpy(selectedAP.bssid, scanResults[scanSel].bssid, 6);
        selectedAP.channel = scanResults[scanSel].channel;
        selectedAP.valid = true;
        showTargetConfirm = true;
        targetConfirmExpiresMs = millis() + 1000;
        currentPage = LabPage::WiFiAttacks;
        wifiSel = 0; wifiTop = 0;
        drawWiFiAttackMenu();
      }
      break;
    case LabPage::EvilTwin:
      if (!evilTwinUI->update(btn)) {
        currentPage = LabPage::WiFiAttacks;
        drawWiFiAttackMenu();
      }
      break;
  }
  return true;
}

void LabUI::handleMenu(ButtonEvent btn) {
  if (btn == BTN_DOWN && menuSel < LAB_MENU_COUNT - 1) {
    menuSel++;
    if (menuSel >= menuTop + 5) menuTop++;
    drawMenu();
  } else if (btn == BTN_UP && menuSel > 0) {
    menuSel--;
    if (menuSel < menuTop) menuTop--;
    drawMenu();
  } else if (btn == BTN_SELECT) {
    if (menuSel == 0) {
      bleSel = 0; bleTop = 0;
      currentPage = LabPage::BLESpam;
      drawBLESpamMenu();
    } else if (menuSel == 1) {
      wifiSel = 0; wifiTop = 0;
      currentPage = LabPage::WiFiAttacks;
      drawWiFiAttackMenu();
    }
  }
}

void LabUI::handleBLESpam(ButtonEvent btn) {
  // Refresh running state from underlying module
  if (ble_spam_is_running()) spamRunning = true;
  if (spamRunning) {
    // Toggle behavior: SELECT stops the spam
    if (btn == BTN_SELECT) {
      Serial.println("[LABUI] BLE Spam stopping...");
      ble_spam_stop();
      spamRunning = false;
      drawBLESpamMenu();
    }
    return;
  }
  if (btn == BTN_BACK) {
    currentPage = LabPage::Menu;
    drawMenu();
    return;
  }
  if (btn == BTN_DOWN && bleSel < BLE_SPAM_COUNT - 1) {
    bleSel++;
    if (bleSel >= bleTop + 5) bleTop++;
    drawBLESpamMenu();
  } else if (btn == BTN_UP && bleSel > 0) {
    bleSel--;
    if (bleSel < bleTop) bleTop--;
    drawBLESpamMenu();
  } else if (btn == BTN_SELECT) {
    // Ensure WiFi attacks are stopped before starting BLE spam
    stopAllWiFiAttacks();
    Serial.printf("[LABUI] Starting BLE Spam type %d\n", (int)bleSpamTypes[bleSel]);
    ble_spam_start(bleSpamTypes[bleSel]);
    spamRunning = true;
    Serial.println("[LABUI] BLE Spam started");
    drawSpamRunning();
  }
}

void LabUI::handleWiFiAttacks(ButtonEvent btn) {
  // If a text input modal is active, forward events to it
  if (textInputActive && textInput) {
    bool stillActive = textInput->update(btn);
    if (!stillActive) {
      // finished or cancelled
      if (!textInput->wasCancelled()) {
        const char* passwd = textInput->getText();
        if (!passwd || passwd[0] == '\0') {
          const char* lines[] = { "Password required", "Press SELECT" };
          display->drawInfoPage("Error", lines, 2);
          // Do not start attack; user can SELECT again to re-enter
        } else {
          // Ensure BLE stopped for radio coexistence
          if (ble_spam_is_running()) {
            Serial.println("[LABUI] BLE active, stopping before GTK Abuse");
            ble_spam_stop();
            spamRunning = false;
          }
          Serial.println("[LABUI] GTK password entered, starting attack");
          gtk_abuse_start(selectedAP.ssid, passwd);
          wifiAttackRunning = true;
          drawWiFiAttackRunning();
        }
      } else {
        // cancelled
        drawWiFiAttackMenu();
      }
      delete textInput;
      textInput = nullptr;
      textInputActive = false;
    }
    return;
  }
  if (wifiAttackRunning) {
    if (btn != BTN_NONE) {
      stopSelectedWiFiAttack();
      wifiAttackRunning = false;
      drawWiFiAttackMenu();
    }
    return;
  }
  if (btn == BTN_BACK) {
    currentPage = LabPage::Menu;
    drawMenu();
    return;
  }
  if (btn == BTN_DOWN && wifiSel < WIFI_ATTACK_COUNT - 1) {
    wifiSel++;
    if (wifiSel >= wifiTop + 5) wifiTop++;
    drawWiFiAttackMenu();
  } else if (btn == BTN_UP && wifiSel > 0) {
    wifiSel--;
    if (wifiSel < wifiTop) wifiTop--;
    drawWiFiAttackMenu();
  } else if (btn == BTN_SELECT) {
    bool started = startSelectedWiFiAttack();
    if (started) {
      wifiAttackRunning = true;
      drawWiFiAttackRunning();
    }
    // if not started, startSelectedWiFiAttack handled UI transition (scan or error)
  }
}

void LabUI::drawMenu() {
  display->drawMenu("Lab Tools", labMenuItems, LAB_MENU_COUNT, menuSel, menuTop);
}

void LabUI::drawBLESpamMenu() {
  display->clear();
  display->drawHeader("BLE Spam");
  Adafruit_SSD1306* d = display->getDriver();
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);
  d->setCursor(2, 12);
  if (ble_spam_is_running()) d->print(F("BLE Spam: ON"));
  else d->print(F("BLE Spam: OFF"));
  // Draw menu items below subtitle
  int maxVisible = 4;
  int startY = 24;
  for (int i = 0; i < maxVisible; i++) {
    int itemIdx = bleTop + i;
    if (itemIdx >= BLE_SPAM_COUNT) break;
    int y = startY + (i * 10);
    if (itemIdx == bleSel) {
      d->fillRect(0, y, 128, 10, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    d->setCursor(2, y + 1);
    d->print(bleSpamItems[itemIdx]);
  }
  d->display();
}

void LabUI::drawSpamRunning() {
  const char* lines[] = {
    bleSpamItems[bleSel],
    "BLE Spam: ON",
    "Spamming...",
    "SELECT to stop"
  };
  display->drawInfoPage("BLE Spam", lines, 4);
}

void LabUI::drawWiFiAttackMenu() {
  // If we recently set a target, show confirmation briefly
  if (showTargetConfirm) {
    if (millis() < targetConfirmExpiresMs) {
      char lineBuf[34];
      snprintf(lineBuf, sizeof(lineBuf), "Target Set:");
      const char* lines[] = { lineBuf, selectedAP.ssid, "", "Back" };
      display->drawInfoPage("WiFi Target", lines, 4);
      return;
    } else {
      showTargetConfirm = false;
    }
  }

  // Draw header and subtitle with current target
  display->clear();
  display->drawHeader("WiFi Attacks");
  Adafruit_SSD1306* d = display->getDriver();
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);
  d->setCursor(2, 12);
  if (selectedAP.valid) d->print(selectedAP.ssid);
  else d->print(F("No target"));

  // Draw menu items below subtitle, reserve one line for subtitle
  int maxVisible = 4;
  int startY = 24;

  for (int i = 0; i < maxVisible; i++) {
    int itemIdx = wifiTop + i;
    if (itemIdx >= WIFI_ATTACK_COUNT) break;
    int y = startY + (i * 10);
    if (itemIdx == wifiSel) {
      d->fillRect(0, y, 128, 10, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    d->setCursor(2, y + 1);
    d->print(wifiAttackItems[itemIdx]);
  }
  d->display();
}

void LabUI::drawWiFiAttackRunning() {
  // For Deauth Attack (wifiSel == 3), show channel and elapsed time
  if (wifiSel == 3) {
    int channel = deauth_attack_get_current_channel();
    uint32_t elapsed = deauth_attack_get_elapsed_ms();
    uint32_t elapsed_sec = elapsed / 1000;
    
    display->clear();
    display->drawHeader("WiFi Attacks");
    Adafruit_SSD1306* d = display->getDriver();
    d->setTextSize(1);
    d->setTextColor(SSD1306_WHITE);
    
    // Status line
    d->setCursor(2, 12);
    d->print("Deauth Attack: ON");
    
    // Channel and time
    d->setCursor(2, 24);
    char buf[32];
    if (channel > 0) {
      snprintf(buf, sizeof(buf), "Channel: %d", channel);
      d->print(buf);
    } else {
      d->print("Scanning...");
    }
    
    d->setCursor(2, 34);
    snprintf(buf, sizeof(buf), "Elapsed: %lus", elapsed_sec);
    d->print(buf);
    
    // PMF warning after 3 seconds
    if (elapsed >= 3000) {
      d->setCursor(2, 44);
      d->setTextSize(1);
      d->setTextColor(SSD1306_WHITE);
      d->print("No effect: PMF?");
    }
    
    // Stop instruction
    d->setCursor(2, 54);
    d->print("SELECT to stop");
    
    d->display();
  } else {
    // Generic display for other attacks
    const char* lines[] = {
      wifiAttackItems[wifiSel],
      "WiFi Attack: ON",
      "Attacking...",
      "SELECT to stop"
    };
    display->drawInfoPage("WiFi Attacks", lines, 4);
  }
}
