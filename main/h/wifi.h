#ifndef WIFI_HOTSPOT_H
#define WIFI_HOTSPOT_H

#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

/* AP - network settings */
#define WIFI_SSID       "ESP32-Config-1"
#define WIFI_PASS       "esp32_config_psswd"
#define WIFI_CHANNEL    1
#define WIFI_MAX_CONNS  10

/* STA - Backup WiFi network credentials */
#define BACKUP_WIFI_SSID "OnePlus 12"
#define BACKUP_WIFI_PASS "z5g57j6g"

void wifi_init_ap_sta_mode(void);
void wifi_connect_backup(void);
void wifi_disconnect_backup(void);
bool wifi_is_backup_connected(void);

#endif