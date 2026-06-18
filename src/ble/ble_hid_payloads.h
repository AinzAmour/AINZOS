#ifndef BLE_HID_PAYLOADS_H
#define BLE_HID_PAYLOADS_H

#include "ble_hid.h"

// ============================================================================
// HID Keycodes used in payloads
// ============================================================================
#define KEY_ENTER       0x28
#define KEY_ESCAPE      0x29
#define KEY_BACKSPACE   0x2A
#define KEY_TAB         0x2B
#define KEY_SPACE       0x2C
#define KEY_F4          0x3D
#define KEY_F5          0x3E
#define KEY_F11         0x44
#define KEY_R           0x15  // 'r' keycode

// ============================================================================
// Payload functions
// ============================================================================

// Alt + F4 — close active window
inline void payload_alt_f4(BleHID* hid) {
  delay(1500);
  hid->sendCombo(MOD_ALT, KEY_F4);
}

// Win + R — open Run dialog
inline void payload_open_run(BleHID* hid) {
  delay(1500);
  hid->sendCombo(MOD_GUI, KEY_R);
}

// Open PowerShell via Run dialog
inline void payload_open_powershell(BleHID* hid) {
  delay(1500);
  hid->sendCombo(MOD_GUI, KEY_R);
  delay(600);
  hid->typeString("powershell");
  delay(200);
  hid->sendKey(0, KEY_ENTER);
}

// Open PowerShell → ipconfig /all
inline void payload_ipconfig(BleHID* hid) {
  delay(1500);
  hid->sendCombo(MOD_GUI, KEY_R);
  delay(600);
  hid->typeString("powershell");
  delay(200);
  hid->sendKey(0, KEY_ENTER);
  delay(1500);
  hid->typeString("ipconfig /all");
  delay(200);
  hid->sendKey(0, KEY_ENTER);
}

// Open PowerShell → systeminfo
inline void payload_sysinfo(BleHID* hid) {
  delay(1500);
  hid->sendCombo(MOD_GUI, KEY_R);
  delay(600);
  hid->typeString("powershell");
  delay(200);
  hid->sendKey(0, KEY_ENTER);
  delay(1500);
  hid->typeString("systeminfo");
  delay(200);
  hid->sendKey(0, KEY_ENTER);
}

// Open PowerShell → ipconfig + systeminfo
inline void payload_ip_and_sys(BleHID* hid) {
  delay(1500);
  hid->sendCombo(MOD_GUI, KEY_R);
  delay(600);
  hid->typeString("powershell");
  delay(200);
  hid->sendKey(0, KEY_ENTER);
  delay(1500);
  hid->typeString("ipconfig /all; systeminfo");
  delay(200);
  hid->sendKey(0, KEY_ENTER);
}

// Close all windows — spam Alt+F4 five times
inline void payload_close_all(BleHID* hid) {
  delay(1500);
  for (int i = 0; i < 5; i++) {
    hid->sendCombo(MOD_ALT, KEY_F4);
    delay(600);
  }
}

// Custom text payload — type arbitrary string and press Enter
inline void payload_custom(BleHID* hid, const char* text) {
  delay(1500);
  hid->typeString(text);
  delay(200);
  hid->sendKey(0, KEY_ENTER);
}

// ============================================================================
// Payload table for menu display
// ============================================================================

#define BADBLE_PAYLOAD_COUNT 8

static const char* BADBLE_PAYLOAD_NAMES[BADBLE_PAYLOAD_COUNT] = {
  "Alt + F4",
  "Open Run",
  "Open PowerShell",
  "IP Config",
  "System Info",
  "IP + Sys Info",
  "Close All Win",
  "Custom Text"
};

// Mouse action names
#define BADBLE_MOUSE_COUNT 8

static const char* BADBLE_MOUSE_NAMES[BADBLE_MOUSE_COUNT] = {
  "Move Up",
  "Move Down",
  "Move Left",
  "Move Right",
  "Left Click",
  "Right Click",
  "Scroll Up",
  "Scroll Down"
};

#define BADBLE_TOTAL_ITEMS (BADBLE_PAYLOAD_COUNT + BADBLE_MOUSE_COUNT)

#endif // BLE_HID_PAYLOADS_H
