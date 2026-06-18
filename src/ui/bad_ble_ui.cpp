#include "bad_ble_ui.h"
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Constructor / Destructor
// ============================================================================

BadBleUI::BadBleUI(DisplayWrapper* disp)
  : display(disp), currentPage(BadBlePage::ModeMenu),
    modeSel(0), modeTop(0),
    selectedPayload(-1), payloadExecuted(false), doneTimerMs(0),
    textInput(nullptr), customTextReady(false) {
  customTextBuf[0] = '\0';
}

BadBleUI::~BadBleUI() {
  stopHID();
  if (textInput) {
    delete textInput;
    textInput = nullptr;
  }
}

// ============================================================================
// enter() — Called when navigating into BadBLE
// ============================================================================

void BadBleUI::enter() {
  currentPage = BadBlePage::ModeMenu;
  modeSel = 0;
  modeTop = 0;
  selectedPayload = -1;
  payloadExecuted = false;
  customTextReady = false;
  drawModeMenu();
}

// ============================================================================
// stopHID() — External call to shut down HID on exit
// ============================================================================

void BadBleUI::stopHID() {
  if (hid.isRunning()) {
    hid.stop();
  }
}

// ============================================================================
// Helper: Get payload name by combined index
// ============================================================================

const char* BadBleUI::getPayloadName(int idx) {
  if (idx < BADBLE_PAYLOAD_COUNT) {
    return BADBLE_PAYLOAD_NAMES[idx];
  }
  int mouseIdx = idx - BADBLE_PAYLOAD_COUNT;
  if (mouseIdx < BADBLE_MOUSE_COUNT) {
    return BADBLE_MOUSE_NAMES[mouseIdx];
  }
  return "???";
}

// ============================================================================
// Drawing
// ============================================================================

void BadBleUI::drawModeMenu() {
  display->clear();
  display->drawHeader("BadBLE");
  Adafruit_SSD1306* d = display->getDriver();
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);

  // Subtitle: Keyboard or Mouse section indicator
  d->setCursor(2, 12);
  if (modeSel < BADBLE_PAYLOAD_COUNT) {
    d->print(F("Keyboard Payloads"));
  } else {
    d->print(F("Mouse Actions"));
  }

  // Scrollable list — 4 visible items
  int maxVisible = 4;
  int startY = 24;

  for (int i = 0; i < maxVisible; i++) {
    int idx = modeTop + i;
    if (idx >= BADBLE_TOTAL_ITEMS) break;
    int y = startY + (i * 10);

    if (idx == modeSel) {
      d->fillRect(0, y, 128, 10, SSD1306_WHITE);
      d->setTextColor(SSD1306_BLACK);
    } else {
      d->setTextColor(SSD1306_WHITE);
    }
    d->setCursor(2, y + 1);

    // Prefix: [K] for keyboard, [M] for mouse
    if (idx < BADBLE_PAYLOAD_COUNT) {
      d->print(F("K "));
    } else {
      d->print(F("M "));
    }
    d->print(getPayloadName(idx));
  }

  d->display();
}

void BadBleUI::drawConnectRun() {
  display->clear();
  display->drawHeader("BadBLE");
  Adafruit_SSD1306* d = display->getDriver();
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);

  if (payloadExecuted) {
    // Done screen
    d->setCursor(2, 14);
    d->print(F("Payload Sent!"));

    d->setCursor(2, 28);
    d->print(getPayloadName(selectedPayload));

    d->setCursor(2, 42);
    d->print(F("Done!"));

    d->setCursor(2, 54);
    d->print(F("BACK to return"));
  } else if (hid.isConnected()) {
    // Connected — ready to run
    d->setCursor(2, 14);
    d->print(F("Host Connected!"));

    d->setCursor(2, 28);
    d->print(F("Payload:"));
    d->setCursor(2, 38);
    d->print(getPayloadName(selectedPayload));

    d->setCursor(2, 54);
    d->print(F("SEL=run  BACK=exit"));
  } else {
    // Advertising / waiting
    d->setCursor(2, 14);
    d->print(F("Advertising as:"));
    d->setCursor(2, 24);
    d->print(F("'BT Keyboard'"));

    d->setCursor(2, 38);
    d->print(F("Waiting for host..."));

    d->setCursor(2, 54);
    d->print(F("BACK to cancel"));
  }

  d->display();
}

// ============================================================================
// Payload execution
// ============================================================================

