#include "ir_ui.h"
#include "mainwindow.h"
#include "universal_ir.h"
#include <string.h>

// Menu item label lists
static const char* irMainItems[] = {
  "Universal",
  "TV Remote",
  "Other Devices"
};

static const char* irUniversalItems[] = {
  "TV Power Blast",
  "TV Mute Blast",
  "TV OK Blast",
  "STB Power Blast",
  "Proj Power Blast",
  "Sbar Power Blast",
  "AC Power Blast"
};

static const char* irTvBrandItems[] = {
  "Samsung",
  "LG",
  "Sony",
  "Panasonic",
  "Philips"
};

static const char* irOtherDeviceItems[] = {
  "Set-top Box",
  "Projector",
  "Soundbar",
  "AC"
};

static const char* irTvKeyItems[] = {
  "Power",
  "Mute",
  "OK",
  "Vol+",
  "Vol-",
  "Up",
  "Down"
};

static const char* irStbKeyItems[] = {
  "Power",
  "OK",
  "Up",
  "Down"
};

static const char* irProjKeyItems[] = {
  "Power",
  "Source",
  "OK"
};

static const char* irSbarKeyItems[] = {
  "Power",
  "Mute",
  "Vol+",
  "Vol-"
};

static const char* irAcKeyItems[] = {
  "Power ON",
  "Power OFF"
};

// Mini state stack to preserve menu positions
static int indexStack[4];
static int topStack[4];
static uint8_t stackPtr = 0;

static void pushMenuState(IRPageState& state, IRPageState newState, int& currentSel, int& currentTop) {
  if (stackPtr < 4) {
    indexStack[stackPtr] = currentSel;
    topStack[stackPtr] = currentTop;
    stackPtr++;
  }
  state = newState;
  currentSel = 0;
  currentTop = 0;
}

static void popMenuState(IRPageState& state, IRPageState parentState, int& currentSel, int& currentTop) {
  state = parentState;
  if (stackPtr > 0) {
    stackPtr--;
    currentSel = indexStack[stackPtr];
    currentTop = topStack[stackPtr];
  } else {
    currentSel = 0;
    currentTop = 0;
  }
}

IrUI::IrUI(DisplayWrapper* disp) : display(disp) {
  pageState = IR_PAGE_MAIN;
  menuSelectedIndex = 0;
  menuTopIndex = 0;
  blastCodes = nullptr;
  blastCount = 0;
  blastIndex = 0;
  lastBlastMs = 0;
  blastTitle = nullptr;
  isUniversalPowerBlast = false;
  statusText = nullptr;
  statusStartMs = 0;
  stackPtr = 0;
}

void IrUI::enter() {
  pageState = IR_PAGE_MAIN;
  menuSelectedIndex = 0;
  menuTopIndex = 0;
  statusText = nullptr;
  stackPtr = 0;
  irBegin(); // Initialize IR sending pin
  drawActivePage();
}

int getActiveItemCount(IRPageState state, IRDeviceType device) {
  switch (state) {
    case IR_PAGE_MAIN: return 3;
    case IR_PAGE_UNIVERSAL: return HIZMOS_ENABLE_IR_AC_CODES ? 7 : 6;
    case IR_PAGE_TV_BRANDS: return 5;
    case IR_PAGE_TV_KEYS: return 7;
    case IR_PAGE_OTHER_DEVICES: return HIZMOS_ENABLE_IR_AC_CODES ? 4 : 3;
    case IR_PAGE_OTHER_KEYS:
      if (device == IR_DEV_STB) return 4;
      if (device == IR_DEV_PROJECTOR) return 3;
      if (device == IR_DEV_SOUNDBAR) return 4;
      if (device == IR_DEV_AC) return 2;
      return 0;
    default: return 0;
  }
}

