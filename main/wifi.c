#include "h/wifi.h"
#include "h/dns_server.h"  // Add DNS server header

#include <string.h>

static const char *TAG = "__WIFI__";


static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch(event_id) {
        case WIFI_EVENT_STA_START:
             ESP_LOGI(TAG, "Starting WIFI...\n");
            break;

        case IP_EVENT_STA_GOT_IP:
            ip_event_got_ip_t *event_ip = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event_ip->ip_info.ip));
            break;

        case WIFI_EVENT_STA_CONNECTED:
            printf("\nWIFI CONNECTED\n");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGE(TAG, "WIFI Disconected");
            break;
        
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "New Device Connected");
            break;

        case WIFI_EVENT_AP_STADISCONNECTED:
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(TAG, "Device Disconnected");
            break;

        default:
            ESP_LOGI(TAG, "IP event not handled");
    }
}


void wifi_init_softap(void) {
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = WIFI_MAX_CONNS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    
    /* Configure custom IP address */
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 11, 111);
    IP4_ADDR(&ip_info.gw, 192, 168, 11, 111);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    
    /* Apply the IP configuration */
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_dhcps_start(ap_netif);
    
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Start DNS server for captive portal */
    ESP_ERROR_CHECK(start_dns_server(ap_netif));

    ESP_LOGI(TAG, "WiFi hotspot started. SSID:%s password:%s channel:%d IP:192.168.11.111",
             WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
}
