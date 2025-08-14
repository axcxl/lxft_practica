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

#define WIFI_SSID       "ESP32-Config"
#define WIFI_PASS       "esp32_config_psswd"
#define WIFI_CHANNEL    1
#define WIFI_MAX_CONNS  10

void wifi_init_softap(void);

#endif