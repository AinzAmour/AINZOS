#include "settings.h"

extern uint8_t wifiRegionSetting;
extern uint32_t probeHopIntervalMs;

void SettingsManager::begin() {
  load();
}

void SettingsManager::loadDefaults() {
  contrast = 127;
  anim_speed = 1;      // Normal
  wifi_region = 0;     // IN
  ble_duration = 5;    // 5s
  dim_time = 30;       // 30s
  boot_logo = true;    // ON
  scroll_speed = 1;    // Med
  probe_hop = 100;     // 100ms
  ble_filter = 0;      // All
  
  cs.enabled = true;
  cs.bootClockEnabled = true;
  cs.autoStartOnIdle = true;
  cs.wifiClock = true;
  cs.ntpSync = true;
  cs.animationEnabled = true;
  cs.deepSleepEnabled = false;
  cs.clockDurationMs = 8000;
  cs.animationDurationMs = 12000;
  cs.idleAnimationTimeoutMs = 30000;
  cs.sleepTimeoutMs = 300000;
  cs.globalIdleTimeoutMs = 60000;
  cs.animationSpeedMs = 50;
  cs.use24h = false;
}

void SettingsManager::load() {
  prefs.begin("hizmos", true); // read-only
  if (!prefs.isKey("setup")) {
    prefs.end();
    loadDefaults();
    save(); // initialize
    return;
  }
  contrast = prefs.getUChar("contrast", 127);
  anim_speed = prefs.getUChar("anim", 1);
  wifi_region = prefs.getUChar("wf_region", 0);
  ble_duration = prefs.getUChar("ble_dur", 5);
  dim_time = prefs.getUChar("dim_time", 30);
  boot_logo = prefs.getBool("boot_logo", true);
  scroll_speed = prefs.getUChar("scr_spd", 1);
  probe_hop = prefs.getUShort("prb_hop", 100);
  ble_filter = prefs.getUChar("ble_flt", 0);
  
  cs.enabled = prefs.getBool("cs_en", true);
  cs.bootClockEnabled = prefs.getBool("cs_boot", true);
  cs.autoStartOnIdle = prefs.getBool("cs_auto", true);
  cs.wifiClock = prefs.getBool("cs_wifi", true);
  cs.ntpSync = prefs.getBool("cs_ntp", true);
  cs.animationEnabled = prefs.getBool("cs_anim", true);
  cs.deepSleepEnabled = prefs.getBool("cs_ds", false);
  cs.clockDurationMs = prefs.getUInt("cs_clk", 8000);
  cs.animationDurationMs = prefs.getUInt("cs_adur", 12000);
  cs.idleAnimationTimeoutMs = prefs.getUInt("cs_idle", 30000);
  cs.sleepTimeoutMs = prefs.getUInt("cs_sleep", 300000);
  cs.globalIdleTimeoutMs = prefs.getUInt("cs_gidle", 60000);
  cs.animationSpeedMs = prefs.getUChar("cs_spd", 50);
  cs.use24h = prefs.getBool("cs_24h", false);
  
  prefs.end();

  // Populate global configuration variables
  wifiRegionSetting = wifi_region;
  probeHopIntervalMs = probe_hop;

  Serial.println("--- Settings Loaded ---");
  Serial.printf("Contrast: %d\n", contrast);
  Serial.printf("Region: %d\n", wifi_region);
  Serial.printf("BLE scan dur: %d\n", ble_duration);
  Serial.printf("Dim timeout: %d\n", dim_time);
}

void SettingsManager::save() {
  prefs.begin("hizmos", false); // read-write
  prefs.putBool("setup", true);
  prefs.putUChar("contrast", contrast);
  prefs.putUChar("anim", anim_speed);
  prefs.putUChar("wf_region", wifi_region);
  prefs.putUChar("ble_dur", ble_duration);
  prefs.putUChar("dim_time", dim_time);
  prefs.putBool("boot_logo", boot_logo);
  prefs.putUChar("scr_spd", scroll_speed);
  prefs.putUShort("prb_hop", probe_hop);
  prefs.putUChar("ble_flt", ble_filter);
  
  prefs.putBool("cs_en", cs.enabled);
  prefs.putBool("cs_boot", cs.bootClockEnabled);
  prefs.putBool("cs_auto", cs.autoStartOnIdle);
  prefs.putBool("cs_wifi", cs.wifiClock);
  prefs.putBool("cs_ntp", cs.ntpSync);
  prefs.putBool("cs_anim", cs.animationEnabled);
  prefs.putBool("cs_ds", cs.deepSleepEnabled);
  prefs.putUInt("cs_clk", cs.clockDurationMs);
  prefs.putUInt("cs_adur", cs.animationDurationMs);
  prefs.putUInt("cs_idle", cs.idleAnimationTimeoutMs);
  prefs.putUInt("cs_sleep", cs.sleepTimeoutMs);
  prefs.putUInt("cs_gidle", cs.globalIdleTimeoutMs);
  prefs.putUChar("cs_spd", cs.animationSpeedMs);
  prefs.putBool("cs_24h", cs.use24h);
  
  prefs.end();
  
  // Sync global configuration variables
  wifiRegionSetting = wifi_region;
  probeHopIntervalMs = probe_hop;
}

void SettingsManager::factoryReset() {
  prefs.begin("hizmos", false);
  prefs.clear();
  prefs.end();
  loadDefaults();
  save();
}
