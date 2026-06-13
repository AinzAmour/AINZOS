#ifndef WIFI_UTILS_H
#define WIFI_UTILS_H

#include <Arduino.h>
#include <WiFi.h>

enum class ScanState {
  Idle,
  Starting,
  Running,
  Complete,
  Failed,
  Cancelled
};

struct WifiNetwork {
  char ssid[33];
  char bssid[18];
  int8_t rssi;
  uint8_t channel;
  uint8_t encryptionType;
};

// Probe Request Monitor structures
struct ProbeResult {
  uint8_t mac[6];
  char ssid[33];
  int8_t rssi;
  uint32_t lastSeenMs;
};

#define MAX_PROBE_RESULTS 10
extern ProbeResult probeResults[MAX_PROBE_RESULTS];
extern uint8_t probeCount;
extern uint32_t totalProbesSeen;
extern bool probeMonitorActive;
extern uint8_t wifiRegionSetting; // 0=IN, 1=US, 2=JP
extern uint32_t probeHopIntervalMs; // default 100ms

void startProbeMonitor();
void stopProbeMonitor();
void updateProbeHopper();
uint8_t getWiFiRegionMaxChannel();
void setWiFiRegion(const char* cc, uint8_t startCh, uint8_t totalCh);
void applyWiFiRegionFromSettings();

class WifiScanner {
public:
  WifiScanner();
  void begin();
  bool startScanAsync();
  ScanState checkScanStatus();
  void storeResults();
  void clearResults();
  void cancelScan();
  
  static const int MAX_NETWORKS = 25;
  WifiNetwork networks[MAX_NETWORKS];
  int networkCount;
  
  ScanState currentState;
  uint32_t lastScanEndTime;
};

#endif
