/**
 * @file beacon_spam.cpp
 * @brief Beacon spam attack implementation
 */

#include "beacon_spam.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern "C" {

#define BEACON_LIST_MAX 16
#define BEACON_SSID_MAX_LEN 32
#define RANDOM_SSID_LEN 8
#define BEACON_BROADCAST_DELAY_MS 100
#define WIFI_CHANNELS_MAX 14

static char g_beacon_list[BEACON_LIST_MAX][BEACON_SSID_MAX_LEN + 1];
static int g_beacon_list_count = 0;
static TaskHandle_t beacon_task_handle = NULL;
static volatile bool beacon_task_running = false;

static void beacon_spam_task(void *param);
static void beacon_spam_list_task(void *param);
static void generate_random_ssid(char *ssid, size_t length);
static void generate_random_mac(uint8_t *mac);
static void configure_hidden_ap(void);
static void wifi_tx_prepare_ap_mode(void);
static void wifi_tx_restore_sta_mode(void);

static uint8_t build_channel_list(uint8_t *channels, size_t max) {
  uint8_t max_ch = 13;
  uint8_t n = (max_ch < max) ? max_ch : (uint8_t)max;
  for (uint8_t i = 0; i < n; i++) {
    channels[i] = i + 1;
  }
  return n;
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

static void generate_random_ssid(char *ssid, size_t length) {
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  for (size_t i = 0; i < length - 1; i++) {
    int random_index = esp_random() % (sizeof(charset) - 1);
    ssid[i] = charset[random_index];
  }
  ssid[length - 1] = '\0';
}

static void generate_random_mac(uint8_t *mac) {
  esp_fill_random(mac, 6);
  mac[0] &= 0xFE;
  mac[0] |= 0x02;
}

static void configure_hidden_ap(void) {
  wifi_config_t wifi_config = {};
  esp_err_t err = esp_wifi_get_config(WIFI_IF_AP, &wifi_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to get Wi-Fi config: %s\n", esp_err_to_name(err));
    return;
  }
  wifi_config.ap.ssid_hidden = 1;
  wifi_config.ap.beacon_interval = 10000;
  wifi_config.ap.ssid_len = 0;
  err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set Wi-Fi config: %s\n", esp_err_to_name(err));
  }
}

