/*
 * NAVIGATION ARCHITECTURE — Two-Tier System
 *
 * GLOBAL tier (NavStack):
 *   Tracks transitions between top-level Page enum values.
 *   BACK button calls navStack.pop() when on a global Page.
 *   Long-press BACK calls navStack.clear() and returns to Page::Main.
 *
 * LOCAL tier (WifiPage / BlePage / AppsPage):
 *   Each module manages its own internal sub-page state.
 *   BACK button is handled within the module (does not touch navStack).
 *   Reaching the module's root local page + pressing BACK transitions
 *   to the parent global Page via navStack.pop().
 *
 * Rule: Never push a local sub-page value onto the global NavStack.
 *       Never let a global Page transition bypass the NavStack.
 */

#include "mainwindow.h"
#include "version.h"
#include <esp_wifi.h>
#include "ir_utils.h"

MainWindow window;
NavStack navStack;
RadioState currentRadioState = RadioState::RADIO_OFF;

MainWindow::MainWindow() {
  currentPage = Page::Boot;
  mainSelectedIndex = 0;
  mainTopIndex = 0;
  stateStartTime = 0;
  globalLastActivityMs = 0;
  settingsUI = nullptr;
  diagnosticsUI = nullptr;
  wifiUI = nullptr;
  bleUI = nullptr;
  appsUI = nullptr;
  gamesUI = nullptr;
  irUI = nullptr;
  labUI = nullptr;
}

MainWindow::~MainWindow() {
  if (settingsUI) delete settingsUI;
  if (diagnosticsUI) delete diagnosticsUI;
  if (wifiUI) delete wifiUI;
  if (bleUI) delete bleUI;
  if (appsUI) delete appsUI;
  if (gamesUI) delete gamesUI;
  if (irUI) delete irUI;
  if (labUI) delete labUI;
}

void MainWindow::begin() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("[BOOT] AINZOS starting");
  Serial.println("[BOOT] Serial OK");
  
  buttons.begin();
  settings.begin();
  
  // Custom boot logo and I2C check
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.beginTransmission(SCREEN_ADDRESS);
  bool oledFound = (Wire.endTransmission() == 0);
  
  if (!oledFound) {
    Serial.println("[BOOT] OLED NOT FOUND on I2C!");
  } else {
    Serial.println("[BOOT] OLED found on I2C");
  }

  if (!display.begin()) {
    // Probed OK but failed constructor, fallback
  } else {
    display.setContrast(settings.contrast);
  }
  
  settingsUI = new SettingsUI(&display, &settings);
  diagnosticsUI = new Diagnostics(&display);
  wifiUI = new WifiUI(&display);
  wifiUI->begin();
  bleUI = new BleUI(&display, &settings);
  bleUI->begin();
  appsUI = new AppsUI(&display, wifiUI->getScanner(), bleUI->getScanner());
  gamesUI = new GamesUI(&display, &settings);
  irUI = new IrUI(&display);
  labUI = new LabUI(&display);
  ir_manager_init();
  
  stateStartTime = millis();
  globalLastActivityMs = millis();
  
  // Enter Boot Clock if enabled
  if (settings.cs.enabled && settings.cs.bootClockEnabled) {
    triggerClockSaverMode(2); // 2 = CS_ENTRY_BOOT_CLOCK
  }
}