bool IrUI::update(ButtonEvent btn) {
  uint32_t now = millis();
  
  // Handle single feedback status screen ("Sent!" or "No code")
  if (statusText) {
    if (now - statusStartMs >= 600 || btn == BTN_BACK || btn == BTN_SELECT) {
      statusText = nullptr;
      drawActivePage();
    }
    return true;
  }
  
  // Non-blocking universal blast logic
  if (pageState == IR_PAGE_BLASTING) {
    if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      statusText = "Stopped";
      statusStartMs = now;
      popMenuState(pageState, IR_PAGE_UNIVERSAL, menuSelectedIndex, menuTopIndex);
      drawActivePage();
      return true;
    }
    
    if (now - lastBlastMs >= 200) {
      lastBlastMs = now;
      int total = isUniversalPowerBlast ? UNIVERSAL_IR_SIGNAL_COUNT : blastCount;
      
      if (blastIndex < total) {
        bool ok = false;
        if (isUniversalPowerBlast) {
          const universal_ir_signal_t* sig = &universal_ir_signals[blastIndex];
          ok = irSendUniversalSignal(sig);
        } else {
          IRCodeEntry code;
          memcpy_P(&code, &blastCodes[blastIndex], sizeof(IRCodeEntry));
          ok = irSendCode(code);
        }
        
        blastIndex++;
        drawActivePage();
      } else {
        statusText = "Sent!";
        statusStartMs = now;
        popMenuState(pageState, IR_PAGE_UNIVERSAL, menuSelectedIndex, menuTopIndex);
        drawActivePage();
      }
    }
    return true;
  }
  
  // Normal page state updates
  int count = getActiveItemCount(pageState, selectedDevice);
  bool redraw = false;
  
  if (btn == BTN_DOWN) {
    if (menuSelectedIndex < count - 1) {
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
    switch (pageState) {
      case IR_PAGE_MAIN:
        if (menuSelectedIndex == 0) pushMenuState(pageState, IR_PAGE_UNIVERSAL, menuSelectedIndex, menuTopIndex);
        else if (menuSelectedIndex == 1) pushMenuState(pageState, IR_PAGE_TV_BRANDS, menuSelectedIndex, menuTopIndex);
        else if (menuSelectedIndex == 2) pushMenuState(pageState, IR_PAGE_OTHER_DEVICES, menuSelectedIndex, menuTopIndex);
        redraw = true;
        break;
        
      case IR_PAGE_UNIVERSAL:
        if (menuSelectedIndex == 0) {
          startUniversalPowerBlast();
        } else if (menuSelectedIndex == 1) {
          startUniversalBlast(tvMuteBlastCodes, 5, "TV Mute Blast");
        } else if (menuSelectedIndex == 2) {
          startUniversalBlast(tvOkBlastCodes, 5, "TV OK Blast");
        } else if (menuSelectedIndex == 3) {
          startUniversalBlast(stbPowerBlastCodes, 3, "STB Power Blast");
        } else if (menuSelectedIndex == 4) {
          startUniversalBlast(projectorPowerBlastCodes, 3, "Proj Power Blast");
        } else if (menuSelectedIndex == 5) {
          startUniversalBlast(soundbarPowerBlastCodes, 3, "Sbar Power Blast");
        } else if (menuSelectedIndex == 6) {
#if HIZMOS_ENABLE_IR_AC_CODES
          startUniversalBlast(acPowerBlastCodes, 2, "AC Power Blast");
#endif
        }
        redraw = true;
        break;
        
      case IR_PAGE_TV_BRANDS:
        selectedBrand = menuSelectedIndex;
        pushMenuState(pageState, IR_PAGE_TV_KEYS, menuSelectedIndex, menuTopIndex);
        redraw = true;
        break;
        
      case IR_PAGE_TV_KEYS:
        sendSingleKey((IRKey)menuSelectedIndex);
        redraw = true;
        break;
        
      case IR_PAGE_OTHER_DEVICES:
        selectedDevice = (IRDeviceType)menuSelectedIndex;
        pushMenuState(pageState, IR_PAGE_OTHER_KEYS, menuSelectedIndex, menuTopIndex);
        redraw = true;
        break;
        
      case IR_PAGE_OTHER_KEYS:
        if (selectedDevice == IR_DEV_STB) {
          if (menuSelectedIndex == 0) sendSingleKey(IR_KEY_POWER);
          else if (menuSelectedIndex == 1) sendSingleKey(IR_KEY_OK);
          else if (menuSelectedIndex == 2) sendSingleKey(IR_KEY_UP);
          else if (menuSelectedIndex == 3) sendSingleKey(IR_KEY_DOWN);
        } else if (selectedDevice == IR_DEV_PROJECTOR) {
          if (menuSelectedIndex == 0) sendSingleKey(IR_KEY_POWER);
          else if (menuSelectedIndex == 1) sendSingleKey(IR_KEY_SOURCE);
          else if (menuSelectedIndex == 2) sendSingleKey(IR_KEY_OK);
        } else if (selectedDevice == IR_DEV_SOUNDBAR) {
          if (menuSelectedIndex == 0) sendSingleKey(IR_KEY_POWER);
          else if (menuSelectedIndex == 1) sendSingleKey(IR_KEY_MUTE);
          else if (menuSelectedIndex == 2) sendSingleKey(IR_KEY_VOL_UP);
          else if (menuSelectedIndex == 3) sendSingleKey(IR_KEY_VOL_DOWN);
        } else if (selectedDevice == IR_DEV_AC) {
          if (menuSelectedIndex == 0) sendSingleKey(IR_KEY_AC_POWER_ON);
          else if (menuSelectedIndex == 1) sendSingleKey(IR_KEY_AC_POWER_OFF);
        }
        redraw = true;
        break;
        
      default:
        break;
    }
  } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    if (pageState == IR_PAGE_MAIN) {
      return false; // Exit IR remote back to MainWindow
    }
    
    if (pageState == IR_PAGE_TV_KEYS) popMenuState(pageState, IR_PAGE_TV_BRANDS, menuSelectedIndex, menuTopIndex);
    else if (pageState == IR_PAGE_TV_BRANDS) popMenuState(pageState, IR_PAGE_MAIN, menuSelectedIndex, menuTopIndex);
    else if (pageState == IR_PAGE_UNIVERSAL) popMenuState(pageState, IR_PAGE_MAIN, menuSelectedIndex, menuTopIndex);
    else if (pageState == IR_PAGE_OTHER_DEVICES) popMenuState(pageState, IR_PAGE_MAIN, menuSelectedIndex, menuTopIndex);
    else if (pageState == IR_PAGE_OTHER_KEYS) popMenuState(pageState, IR_PAGE_OTHER_DEVICES, menuSelectedIndex, menuTopIndex);
    redraw = true;
  }
  
  if (redraw) {
    drawActivePage();
  }
  return true;
}