esp_err_t beacon_spam_broadcast(const char *ssid) {
  uint8_t packet[256] = {
    0x80, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0xc0, 0x6c,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x00,
    0x11, 0x04,
  };

  for (int ch = 1; ch <= 13; ch++) {
    if (!beacon_task_running) {
      return ESP_OK;
    }

    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    generate_random_mac(&packet[10]);
    memcpy(&packet[16], &packet[10], 6);

    char ssid_buffer[RANDOM_SSID_LEN + 1];
    const char *ssid_to_use = ssid;
    if (ssid_to_use == NULL) {
      generate_random_ssid(ssid_buffer, RANDOM_SSID_LEN + 1);
      ssid_to_use = ssid_buffer;
    }

    uint8_t ssid_len = strlen(ssid_to_use);
    packet[37] = ssid_len;
    memcpy(&packet[38], ssid_to_use, ssid_len);

    uint8_t *supported_rates_ie = &packet[38 + ssid_len];
    supported_rates_ie[0] = 0x01;
    supported_rates_ie[1] = 0x08;
    supported_rates_ie[2] = 0x82;
    supported_rates_ie[3] = 0x84;
    supported_rates_ie[4] = 0x8B;
    supported_rates_ie[5] = 0x96;
    supported_rates_ie[6] = 0x24;
    supported_rates_ie[7] = 0x30;
    supported_rates_ie[8] = 0x48;
    supported_rates_ie[9] = 0x6C;

    uint8_t *ds_param_set_ie = &supported_rates_ie[10];
    ds_param_set_ie[0] = 0x03;
    ds_param_set_ie[1] = 0x01;

    uint8_t primary_channel;
    wifi_second_chan_t second_channel;
    esp_wifi_get_channel(&primary_channel, &second_channel);
    ds_param_set_ie[2] = primary_channel;

    uint8_t *he_capabilities_ie = &ds_param_set_ie[3];
    he_capabilities_ie[0] = 0xFF;
    he_capabilities_ie[1] = 0x0D;
    he_capabilities_ie[2] = 0x50;
    he_capabilities_ie[3] = 0x6f;
    he_capabilities_ie[4] = 0x9A;
    he_capabilities_ie[5] = 0x00;
    he_capabilities_ie[6] = 0x08;
    he_capabilities_ie[7] = 0x00;
    he_capabilities_ie[8] = 0x00;
    he_capabilities_ie[9] = 0x40;
    he_capabilities_ie[10] = 0x00;
    he_capabilities_ie[11] = 0x00;
    he_capabilities_ie[12] = 0x01;

    size_t packet_size = (38 + ssid_len + 12 + 3 + 13);

    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, packet, packet_size, false);
    if (err != ESP_OK) {
      Serial.printf("Failed to send beacon frame: %s\n", esp_err_to_name(err));
      return err;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  return ESP_OK;
}

esp_err_t beacon_spam_broadcast_karma(const char *ssid) {
  if (ssid == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t packet[256] = {
    0x80, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xc0, 0x6c,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x00,
    0x11, 0x04,
  };

  uint8_t ap_mac[6];
  esp_wifi_get_mac(WIFI_IF_AP, ap_mac);
  memcpy(&packet[10], ap_mac, 6);
  memcpy(&packet[16], ap_mac, 6);

  uint8_t primary_channel;
  wifi_second_chan_t second_channel;
  esp_wifi_get_channel(&primary_channel, &second_channel);

  uint8_t ssid_len = strlen(ssid);
  packet[37] = ssid_len;
  memcpy(&packet[38], ssid, ssid_len);

  uint8_t *supported_rates_ie = &packet[38 + ssid_len];
  supported_rates_ie[0] = 0x01;
  supported_rates_ie[1] = 0x08;
  supported_rates_ie[2] = 0x82;
  supported_rates_ie[3] = 0x84;
  supported_rates_ie[4] = 0x8B;
  supported_rates_ie[5] = 0x96;
  supported_rates_ie[6] = 0x24;
  supported_rates_ie[7] = 0x30;
  supported_rates_ie[8] = 0x48;
  supported_rates_ie[9] = 0x6C;

  uint8_t *ds_param_set_ie = &supported_rates_ie[10];
  ds_param_set_ie[0] = 0x03;
  ds_param_set_ie[1] = 0x01;
  ds_param_set_ie[2] = primary_channel;

  uint8_t *he_capabilities_ie = &ds_param_set_ie[3];
  he_capabilities_ie[0] = 0xFF;
  he_capabilities_ie[1] = 0x0D;
  he_capabilities_ie[2] = 0x50;
  he_capabilities_ie[3] = 0x6f;
  he_capabilities_ie[4] = 0x9A;
  he_capabilities_ie[5] = 0x00;
  he_capabilities_ie[6] = 0x08;
  he_capabilities_ie[7] = 0x00;
  he_capabilities_ie[8] = 0x00;
  he_capabilities_ie[9] = 0x40;
  he_capabilities_ie[10] = 0x00;
  he_capabilities_ie[11] = 0x00;
  he_capabilities_ie[12] = 0x01;

  size_t packet_size = (38 + ssid_len + 12 + 3 + 13);

  uint8_t channels[WIFI_CHANNELS_MAX];
  uint8_t channel_count = build_channel_list(channels, sizeof(channels));

  for (int i = 0; i < channel_count; i++) {
    uint8_t ch = channels[i];
    if (!beacon_task_running) {
      return ESP_OK;
    }

    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    ds_param_set_ie[2] = ch;
    esp_wifi_80211_tx(WIFI_IF_AP, packet, packet_size, false);
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  esp_wifi_set_channel(primary_channel, second_channel);
  return ESP_OK;
}

static void beacon_spam_task(void *param) {
  const char *ssid = (const char *)param;

  const char *rickroll_lyrics[] = {
    "Never gonna give you up",
    "Never gonna let you down",
    "Never gonna run around and desert you",
    "Never gonna make you cry",
    "Never gonna say goodbye",
    "Never gonna tell a lie and hurt you"
  };
  int num_lines = 6;
  int line_index = 0;

  int IsRickRoll = ssid != NULL ? (strcmp(ssid, "RICKROLL") == 0) : 0;

  while (beacon_task_running) {
    if (IsRickRoll) {
      beacon_spam_broadcast(rickroll_lyrics[line_index]);
      line_index = (line_index + 1) % num_lines;
    } else {
      beacon_spam_broadcast(ssid);
    }
    vTaskDelay(pdMS_TO_TICKS(BEACON_BROADCAST_DELAY_MS));
  }

  free(param);
  beacon_task_handle = NULL;
  vTaskDelete(NULL);
}

static void beacon_spam_list_task(void *param) {
  (void)param;
  while (beacon_task_running) {
    for (int i = 0; i < g_beacon_list_count; ++i) {
      if (!beacon_task_running) break;
      beacon_spam_broadcast(g_beacon_list[i]);
      vTaskDelay(pdMS_TO_TICKS(BEACON_BROADCAST_DELAY_MS));
    }
  }
  beacon_task_handle = NULL;
  vTaskDelete(NULL);
}

void beacon_spam_start(const char *ssid) {
  if (!beacon_task_running) {
    Serial.println("[BEACON] start called");
    Serial.println("[BEACON] calling wifi_tx_prepare_ap_mode");
    wifi_tx_prepare_ap_mode();
    Serial.println("[BEACON] wifi_tx_prepare_ap_mode done");
    beacon_task_running = true;
    char *ssid_copy = ssid ? strdup(ssid) : NULL;
    if (ssid && !ssid_copy) {
      Serial.println("Failed to allocate SSID copy");
      beacon_task_running = false;
      wifi_tx_restore_sta_mode();
      return;
    }
    Serial.println("[BEACON] creating task");
    BaseType_t rc = xTaskCreate(beacon_spam_task, "beacon_task", 4096, (void *)ssid_copy, 5, &beacon_task_handle);
    if (rc != pdPASS) {
      Serial.printf("Failed to start beacon task (%ld)\n", (long)rc);
      beacon_task_running = false;
      beacon_task_handle = NULL;
      free(ssid_copy);
      wifi_tx_restore_sta_mode();
    }
    Serial.println("[BEACON] task created");
  } else {
    Serial.println("Beacon transmission already running.");
  }
}

void beacon_spam_stop(void) {
  if (beacon_task_running) {
    Serial.println("Stopping beacon transmission...");
    beacon_task_running = false;

    int wait_count = 0;
    while (beacon_task_handle != NULL && wait_count < 20) {
      vTaskDelay(pdMS_TO_TICKS(100));
      wait_count++;
    }

    if (beacon_task_handle != NULL) {
      vTaskDelete(beacon_task_handle);
      beacon_task_handle = NULL;
    }

    wifi_tx_restore_sta_mode();
  }
}

void beacon_spam_start_list(void) {
  if (g_beacon_list_count == 0) {
    Serial.println("No SSIDs in beacon list");
    return;
  }
  beacon_spam_stop();
  Serial.printf("Starting beacon spam list (%d SSIDs)...\n", g_beacon_list_count);
  wifi_tx_prepare_ap_mode();
  beacon_task_running = true;
  BaseType_t rc = xTaskCreate(beacon_spam_list_task, "beacon_list", 4096, NULL, 5, &beacon_task_handle);
  if (rc != pdPASS) {
    Serial.printf("Failed to start beacon list task (%ld)\n", (long)rc);
    beacon_task_running = false;
    beacon_task_handle = NULL;
    wifi_tx_restore_sta_mode();
  }
}

void beacon_spam_add_ssid(const char *ssid) {
  if (g_beacon_list_count >= BEACON_LIST_MAX) {
    Serial.println("Beacon list full");
    return;
  }
  if (strlen(ssid) > BEACON_SSID_MAX_LEN) {
    Serial.println("SSID too long");
    return;
  }
  for (int i = 0; i < g_beacon_list_count; ++i) {
    if (strcmp(g_beacon_list[i], ssid) == 0) {
      Serial.printf("SSID already in list: %s\n", ssid);
      return;
    }
  }
  strcpy(g_beacon_list[g_beacon_list_count++], ssid);
  Serial.printf("Added SSID to beacon list: %s\n", ssid);
}

void beacon_spam_remove_ssid(const char *ssid) {
  for (int i = 0; i < g_beacon_list_count; ++i) {
    if (strcmp(g_beacon_list[i], ssid) == 0) {
      for (int j = i; j < g_beacon_list_count - 1; ++j) {
        strcpy(g_beacon_list[j], g_beacon_list[j + 1]);
      }
      --g_beacon_list_count;
      Serial.printf("Removed SSID from beacon list: %s\n", ssid);
      return;
    }
  }
  Serial.printf("SSID not found in list: %s\n", ssid);
}

void beacon_spam_clear_list(void) {
  g_beacon_list_count = 0;
  Serial.println("Cleared beacon list");
}

void beacon_spam_show_list(void) {
  Serial.printf("Beacon list (%d entries):\n", g_beacon_list_count);
  for (int i = 0; i < g_beacon_list_count; ++i) {
    Serial.printf("  %d: %s\n", i, g_beacon_list[i]);
  }
}

bool beacon_spam_is_running(void) {
  return beacon_task_running;
}

} // extern "C"