void MainWindow::update() {
  ButtonEvent btn = buttons.read();
  uint32_t now = millis();
  
  // Auto-dim logic
  static uint32_t lastInteractMs = 0;
  static bool isDimmed = false;
  
  if (btn != BTN_NONE) {
    lastInteractMs = now;
    globalLastActivityMs = now;
    if (isDimmed) {
      display.setContrast(settings.contrast);
      isDimmed = false;
    }
  }
  
  uint32_t dimTimeoutMs = settings.dim_time * 1000;
  if (settings.dim_time != 0 && (now - lastInteractMs > dimTimeoutMs)) {
    if (!isDimmed) {
      display.setContrast(0x10); // dim to ~6%
      isDimmed = true;
    }
  }

  // Call the non-blocking probe request monitor channel hopper
  extern void updateProbeHopper();
  updateProbeHopper();

  // Global Idle Check for Clock Saver
  if (!clockSaverActive && settings.cs.enabled && settings.cs.autoStartOnIdle && 
      settings.cs.globalIdleTimeoutMs > 0 && 
      (now - globalLastActivityMs >= settings.cs.globalIdleTimeoutMs)) {
    if (canAutoStartClockSaver()) {
      Serial.println("[ClockSaver] Global idle reached, auto-starting");
      triggerClockSaverMode(1); // 1 = CS_ENTRY_GLOBAL_IDLE
    }
  }

  // Global tier navigation logic:
  if (btn == BTN_BACK_LONG) {
    navStack.clear();
    changePage(Page::Main);
    return;
  }

  // Check if we are on a global leaf Page that should pop directly back to parent
  bool isGlobalLeafPage = (currentPage == Page::WiFiDetails || 
                           currentPage == Page::WiFiProbeMonitor ||
                           currentPage == Page::WiFiChannelOccupancy ||
                           currentPage == Page::BLEDetails ||
                           (currentPage == Page::BLEPacketInspector && appsUI && appsUI->getInspectDeviceIdx() == 255) ||
                           currentPage == Page::DeviceIdentity ||
                           currentPage == Page::About ||
                           currentPage == Page::NotImplemented);

  if (isGlobalLeafPage && btn == BTN_BACK) {
    changePage(navStack.pop());
    return;
  }

  // Page specific updates
  switch (currentPage) {
    case Page::Boot: handleBoot(btn); break;
    case Page::Main: handleMain(btn); break;
    case Page::About: handleAbout(btn); break;
    case Page::WiFiMenu:
    case Page::WiFiDetails:
    case Page::WiFiProbeMonitor:
      if (!wifiUI->update(btn)) changePage(navStack.pop());
      break;
    case Page::BLEMenu:
    case Page::BLEDetails:
      if (!bleUI->update(btn)) changePage(navStack.pop());
      break;
    case Page::AppsMenu:
    case Page::WiFiChannelOccupancy:
    case Page::BLEPacketInspector:
    case Page::DeviceIdentity:
      if (!appsUI->update(btn)) changePage(navStack.pop());
      break;
    case Page::GamesMenu:
      if (!gamesUI->update(btn)) changePage(navStack.pop());
      break;
    case Page::Diagnostics:
      if (!diagnosticsUI->update(btn)) changePage(navStack.pop());
      break;
    case Page::Settings:
      if (!settingsUI->update(btn)) changePage(navStack.pop());
      break;
    case Page::IRMenu:
      if (!irUI->update(btn)) changePage(navStack.pop());
      break;
    case Page::LabToolsMenu:
      if (!labUI->update(btn)) changePage(navStack.pop());
      break;
    case Page::NotImplemented:
    default: handleNotImplemented(btn); break;
  }
}