void IrUI::sendSingleKey(IRKey key) {
  bool ok = false;
  if (pageState == IR_PAGE_TV_KEYS) {
    ok = irSendBrandKey(selectedBrand, key);
  } else if (pageState == IR_PAGE_OTHER_KEYS) {
    ok = irSendOtherDeviceKey(selectedDevice, key);
  }
  
  if (ok) {
    statusText = "Sent!";
  } else {
    statusText = "No code";
  }
  statusStartMs = millis();
}

void IrUI::startUniversalBlast(const IRCodeEntry* codes, uint8_t count, const char* title) {
  pushMenuState(pageState, IR_PAGE_BLASTING, menuSelectedIndex, menuTopIndex);
  blastCodes = codes;
  blastCount = count;
  blastIndex = 0;
  lastBlastMs = millis();
  blastTitle = title;
  isUniversalPowerBlast = false;
}

void IrUI::startUniversalPowerBlast() {
  pushMenuState(pageState, IR_PAGE_BLASTING, menuSelectedIndex, menuTopIndex);
  blastCodes = nullptr;
  blastCount = UNIVERSAL_IR_SIGNAL_COUNT;
  blastIndex = 0;
  lastBlastMs = millis();
  blastTitle = "TV Power Blast";
  isUniversalPowerBlast = true;
}

