#include "wifi_utils.h"
#include <esp_wifi.h>

// Global structures for Probe Request Monitor
ProbeResult probeResults[MAX_PROBE_RESULTS];
uint8_t probeCount = 0;
uint32_t totalProbesSeen = 0;
bool probeMonitorActive = false;
uint8_t wifiRegionSetting = 0; // default IN
uint32_t probeHopIntervalMs = 100; // default 100ms

void sanitizeSSID(char* out, const uint8_t* raw, uint8_t rawLen, uint8_t outMax) {
  if (rawLen == 0 || raw[0] == 0x00) {
    strncpy(out, "(Hidden)", outMax);
    out[outMax - 1] = '\0';
    return;
  }
  uint8_t i;
  for (i = 0; i < rawLen && i < (outMax - 1); i++) {
    out[i] = (raw[i] >= 0x20 && raw[i] < 0x7F) ? (char)raw[i] : '.';
  }
  out[i] = '\0';
}

void setWiFiRegion(const char* cc, uint8_t startCh, uint8_t totalCh) {
  wifi_country_t country;
  memset(&country, 0, sizeof(country));
  strncpy(country.cc, cc, 3);
  country.schan     = startCh;
  country.nchan     = totalCh;
  country.max_tx_power = 20;
  country.policy    = WIFI_COUNTRY_POLICY_MANUAL;
  esp_wifi_set_country(&country);
}

uint8_t getWiFiRegionMaxChannel() {
  if (wifiRegionSetting == 1) return 11; // US
  if (wifiRegionSetting == 2) return 14; // JP
  return 13; // IN
}

void applyWiFiRegionFromSettings() {
  if (wifiRegionSetting == 1) {
    setWiFiRegion("US", 1, 11);
  } else if (wifiRegionSetting == 2) {
    setWiFiRegion("JP", 1, 14);
  } else {
    setWiFiRegion("IN", 1, 13);
  }
}

// Sniffer callback for probe request frames
static void probeSnifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  
  // Safe limit check for minimum packet size
  if (pkt->rx_ctrl.sig_len < 26) return;
  
  uint8_t* payload = pkt->payload;
  
  // Frame control bytes: type=0 (Management), subtype=4 (Probe Request)
  uint8_t frameType    = (payload[0] & 0x0C) >> 2;
  uint8_t frameSubtype = (payload[0] & 0xF0) >> 4;
  if (frameType != 0 || frameSubtype != 4) return;
  
  // SSID element: offset 24, tag=0, length at [25], data at [26]
  uint8_t ssidLen = payload[25];
  if (26 + ssidLen > pkt->rx_ctrl.sig_len) return; // Truncated SSID
  if (ssidLen > 32) ssidLen = 32;
  
  uint8_t* srcMac = &payload[10];
  
  char ssidBuf[33];
  if (ssidLen == 0) {
    strcpy(ssidBuf, "(Wildcard)");
  } else {
    for (uint8_t i = 0; i < ssidLen; i++) {
      ssidBuf[i] = (payload[26+i] >= 0x20 && payload[26+i] < 0x7F) ? (char)payload[26+i] : '.';
    }
    ssidBuf[ssidLen] = '\0';
  }
  
  // Check if we already have this probe (same MAC and same SSID)
  int foundIdx = -1;
  for (int i = 0; i < probeCount; i++) {
    if (memcmp(probeResults[i].mac, srcMac, 6) == 0 && strcmp(probeResults[i].ssid, ssidBuf) == 0) {
      foundIdx = i;
      break;
    }
  }
  
  totalProbesSeen++;
  
  if (foundIdx != -1) {
    probeResults[foundIdx].rssi = pkt->rx_ctrl.rssi;
    probeResults[foundIdx].lastSeenMs = millis();
  } else {
    uint8_t limit = (probeCount < MAX_PROBE_RESULTS) ? probeCount : (MAX_PROBE_RESULTS - 1);
    for (int i = limit; i > 0; i--) {
      probeResults[i] = probeResults[i-1];
    }
    memcpy(probeResults[0].mac, srcMac, 6);
    strncpy(probeResults[0].ssid, ssidBuf, 32);
    probeResults[0].ssid[32] = '\0';
    probeResults[0].rssi = pkt->rx_ctrl.rssi;
    probeResults[0].lastSeenMs = millis();
    
    if (probeCount < MAX_PROBE_RESULTS) {
      probeCount++;
    }
  }
}

