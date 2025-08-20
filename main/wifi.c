#include "h/wifi.h"
#include "h/dns_server.h"
#include <string.h>

static const char *TAG = "__WIFI__";
static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;
static bool backup_wifi_connected = false;
static bool backup_wifi_enabled = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch(event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi STA started");
            if (backup_wifi_enabled) {
                esp_wifi_connect();
            }
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi STA connected to backup network");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi STA disconnected from backup network");
            backup_wifi_connected = false;
            if (backup_wifi_enabled) {
                ESP_LOGI(TAG, "Retrying backup WiFi connection...");
                vTaskDelay(pdMS_TO_TICKS(3000)); // Wait before retry
                esp_wifi_connect();
            }
            break;

        case IP_EVENT_STA_GOT_IP:
            ip_event_got_ip_t *event_ip = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "WiFi STA Got IP: " IPSTR, IP2STR(&event_ip->ip_info.ip));
            backup_wifi_connected = true;
            break;
        
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "Device connected to hotspot");
            break;

        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "Device disconnected from hotspot");
            break;

        default:
            ESP_LOGD(TAG, "WiFi event not handled: %ld", event_id);
    }
}

void wifi_init_ap_sta_mode(void) {
    /* Create network interfaces for both AP and STA */
    ap_netif = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    /* Configure AP (Access Point) mode */
    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = WIFI_MAX_CONNS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    /* Configure STA (Station) mode for backup network */
    wifi_config_t sta_config = {
        .sta = {
            .ssid = BACKUP_WIFI_SSID,
            .password = BACKUP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    /* Set WiFi mode to AP+STA */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    
    /* Configure custom IP address for AP */
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 11, 111);
    IP4_ADDR(&ip_info.gw, 192, 168, 11, 111);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    
    /* Apply the IP configuration to AP interface */
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_dhcps_start(ap_netif);
    
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Start DNS server for captive portal */
    ESP_ERROR_CHECK(start_dns_server(ap_netif));

    ESP_LOGI(TAG, "WiFi AP+STA mode started. Hotspot SSID:%s channel:%d IP:192.168.11.111",
             WIFI_SSID, WIFI_CHANNEL);
}

void wifi_connect_backup(void) {
    if (!backup_wifi_enabled) {
        backup_wifi_enabled = true;
        ESP_LOGI(TAG, "Enabling backup WiFi connection to %s", BACKUP_WIFI_SSID);
        esp_wifi_connect();
    }
}

void wifi_disconnect_backup(void) {
    if (backup_wifi_enabled) {
        backup_wifi_enabled = false;
        backup_wifi_connected = false;
        ESP_LOGI(TAG, "Disabling backup WiFi connection");
        esp_wifi_disconnect();
    }
}

bool wifi_is_backup_connected(void) {
    return backup_wifi_connected;
}

