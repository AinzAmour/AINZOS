#ifndef BLE_UTILS_H
#define BLE_UTILS_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "wifi_utils.h" // For ScanState enum

enum BleDeviceType {
    BLE_TYPE_GENERIC = 0,
    BLE_TYPE_AIRTAG,
    BLE_TYPE_FIND_MY,     // other Apple FindMy devices
    BLE_TYPE_FLIPPER,
    BLE_TYPE_TILE,
    BLE_TYPE_NORDIC_UART, // HM-10, HC-08, BLE serial modules
    BLE_TYPE_SAMSUNG_TAG,
    BLE_TYPE_SKIMMER_SUSPECT, // HC-05/HM-10 default names with no real name
};

struct BleDevice {
  char name[32];
  char address[18];
  int8_t rssi;
  uint8_t serviceCount;
  bool hasName;
  bool hasMfgData;
  uint8_t deviceType; // cast of BleDeviceType
  uint8_t advPayload[62];
  uint8_t advPayloadLen;
};

class BleScanner {
public:
  BleScanner();
  void begin();
  void startScanAsync();
  ScanState checkScanStatus();
  void stopScanAndStore();
  void cancelScan();
  void clearResults();

  const BleDevice* getBleResultAt(uint8_t index) const {
    if (index < deviceCount) return &devices[index];
    return nullptr;
  }
  uint8_t getBleResultCount() const { return deviceCount; }

  static const int MAX_DEVICES = 20;
  BleDevice devices[MAX_DEVICES];
  int deviceCount;
  
  ScanState currentState;
  unsigned long scanStartTime;
  
  // Custom scan duration from NVS
  uint32_t scanDurationMs;
};

#endif
