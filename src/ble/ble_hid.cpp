#include "ble_hid.h"
#include "ble_spam.h"

// ============================================================================
// HID Report Descriptors — Composite Keyboard + Mouse
// ============================================================================

// Keyboard Report (Report ID 1): 8 bytes
//   byte 0: modifier bits
//   byte 1: reserved (0x00)
//   bytes 2-7: up to 6 simultaneous keycodes
//
// Mouse Report (Report ID 2): 4 bytes
//   byte 0: button bits
//   byte 1: X delta (-127 to 127)
//   byte 2: Y delta (-127 to 127)
//   byte 3: scroll wheel delta

static const uint8_t HID_REPORT_MAP[] = {
  // Keyboard
  0x05, 0x01,       // Usage Page (Generic Desktop)
  0x09, 0x06,       // Usage (Keyboard)
  0xA1, 0x01,       // Collection (Application)
  0x85, 0x01,       //   Report ID (1)
  0x05, 0x07,       //   Usage Page (Key Codes)
  0x19, 0xE0,       //   Usage Minimum (224) — Left Control
  0x29, 0xE7,       //   Usage Maximum (231) — Right GUI
  0x15, 0x00,       //   Logical Minimum (0)
  0x25, 0x01,       //   Logical Maximum (1)
  0x75, 0x01,       //   Report Size (1)
  0x95, 0x08,       //   Report Count (8) — 8 modifier bits
  0x81, 0x02,       //   Input (Data, Variable, Absolute)
  0x95, 0x01,       //   Report Count (1)
  0x75, 0x08,       //   Report Size (8)
  0x81, 0x01,       //   Input (Constant) — reserved byte
  0x95, 0x06,       //   Report Count (6)
  0x75, 0x08,       //   Report Size (8)
  0x15, 0x00,       //   Logical Minimum (0)
  0x25, 0x65,       //   Logical Maximum (101)
  0x05, 0x07,       //   Usage Page (Key Codes)
  0x19, 0x00,       //   Usage Minimum (0)
  0x29, 0x65,       //   Usage Maximum (101)
  0x81, 0x00,       //   Input (Data, Array)
  0xC0,             // End Collection

  // Mouse
  0x05, 0x01,       // Usage Page (Generic Desktop)
  0x09, 0x02,       // Usage (Mouse)
  0xA1, 0x01,       // Collection (Application)
  0x85, 0x02,       //   Report ID (2)
  0x09, 0x01,       //   Usage (Pointer)
  0xA1, 0x00,       //   Collection (Physical)
  0x05, 0x09,       //     Usage Page (Button)
  0x19, 0x01,       //     Usage Minimum (Button 1)
  0x29, 0x03,       //     Usage Maximum (Button 3)
  0x15, 0x00,       //     Logical Minimum (0)
  0x25, 0x01,       //     Logical Maximum (1)
  0x95, 0x03,       //     Report Count (3)
  0x75, 0x01,       //     Report Size (1)
  0x81, 0x02,       //     Input (Data, Variable, Absolute) — 3 button bits
  0x95, 0x01,       //     Report Count (1)
  0x75, 0x05,       //     Report Size (5)
  0x81, 0x01,       //     Input (Constant) — 5 padding bits
  0x05, 0x01,       //     Usage Page (Generic Desktop)
  0x09, 0x30,       //     Usage (X)
  0x09, 0x31,       //     Usage (Y)
  0x09, 0x38,       //     Usage (Wheel)
  0x15, 0x81,       //     Logical Minimum (-127)
  0x25, 0x7F,       //     Logical Maximum (127)
  0x75, 0x08,       //     Report Size (8)
  0x95, 0x03,       //     Report Count (3)
  0x81, 0x06,       //     Input (Data, Variable, Relative)
  0xC0,             //   End Collection (Physical)
  0xC0              // End Collection (Application)
};

