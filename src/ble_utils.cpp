#include "ble_utils.h"

#define COMPANY_APPLE    0x004C
#define COMPANY_SAMSUNG  0x0075
#define COMPANY_TILE     0x00D7

BleDeviceType classifyBLEDevice(NimBLEAdvertisedDevice* dev) {
    if (dev->haveManufacturerData()) {
        const std::string& mfr = dev->getManufacturerData();
        if (mfr.length() >= 2) {
            uint16_t cid = (uint8_t)mfr[0] | ((uint8_t)mfr[1] << 8);
            if (cid == COMPANY_APPLE) {
                if (mfr.length() > 2 && (uint8_t)mfr[2] == 0x12)
                    return BLE_TYPE_AIRTAG;
                return BLE_TYPE_FIND_MY;
            }
            if (cid == COMPANY_SAMSUNG) return BLE_TYPE_SAMSUNG_TAG;
            if (cid == COMPANY_TILE)    return BLE_TYPE_TILE;
        }
    }
    if (dev->haveServiceUUID()) {
        NimBLEUUID nusUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
        if (dev->isAdvertisingService(nusUUID)) return BLE_TYPE_NORDIC_UART;
    }
    if (dev->haveName()) {
        const std::string& nm = dev->getName();
        if (nm == "HC-05" || nm == "HC-08" || nm == "HMSoft" ||
            nm == "MLT-BT05" || nm == "BT04-A") return BLE_TYPE_SKIMMER_SUSPECT;
        if (nm.find("Flipper") != std::string::npos) return BLE_TYPE_FLIPPER;
    }
    return BLE_TYPE_GENERIC;
}

BleScanner::BleScanner() {
  deviceCount = 0;
  currentState = ScanState::Idle;
  scanDurationMs = 5000; // Default 5 seconds
}

void BleScanner::begin() {
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
}

void BleScanner::startScanAsync() {
  if (currentState == ScanState::Running || currentState == ScanState::Starting) {
    return;
  }
  
  Serial.println("--- BLE Scan Started ---");
  Serial.printf("Free Heap before BLE scan: %u\n", ESP.getFreeHeap());
  
  NimBLEScan* pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->clearResults(); // Ensure old results are clear
  
  currentState = ScanState::Starting;
  scanStartTime = millis();
  
  pBLEScan->start(0, nullptr, false); // continuous async scan
  
  currentState = ScanState::Running;
}

ScanState BleScanner::checkScanStatus() {
  if (currentState != ScanState::Running) {
    return currentState;
  }
  
  if (millis() - scanStartTime > scanDurationMs) {
    currentState = ScanState::Complete;
  }
  return currentState;
}

void BleScanner::stopScanAndStore() {
  NimBLEScan* pBLEScan = NimBLEDevice::getScan();
  pBLEScan->stop();
  
  NimBLEScanResults results = pBLEScan->getResults();
  int count = results.getCount();
  
  if (count <= 0) {
    deviceCount = 0;
    pBLEScan->clearResults();
    Serial.printf("Free Heap after BLE scanDelete (0 devs): %u\n", ESP.getFreeHeap());
    currentState = ScanState::Idle;
    return;
  }
  
  deviceCount = (count > MAX_DEVICES) ? MAX_DEVICES : count;
  Serial.printf("--- BLE Scan Complete: %d found (storing %d) ---\n", count, deviceCount);
  
  for (int i = 0; i < deviceCount; i++) {
    NimBLEAdvertisedDevice dev = results.getDevice(i);
    
    if (dev.haveName() && dev.getName().length() > 0) {
      devices[i].hasName = true;
      strncpy(devices[i].name, dev.getName().c_str(), 31);
      devices[i].name[31] = '\0';
    } else {
      devices[i].hasName = false;
      strcpy(devices[i].name, "Unknown");
    }
    
    strncpy(devices[i].address, dev.getAddress().toString().c_str(), 17);
    devices[i].address[17] = '\0';
    
    devices[i].rssi = dev.getRSSI();
    devices[i].serviceCount = dev.getServiceDataCount() + dev.getServiceUUIDCount();
    devices[i].hasMfgData = dev.haveManufacturerData();
    devices[i].deviceType = (uint8_t)classifyBLEDevice(&dev);
    
    // Cache raw advertisement payload bytes
    uint8_t* payload = dev.getPayload();
    uint8_t len = dev.getPayloadLength();
    if (len > 62) len = 62;
    devices[i].advPayloadLen = len;
    memcpy(devices[i].advPayload, payload, len);
  }
  
  // Sort by RSSI (strongest first)
  for (int i = 0; i < deviceCount - 1; i++) {
    for (int j = i + 1; j < deviceCount; j++) {
      if (devices[j].rssi > devices[i].rssi) {
        BleDevice temp = devices[i];
        devices[i] = devices[j];
        devices[j] = temp;
      }
    }
  }
  
  pBLEScan->clearResults();
  Serial.printf("Free Heap after BLE scanDelete: %u\n", ESP.getFreeHeap());
  currentState = ScanState::Idle;
}

void BleScanner::cancelScan() {
  if (currentState == ScanState::Running) {
    NimBLEDevice::getScan()->stop();
    NimBLEDevice::getScan()->clearResults();
    currentState = ScanState::Cancelled;
    Serial.println("BLE Scan cancelled.");
    Serial.printf("Free Heap after BLE cancel: %u\n", ESP.getFreeHeap());
  }
}

void BleScanner::clearResults() {
  deviceCount = 0;
  Serial.println("BLE results cleared.");
}
