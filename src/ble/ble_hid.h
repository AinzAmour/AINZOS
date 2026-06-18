#ifndef BLE_HID_H
#define BLE_HID_H

#include <Arduino.h>
#include <NimBLEDevice.h>

// HID Modifier key bits
#define MOD_CTRL   0x01
#define MOD_SHIFT  0x02
#define MOD_ALT    0x04
#define MOD_GUI    0x08

// Mouse button bits
#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04

class BleHID {
public:
  BleHID();

  void begin();           // Init NimBLE as BLE HID peripheral
  void stop();            // Stop HID, deinit NimBLE
  bool isConnected();     // True when a host has paired and connected
  bool isRunning();       // True when HID server is active

  // Keyboard
  void sendKey(uint8_t modifier, uint8_t keycode);
  void typeString(const char* str);
  void sendCombo(uint8_t mod, uint8_t key);
  void releaseAll();

  // Mouse
  void moveMouse(int8_t x, int8_t y);
  void clickLeft();
  void clickRight();
  void scroll(int8_t amount);

private:
  bool _running;
  bool _connected;

  NimBLEServer* pServer;
  NimBLECharacteristic* pKeyboardReport;
  NimBLECharacteristic* pMouseReport;

  void sendKeyboardReport(uint8_t mod, uint8_t key1, uint8_t key2 = 0,
                          uint8_t key3 = 0, uint8_t key4 = 0,
                          uint8_t key5 = 0, uint8_t key6 = 0);
  void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);

  uint8_t charToKeycode(char c, uint8_t* modifier);

  // Server callbacks (implemented as friend class)
  class ServerCallbacks : public NimBLEServerCallbacks {
  public:
    ServerCallbacks(BleHID* parent) : _parent(parent) {}
    void onConnect(NimBLEServer* pServer) override;
    void onDisconnect(NimBLEServer* pServer) override;
  private:
    BleHID* _parent;
  };
  friend class ServerCallbacks;
  ServerCallbacks* _serverCallbacks;
};

#endif // BLE_HID_H