// ============================================================================
// HID Information value: version 1.1, country 0, flags 0x01 (remote wake)
// ============================================================================
static const uint8_t HID_INFORMATION[] = { 0x11, 0x01, 0x00, 0x01 };

// ============================================================================
// Server Callbacks
// ============================================================================

void BleHID::ServerCallbacks::onConnect(NimBLEServer* pServer) {
  Serial.println("[BadBLE] Host connected");
  _parent->_connected = true;
}

void BleHID::ServerCallbacks::onDisconnect(NimBLEServer* pServer) {
  Serial.println("[BadBLE] Host disconnected");
  _parent->_connected = false;
  // Restart advertising if still running
  if (_parent->_running) {
    NimBLEDevice::startAdvertising();
  }
}

// ============================================================================
// Constructor
// ============================================================================

BleHID::BleHID()
  : _running(false), _connected(false),
    pServer(nullptr), pKeyboardReport(nullptr), pMouseReport(nullptr),
    _serverCallbacks(nullptr) {
}

// ============================================================================
// begin() — Initialize NimBLE as HID peripheral
// ============================================================================

void BleHID::begin() {
  if (_running) return;

  // Stop BLE spam if running (radio conflict guard)
  if (ble_spam_is_running()) {
    Serial.println("[BadBLE] Stopping BLE spam before HID init");
    ble_spam_stop();
  }

  // Full deinit to clear any previous NimBLE state (scanner, spam, etc.)
  if (NimBLEDevice::getInitialized()) {
    NimBLEDevice::deinit(true);
    delay(100);
  }

  // Initialize as named peripheral
  NimBLEDevice::init("BT Keyboard");
  NimBLEDevice::setSecurityAuth(true, true, true); // bonding, MITM, SC
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  // Create server
  pServer = NimBLEDevice::createServer();
  _serverCallbacks = new ServerCallbacks(this);
  pServer->setCallbacks(_serverCallbacks);

  // ---- Device Information Service (0x180A) ----
  NimBLEService* pDevInfo = pServer->createService("180A");
  pDevInfo->createCharacteristic("2A29", NIMBLE_PROPERTY::READ)
    ->setValue("AINZOS");
  pDevInfo->createCharacteristic("2A24", NIMBLE_PROPERTY::READ)
    ->setValue("BadBLE HID");
  pDevInfo->createCharacteristic("2A26", NIMBLE_PROPERTY::READ)
    ->setValue("1.0");

  // ---- Battery Service (0x180F) ----
  NimBLEService* pBattery = pServer->createService("180F");
  uint8_t batteryLevel = 100;
  pBattery->createCharacteristic("2A19", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY)
    ->setValue(&batteryLevel, 1);

  // ---- HID Service (0x1812) ----
  NimBLEService* pHID = pServer->createService(NimBLEUUID((uint16_t)0x1812));

  // HID Information (0x2A4A)
  pHID->createCharacteristic(
    NimBLEUUID((uint16_t)0x2A4A),
    NIMBLE_PROPERTY::READ
  )->setValue((uint8_t*)HID_INFORMATION, sizeof(HID_INFORMATION));

  // Report Map (0x2A4B) — the composite descriptor
  pHID->createCharacteristic(
    NimBLEUUID((uint16_t)0x2A4B),
    NIMBLE_PROPERTY::READ
  )->setValue((uint8_t*)HID_REPORT_MAP, sizeof(HID_REPORT_MAP));

  // HID Control Point (0x2A4C) — host writes suspend/exit-suspend
  pHID->createCharacteristic(
    NimBLEUUID((uint16_t)0x2A4C),
    NIMBLE_PROPERTY::WRITE_NR
  );

  // Protocol Mode (0x2A4E) — Report Protocol (1)
  uint8_t protocolMode = 1;
  pHID->createCharacteristic(
    NimBLEUUID((uint16_t)0x2A4E),
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR
  )->setValue(&protocolMode, 1);

  // Keyboard Input Report (Report ID 1)
  pKeyboardReport = pHID->createCharacteristic(
    NimBLEUUID((uint16_t)0x2A4D),
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  // Add Report Reference descriptor: Report ID=1, Type=Input(1)
  NimBLEDescriptor* kbDesc = pKeyboardReport->createDescriptor(
    NimBLEUUID((uint16_t)0x2908),
    NIMBLE_PROPERTY::READ
  );
  uint8_t kbRef[] = { 0x01, 0x01 }; // Report ID 1, Input
  kbDesc->setValue(kbRef, 2);

  // Mouse Input Report (Report ID 2)
  pMouseReport = pHID->createCharacteristic(
    NimBLEUUID((uint16_t)0x2A4D),
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  // Add Report Reference descriptor: Report ID=2, Type=Input(1)
  NimBLEDescriptor* msDesc = pMouseReport->createDescriptor(
    NimBLEUUID((uint16_t)0x2908),
    NIMBLE_PROPERTY::READ
  );
  uint8_t msRef[] = { 0x02, 0x01 }; // Report ID 2, Input
  msDesc->setValue(msRef, 2);

  // Start all services
  pDevInfo->start();
  pBattery->start();
  pHID->start();

  // Configure advertising
  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->setAppearance(0x03C1); // Keyboard appearance
  pAdv->addServiceUUID(NimBLEUUID((uint16_t)0x1812)); // HID service
  pAdv->setScanResponse(true);
  pAdv->setMinPreferred(0x06);
  pAdv->setMaxPreferred(0x12);
  NimBLEDevice::startAdvertising();

  _running = true;
  _connected = false;
  Serial.println("[BadBLE] HID server started, advertising as 'BT Keyboard'");
}

// ============================================================================
// stop() — Tear down HID server
// ============================================================================

void BleHID::stop() {
  if (!_running) return;

  _running = false;
  _connected = false;

  if (NimBLEDevice::getInitialized()) {
    if (NimBLEDevice::getAdvertising()->isAdvertising()) {
      NimBLEDevice::getAdvertising()->stop();
    }
    NimBLEDevice::deinit(true);
  }

  pServer = nullptr;
  pKeyboardReport = nullptr;
  pMouseReport = nullptr;

  if (_serverCallbacks) {
    delete _serverCallbacks;
    _serverCallbacks = nullptr;
  }

  Serial.println("[BadBLE] HID server stopped");
}

bool BleHID::isConnected() {
  return _connected;
}

bool BleHID::isRunning() {
  return _running;
}

// ============================================================================
// Keyboard functions
// ============================================================================

void BleHID::sendKeyboardReport(uint8_t mod, uint8_t key1, uint8_t key2,
                                 uint8_t key3, uint8_t key4,
                                 uint8_t key5, uint8_t key6) {
  if (!_connected || !pKeyboardReport) return;
  uint8_t report[8] = { mod, 0x00, key1, key2, key3, key4, key5, key6 };
  pKeyboardReport->setValue(report, sizeof(report));
  pKeyboardReport->notify();
}

void BleHID::sendKey(uint8_t modifier, uint8_t keycode) {
  sendKeyboardReport(modifier, keycode);
  delay(10);
  releaseAll();
  delay(50);
}

void BleHID::sendCombo(uint8_t mod, uint8_t key) {
  sendKey(mod, key);
}

void BleHID::releaseAll() {
  sendKeyboardReport(0, 0);
}

// ASCII to HID keycode lookup
uint8_t BleHID::charToKeycode(char c, uint8_t* modifier) {
  *modifier = 0;

  if (c >= 'a' && c <= 'z') {
    return 0x04 + (c - 'a'); // a=0x04 .. z=0x1D
  }
  if (c >= 'A' && c <= 'Z') {
    *modifier = MOD_SHIFT;
    return 0x04 + (c - 'A');
  }
  if (c >= '1' && c <= '9') {
    return 0x1E + (c - '1'); // 1=0x1E .. 9=0x26
  }
  if (c == '0') return 0x27;

  // Special characters
  switch (c) {
    case '\n': case '\r': return 0x28; // Enter
    case '\t':            return 0x2B; // Tab
    case ' ':             return 0x2C; // Space
    case '-':             return 0x2D;
    case '=':             return 0x2E;
    case '[':             return 0x2F;
    case ']':             return 0x30;
    case '\\':            return 0x31;
    case ';':             return 0x33;
    case '\'':            return 0x34;
    case '`':             return 0x35;
    case ',':             return 0x36;
    case '.':             return 0x37;
    case '/':             return 0x38;

    // Shifted special characters
    case '!': *modifier = MOD_SHIFT; return 0x1E; // Shift+1
    case '@': *modifier = MOD_SHIFT; return 0x1F; // Shift+2
    case '#': *modifier = MOD_SHIFT; return 0x20; // Shift+3
    case '$': *modifier = MOD_SHIFT; return 0x21; // Shift+4
    case '%': *modifier = MOD_SHIFT; return 0x22; // Shift+5
    case '^': *modifier = MOD_SHIFT; return 0x23; // Shift+6
    case '&': *modifier = MOD_SHIFT; return 0x24; // Shift+7
    case '*': *modifier = MOD_SHIFT; return 0x25; // Shift+8
    case '(': *modifier = MOD_SHIFT; return 0x26; // Shift+9
    case ')': *modifier = MOD_SHIFT; return 0x27; // Shift+0
    case '_': *modifier = MOD_SHIFT; return 0x2D; // Shift+-
    case '+': *modifier = MOD_SHIFT; return 0x2E; // Shift+=
    case '{': *modifier = MOD_SHIFT; return 0x2F; // Shift+[
    case '}': *modifier = MOD_SHIFT; return 0x30; // Shift+]
    case '|': *modifier = MOD_SHIFT; return 0x31; // Shift+backslash
    case ':': *modifier = MOD_SHIFT; return 0x33; // Shift+;
    case '"': *modifier = MOD_SHIFT; return 0x34; // Shift+'
    case '~': *modifier = MOD_SHIFT; return 0x35; // Shift+`
    case '<': *modifier = MOD_SHIFT; return 0x36; // Shift+,
    case '>': *modifier = MOD_SHIFT; return 0x37; // Shift+.
    case '?': *modifier = MOD_SHIFT; return 0x38; // Shift+/

    default: return 0x00; // Unknown
  }
}

void BleHID::typeString(const char* str) {
  if (!_connected || !str) return;
  while (*str) {
    uint8_t mod = 0;
    uint8_t keycode = charToKeycode(*str, &mod);
    if (keycode != 0x00) {
      sendKey(mod, keycode);
    }
    str++;
  }
}

// ============================================================================
// Mouse functions
// ============================================================================

void BleHID::sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
  if (!_connected || !pMouseReport) return;
  uint8_t report[4] = { buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel };
  pMouseReport->setValue(report, sizeof(report));
  pMouseReport->notify();
}

void BleHID::moveMouse(int8_t x, int8_t y) {
  sendMouseReport(0, x, y, 0);
  delay(10);
  sendMouseReport(0, 0, 0, 0); // Release
}

void BleHID::clickLeft() {
  sendMouseReport(MOUSE_LEFT, 0, 0, 0);
  delay(50);
  sendMouseReport(0, 0, 0, 0);
  delay(10);
}

void BleHID::clickRight() {
  sendMouseReport(MOUSE_RIGHT, 0, 0, 0);
  delay(50);
  sendMouseReport(0, 0, 0, 0);
  delay(10);
}

void BleHID::scroll(int8_t amount) {
  sendMouseReport(0, 0, 0, amount);
  delay(10);
  sendMouseReport(0, 0, 0, 0);
}