void startProbeMonitor() {
  probeCount = 0;
  totalProbesSeen = 0;
  memset(probeResults, 0, sizeof(probeResults));
  
  Serial.println("[ProbeMon] Starting...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true); // ensure radio on, disconnected
  applyWiFiRegionFromSettings();
  
  delay(100); // Give Wi-Fi stack driver time to start
  
  esp_err_t err_prom_off = esp_wifi_set_promiscuous(false);
  
  esp_err_t err_cb = esp_wifi_set_promiscuous_rx_cb(probeSnifferCallback);
  Serial.printf("[ProbeMon] Set RX CB: %d\n", err_cb);
  
  wifi_promiscuous_filter_t filter;
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_err_t err_filt = esp_wifi_set_promiscuous_filter(&filter);
  Serial.printf("[ProbeMon] Set filter: %d\n", err_filt);
  
  esp_err_t err_prom_on = esp_wifi_set_promiscuous(true);
  Serial.printf("[ProbeMon] Promiscuous enable: %d\n", err_prom_on);
  
  esp_err_t err_chan = esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  Serial.printf("[ProbeMon] Set channel 1: %d\n", err_chan);
  
  probeMonitorActive = true;
}

void stopProbeMonitor() {
  probeMonitorActive = false;
  esp_wifi_set_promiscuous(false);
}

void updateProbeHopper() {
  if (!probeMonitorActive) return;
  
  static uint32_t lastHopMs = 0;
  static uint8_t currentChan = 1;
  
  uint8_t maxChan = getWiFiRegionMaxChannel();
  
  if ((millis() - lastHopMs) >= probeHopIntervalMs) {
    currentChan = (currentChan % maxChan) + 1;
    esp_wifi_set_channel(currentChan, WIFI_SECOND_CHAN_NONE);
    lastHopMs = millis();
  }
}

WifiScanner::WifiScanner() {
  networkCount = 0;
  currentState = ScanState::Idle;
  lastScanEndTime = 0;
}

void WifiScanner::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  applyWiFiRegionFromSettings();
}

bool WifiScanner::startScanAsync() {
  if (currentState == ScanState::Running || currentState == ScanState::Starting) {
    Serial.println("Scan request ignored: already running");
    return false;
  }
  
  if (lastScanEndTime > 0 && (millis() - lastScanEndTime < 1000)) {
    Serial.println("Scan request ignored: cooldown");
    return false;
  }
  
  Serial.println("--- Wi-Fi Scan Started ---");
  Serial.printf("Free Heap before scan: %u\n", ESP.getFreeHeap());
  
  WiFi.scanDelete(); // Ensure old buffers are cleared safely
  WiFi.mode(WIFI_STA);
  applyWiFiRegionFromSettings();
  WiFi.disconnect(false, true); // Don't turn off radio, just disconnect
  
  currentState = ScanState::Starting;
  WiFi.scanNetworks(true, true); // async=true, show_hidden=true
  currentState = ScanState::Running;
  return true;
}

ScanState WifiScanner::checkScanStatus() {
  if (currentState != ScanState::Running) {
    return currentState;
  }
  
  int16_t status = WiFi.scanComplete();
  
  if (status == WIFI_SCAN_RUNNING) {
    return ScanState::Running;
  } else if (status == WIFI_SCAN_FAILED) {
    currentState = ScanState::Failed;
    lastScanEndTime = millis();
    return currentState;
  } else {
    currentState = ScanState::Complete;
    return currentState;
  }
}

void WifiScanner::storeResults() {
  int count = WiFi.scanComplete();
  
  if (count <= 0) {
    networkCount = 0;
    WiFi.scanDelete();
    Serial.printf("Free Heap after scanDelete (0 nets): %u\n", ESP.getFreeHeap());
    currentState = ScanState::Idle;
    lastScanEndTime = millis();
    return;
  }
  
  networkCount = (count > MAX_NETWORKS) ? MAX_NETWORKS : count;
  Serial.printf("--- Wi-Fi Scan Complete: %d found (storing %d) ---\n", count, networkCount);
  
  for (int i = 0; i < networkCount; i++) {
    String rawSSID = WiFi.SSID(i);
    sanitizeSSID(networks[i].ssid, (const uint8_t*)rawSSID.c_str(), rawSSID.length(), sizeof(networks[i].ssid));
    
    strncpy(networks[i].bssid, WiFi.BSSIDstr(i).c_str(), 17);
    networks[i].bssid[17] = '\0';
    
    networks[i].rssi = WiFi.RSSI(i);
    networks[i].channel = WiFi.channel(i);
    networks[i].encryptionType = WiFi.encryptionType(i);
    
    Serial.printf("[%d] %s (%s) Ch:%d RSSI:%d Enc:%d\n", i, networks[i].ssid, networks[i].bssid, networks[i].channel, networks[i].rssi, networks[i].encryptionType);
  }
  
  // Sort by RSSI
  for (int i = 0; i < networkCount - 1; i++) {
    for (int j = i + 1; j < networkCount; j++) {
      if (networks[j].rssi > networks[i].rssi) {
        WifiNetwork temp = networks[i];
        networks[i] = networks[j];
        networks[j] = temp;
      }
    }
  }
  
  WiFi.scanDelete();
  Serial.printf("Free Heap after scanDelete: %u\n", ESP.getFreeHeap());
  currentState = ScanState::Idle;
  lastScanEndTime = millis();
}

void WifiScanner::cancelScan() {
  if (currentState == ScanState::Running || currentState == ScanState::Starting) {
    currentState = ScanState::Cancelled;
    WiFi.scanDelete();
    lastScanEndTime = millis();
    Serial.println("Scan cancelled. scanDelete() called.");
    Serial.printf("Free Heap after cancel: %u\n", ESP.getFreeHeap());
  }
}

void WifiScanner::clearResults() {
  networkCount = 0;
  Serial.println("Wi-Fi results cleared.");
}