void IrUI::drawActivePage() {
  if (statusText) {
    display->clear();
    display->drawStatusBar("IR Remote", RadioState::RADIO_OFF);
    auto d = display->getDriver();
    d->setTextSize(2);
    d->setTextColor(SSD1306_WHITE);
    
    // Draw centered status text
    int16_t x1, y1;
    uint16_t w, h;
    d->getTextBounds(statusText, 0, 0, &x1, &y1, &w, &h);
    d->setCursor((128 - w) / 2, (64 - h) / 2 + 5);
    
    d->print(statusText);
    d->setTextSize(1);
    d->display();
    return;
  }
  
  switch (pageState) {
    case IR_PAGE_MAIN:
      display->drawMenu("IR Remote", irMainItems, 3, menuSelectedIndex, menuTopIndex);
      break;
    case IR_PAGE_UNIVERSAL:
      display->drawMenu("Universal Blast", irUniversalItems, HIZMOS_ENABLE_IR_AC_CODES ? 7 : 6, menuSelectedIndex, menuTopIndex);
      break;
    case IR_PAGE_TV_BRANDS:
      display->drawMenu("TV Brand", irTvBrandItems, 5, menuSelectedIndex, menuTopIndex);
      break;
    case IR_PAGE_TV_KEYS:
      display->drawMenu("TV Remote", irTvKeyItems, 7, menuSelectedIndex, menuTopIndex);
      break;
    case IR_PAGE_OTHER_DEVICES:
      display->drawMenu("Other Devices", irOtherDeviceItems, HIZMOS_ENABLE_IR_AC_CODES ? 4 : 3, menuSelectedIndex, menuTopIndex);
      break;
    case IR_PAGE_OTHER_KEYS:
      if (selectedDevice == IR_DEV_STB) {
        display->drawMenu("STB Remote", irStbKeyItems, 4, menuSelectedIndex, menuTopIndex);
      } else if (selectedDevice == IR_DEV_PROJECTOR) {
        display->drawMenu("Proj Remote", irProjKeyItems, 3, menuSelectedIndex, menuTopIndex);
      } else if (selectedDevice == IR_DEV_SOUNDBAR) {
        display->drawMenu("Sbar Remote", irSbarKeyItems, 4, menuSelectedIndex, menuTopIndex);
      } else if (selectedDevice == IR_DEV_AC) {
        display->drawMenu("AC Remote", irAcKeyItems, 2, menuSelectedIndex, menuTopIndex);
      }
      break;
      
    case IR_PAGE_BLASTING: {
      display->clear();
      display->drawStatusBar(blastTitle, RadioState::RADIO_OFF);
      auto d = display->getDriver();
      
      d->setCursor(5, 20);
      d->setTextColor(SSD1306_WHITE);
      d->print(F("Blasting codes..."));
      
      // Progress bar
      d->drawRect(10, 35, 108, 10, SSD1306_WHITE);
      int total = isUniversalPowerBlast ? UNIVERSAL_IR_SIGNAL_COUNT : blastCount;
      int current = blastIndex;
      if (current > total) current = total;
      
      uint8_t barW = (total > 0) ? (current * 106) / total : 0;
      d->fillRect(11, 36, barW, 8, SSD1306_WHITE);
      
      char countBuf[32];
      snprintf(countBuf, sizeof(countBuf), "Trying %d/%d", current, total);
      d->setCursor(5, 52);
      d->print(countBuf);
      
      // Print short label for current code if available
      d->setCursor(80, 52);
      if (current < total) {
        if (isUniversalPowerBlast) {
          d->print(universal_ir_get_signal_name(current));
        } else {
          IRCodeEntry code;
          memcpy_P(&code, &blastCodes[current], sizeof(IRCodeEntry));
          d->print(code.label);
        }
      }
      
      d->display();
      break;
    }
  }
}