void MainWindow::changePage(Page newPage) {
  // Stop Wi-Fi promiscuous if leaving Probe Monitor
  if (currentPage == Page::WiFiProbeMonitor) {
    extern void stopProbeMonitor();
    stopProbeMonitor();
  }

  // Radio coexistence checks before changing page
  bool enteringWiFi = (newPage == Page::WiFiMenu || newPage == Page::WiFiDetails || newPage == Page::WiFiProbeMonitor || newPage == Page::WiFiChannelOccupancy);
  bool enteringBLE = (newPage == Page::BLEMenu || newPage == Page::BLEDetails || newPage == Page::BLEPacketInspector);
  bool enteringLabTools = (newPage == Page::LabToolsMenu);

  if (enteringWiFi && currentRadioState != RadioState::RADIO_WIFI) {
    if (currentRadioState == RadioState::RADIO_BLE) {
      NimBLEDevice::deinit(true);
    }
    currentRadioState = RadioState::RADIO_WIFI;
  } else if (enteringLabTools) {
    if (currentRadioState == RadioState::RADIO_BLE) {
      NimBLEDevice::deinit(true);
    }
    if (currentRadioState == RadioState::RADIO_WIFI) {
      esp_wifi_set_promiscuous(false);
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    }
    currentRadioState = RadioState::RADIO_OFF;
  } else if (enteringBLE && currentRadioState != RadioState::RADIO_BLE) {
    if (currentRadioState == RadioState::RADIO_WIFI) {
      // Deactivate Wi-Fi sniffer/promiscuous and modes
      esp_wifi_set_promiscuous(false);
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    }
    currentRadioState = RadioState::RADIO_BLE;
    if (bleUI) bleUI->begin(); // Re-initialize BLE stack
  }

  currentPage = newPage;
  stateStartTime = millis();
  
  if (currentPage == Page::Main) {
    display.drawMenu("AINZOS", mainMenuItems, MAIN_MENU_COUNT, mainSelectedIndex, mainTopIndex);
  } else if (currentPage == Page::WiFiMenu) {
    if (wifiUI && wifiUI->getCurrentPage() == WifiPage::ResultsList) {
      wifiUI->enter(WifiPage::ResultsList);
    } else {
      wifiUI->enter(WifiPage::Menu);
    }
  } else if (currentPage == Page::WiFiDetails) {
    wifiUI->enter(WifiPage::Details);
  } else if (currentPage == Page::WiFiProbeMonitor) {
    wifiUI->enter(WifiPage::ProbeMonitor);
  } else if (currentPage == Page::BLEMenu) {
    if (bleUI && bleUI->getCurrentPage() == BlePage::ResultsList) {
      bleUI->enter(BlePage::ResultsList);
    } else {
      bleUI->enter(BlePage::Menu);
    }
  } else if (currentPage == Page::BLEDetails) {
    bleUI->enter(BlePage::Details);
  } else if (currentPage == Page::AppsMenu) {
    appsUI->enter();
  } else if (currentPage == Page::WiFiChannelOccupancy) {
    appsUI->enter(AppsPage::WiFiChannelOccupancy);
  } else if (currentPage == Page::BLEPacketInspector) {
    appsUI->enter(AppsPage::BleInspector);
  } else if (currentPage == Page::DeviceIdentity) {
    appsUI->enter(AppsPage::DeviceIdentity);
  } else if (currentPage == Page::GamesMenu) {
    gamesUI->enter();
  } else if (currentPage == Page::Diagnostics) {
    diagnosticsUI->enter();
  } else if (currentPage == Page::Settings) {
    settingsUI->enter();
  } else if (currentPage == Page::IRMenu) {
    irUI->enter();
  } else if (currentPage == Page::LabToolsMenu) {
    labUI->enter();
  } else if (currentPage == Page::About) {
    const char* lines[] = {
      "AINZOS",
      AINZOS_VERSION,
      "ESP32-C3 SuperMini",
      "Lab Testing Only",
      "Author: AinZ"
    };
    display.drawInfoPage("About", lines, 5);
  } else if (currentPage == Page::NotImplemented) {
    const char* lines[] = {
      "Feature not",
      "implemented yet.",
      "",
      "Press BACK"
    };
    display.drawInfoPage("Error", lines, 4);
  }
}

void MainWindow::handleBoot(ButtonEvent btn) {
  uint32_t elapsed = millis() - stateStartTime;
  
  display.clear();
  auto d = display.getDriver();
  
  // Outer frame
  d->drawRect(0, 0, 128, 64, SSD1306_WHITE);
  
  // Header
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);
  d->setCursor(35, 6);
  d->print(F("A I N Z O S"));
  d->drawFastHLine(6, 16, 116, SSD1306_WHITE);
  
  // Terminal typing simulation
  if (elapsed >= 0) {
    d->setCursor(8, 20);
    d->print(F("Version: "));
    d->print(F(AINZOS_VERSION));
  }
  
  if (elapsed >= 400) {
    uint8_t wifiMac[6];
    esp_read_mac(wifiMac, ESP_MAC_WIFI_STA);
    char macStr[20];
    snprintf(macStr, sizeof(macStr), "MAC:%02X:%02X:%02X..", wifiMac[0], wifiMac[1], wifiMac[2]);
    d->setCursor(8, 30);
    d->print(macStr);
  }
  
  if (elapsed >= 800) {
    d->setCursor(8, 40);
    d->print(F("Flash: 4MB"));
  }
  
  if (elapsed >= 1200) {
    d->setCursor(8, 50);
    d->print(F("Booting..."));
  }
  
  // Progress Bar
  d->drawRect(72, 51, 48, 7, SSD1306_WHITE);
  float pct = (float)elapsed / 2000.0f;
  if (pct > 1.0f) pct = 1.0f;
  int w = (int)(pct * 44.0f);
  if (w > 0) {
    d->fillRect(74, 53, w, 3, SSD1306_WHITE);
  }
  
  if (elapsed >= 2000) {
    // Show [OK] overlay or print at end
    d->fillRect(72, 51, 48, 7, SSD1306_BLACK); // Clear progress bar area
    d->setCursor(84, 50);
    d->print(F("[OK]"));
  }
  
  // Cursor blinking on active printing line
  int cursorY = 20;
  if (elapsed >= 1200) cursorY = 50;
  else if (elapsed >= 800) cursorY = 40;
  else if (elapsed >= 400) cursorY = 30;
  
  int cursorX = 8 + 15 * 6; // Version line length (roughly)
  if (cursorY == 30) cursorX = 8 + 14 * 6;
  if (cursorY == 40) cursorX = 8 + 10 * 6;
  if (cursorY == 50) cursorX = 8 + 10 * 6;
  
  // Draw blinking block cursor (only if boot not done)
  if (elapsed < 2000 && (elapsed / 250) % 2 == 0) {
    d->fillRect(cursorX, cursorY, 5, 7, SSD1306_WHITE);
  }
  
  d->display();
  
  if (elapsed > 2400 || btn != BTN_NONE) {
    changePage(Page::Main);
  }
}