void BadBleUI::executeMouseAction(int mouseIdx) {
  switch (mouseIdx) {
    case 0: hid.moveMouse(0, -40);  break; // Move Up
    case 1: hid.moveMouse(0, 40);   break; // Move Down
    case 2: hid.moveMouse(-40, 0);  break; // Move Left
    case 3: hid.moveMouse(40, 0);   break; // Move Right
    case 4: hid.clickLeft();         break; // Left Click
    case 5: hid.clickRight();        break; // Right Click
    case 6: hid.scroll(5);           break; // Scroll Up
    case 7: hid.scroll(-5);          break; // Scroll Down
    default: break;
  }
}

void BadBleUI::executePayload(int idx) {
  if (idx < BADBLE_PAYLOAD_COUNT) {
    // Keyboard payload
    switch (idx) {
      case 0: payload_alt_f4(&hid);         break;
      case 1: payload_open_run(&hid);        break;
      case 2: payload_open_powershell(&hid); break;
      case 3: payload_ipconfig(&hid);        break;
      case 4: payload_sysinfo(&hid);         break;
      case 5: payload_ip_and_sys(&hid);      break;
      case 6: payload_close_all(&hid);       break;
      case 7: // Custom Text
        if (customTextReady && customTextBuf[0] != '\0') {
          payload_custom(&hid, customTextBuf);
        }
        break;
    }
  } else {
    // Mouse action
    int mouseIdx = idx - BADBLE_PAYLOAD_COUNT;
    executeMouseAction(mouseIdx);
  }
}

// ============================================================================
// update() — Main event loop
// ============================================================================

bool BadBleUI::update(ButtonEvent btn) {
  switch (currentPage) {

    // ---- Mode Menu ----
    case BadBlePage::ModeMenu: {
      if (btn == BTN_BACK) {
        stopHID();
        return false; // Exit BadBLE
      }
      if (btn == BTN_DOWN && modeSel < BADBLE_TOTAL_ITEMS - 1) {
        modeSel++;
        if (modeSel >= modeTop + 4) modeTop++;
        drawModeMenu();
      } else if (btn == BTN_UP && modeSel > 0) {
        modeSel--;
        if (modeSel < modeTop) modeTop--;
        drawModeMenu();
      } else if (btn == BTN_SELECT) {
        selectedPayload = modeSel;
        payloadExecuted = false;

        // If "Custom Text" selected, go to text input first
        if (selectedPayload == 7) { // Custom Text index
          customTextBuf[0] = '\0';
          textInput = new TextInputUI(display, customTextBuf, sizeof(customTextBuf), "BadBLE Text", false);
          textInput->enter();
          currentPage = BadBlePage::CustomText;
          return true;
        }

        // Start HID if not already running
        if (!hid.isRunning()) {
          hid.begin();
        }
        currentPage = BadBlePage::ConnectRun;
        drawConnectRun();
      }
      break;
    }

    // ---- Connect / Run ----
    case BadBlePage::ConnectRun: {
      if (btn == BTN_BACK) {
        stopHID();
        currentPage = BadBlePage::ModeMenu;
        drawModeMenu();
        return true;
      }

      if (hid.isConnected() && !payloadExecuted) {
        if (btn == BTN_SELECT) {
          // Show "Sending..." briefly
          display->clear();
          display->drawHeader("BadBLE");
          Adafruit_SSD1306* d = display->getDriver();
          d->setTextSize(1);
          d->setTextColor(SSD1306_WHITE);
          d->setCursor(2, 28);
          d->print(F("Sending..."));
          d->display();

          executePayload(selectedPayload);

          payloadExecuted = true;
          doneTimerMs = millis() + 2000;
          drawConnectRun();
        }
      }

      // Auto-dismiss done screen after 2 seconds (on any button or timeout)
      if (payloadExecuted && btn != BTN_NONE) {
        stopHID();
        currentPage = BadBlePage::ModeMenu;
        drawModeMenu();
        return true;
      }

      // Refresh display on each update to show connection status changes
      if (btn == BTN_NONE) {
        drawConnectRun();
      }
      break;
    }

    // ---- Custom Text Input ----
    case BadBlePage::CustomText: {
      if (textInput) {
        bool stillActive = textInput->update(btn);
        if (!stillActive) {
          if (!textInput->wasCancelled() && customTextBuf[0] != '\0') {
            customTextReady = true;
            // Start HID and go to connect screen
            if (!hid.isRunning()) {
              hid.begin();
            }
            currentPage = BadBlePage::ConnectRun;
            drawConnectRun();
          } else {
            // Cancelled or empty — back to menu
            customTextReady = false;
            currentPage = BadBlePage::ModeMenu;
            drawModeMenu();
          }
          delete textInput;
          textInput = nullptr;
        }
      }
      break;
    }
  }

  return true;
}