void MainWindow::handleMain(ButtonEvent btn) {
  bool redraw = false;
  
  if (btn == BTN_DOWN) {
    if (mainSelectedIndex < MAIN_MENU_COUNT - 1) {
      mainSelectedIndex++;
      if (mainSelectedIndex >= mainTopIndex + 5) {
        mainTopIndex++;
      }
      redraw = true;
    }
  } else if (btn == BTN_UP) {
    if (mainSelectedIndex > 0) {
      mainSelectedIndex--;
      if (mainSelectedIndex < mainTopIndex) {
        mainTopIndex--;
      }
      redraw = true;
    }
  } else if (btn == BTN_SELECT) {
    navStack.push(currentPage); // Push parent page before transitioning
    
    switch (mainSelectedIndex) {
      case 0: changePage(Page::WiFiMenu); break;
      case 1: changePage(Page::BLEMenu); break;
      case 2: changePage(Page::LabToolsMenu); break; // Lab Tools
      case 3: changePage(Page::IRMenu); break;         // IR Tools
      case 4: changePage(Page::AppsMenu); break;
      case 5: changePage(Page::GamesMenu); break;
      case 6: changePage(Page::Diagnostics); break;
      case 7: changePage(Page::Settings); break;
      case 8: changePage(Page::About); break;
      default: changePage(Page::NotImplemented); break;
    }
    return;
  } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    triggerClockSaverMode(1); // Manually trigger Clock/Pet mode from Main Menu
    return;
  }
  
  if (redraw) {
    display.drawMenu("AINZOS", mainMenuItems, MAIN_MENU_COUNT, mainSelectedIndex, mainTopIndex);
  }
}

void MainWindow::handleAbout(ButtonEvent btn) {
}

void MainWindow::handleNotImplemented(ButtonEvent btn) {
}

bool MainWindow::canAutoStartClockSaver() {
  // Enforce 2000ms cooldown after explicit exit to prevent instant re-entry
  if (millis() - lastClockSaverExitMs < 2000) {
    return false;
  }
  
  if (currentPage == Page::Main ||
      currentPage == Page::AppsMenu ||
      currentPage == Page::GamesMenu ||
      currentPage == Page::Settings ||
      currentPage == Page::About ||
      currentPage == Page::NotImplemented ||
      currentPage == Page::Diagnostics ||
      currentPage == Page::IRMenu ||
      currentPage == Page::WiFiMenu ||
      currentPage == Page::BLEMenu) {
      
      if (currentPage == Page::WiFiMenu && wifiUI && wifiUI->getCurrentPage() == WifiPage::Scanning) return false;
      if (currentPage == Page::BLEMenu && bleUI && bleUI->getCurrentPage() == BlePage::Scanning) return false;
      
      return true;
  }
  return false;
}

void MainWindow::markClockSaverExited() {
  clockSaverActive = false;
  globalLastActivityMs = millis();
  lastClockSaverExitMs = millis();
  changePage(Page::Main);
}

void MainWindow::triggerClockSaverMode(int mode) {
  if (currentPage != Page::AppsMenu) {
    navStack.push(currentPage);
    currentPage = Page::AppsMenu;
  }
  appsUI->enter(AppsPage::ClockSaver);
  extern void startClockSaver(int mode);
  startClockSaver(mode);
}
