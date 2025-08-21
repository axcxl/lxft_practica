#include "h/task_comms.h"
#include "h/sensor_queue.h"
#include "h/wifi.h"
#include "h/http_server.h"
#include <string.h>
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "ethernet_init.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "lwip/ip4_addr.h"
#include "esp_task_wdt.h"

const static char *TAG = "__COMMS__";


static uint8_t eth_port_cnt = 0;
static esp_eth_handle_t *eth_handles = NULL;
static bool ip_acquired = false;

static bool mqtt_is_connected = false;
static esp_mqtt_client_handle_t client = NULL;

/* Certificates for MQTTS */
extern const uint8_t client_cert_pem_start[] asm("_binary_client_esp1_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_esp1_crt_end");

extern const uint8_t client_key_pem_start[] asm("_binary_client_esp1_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_esp1_key_end");

extern const uint8_t ca_cert_pem_start[] asm("_binary_ca_crt_start");
extern const uint8_t ca_cert_pem_end[] asm("_binary_ca_crt_end");


/* Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down - activating WiFi backup");
        ip_acquired = false;
        mqtt_is_connected = false;
        /*  Enable WiFi backup when Ethernet disconnects */
        wifi_connect_backup();
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}


static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}


/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "MQTT Event: Trying to connect");
            break;
        case MQTT_EVENT_CONNECTED:
            mqtt_is_connected = true;
            ESP_LOGI(TAG, "MQTT Event: Connected!");
            break;
        case MQTT_EVENT_DISCONNECTED:
            mqtt_is_connected = false;
            ESP_LOGE(TAG, "MQTT Event: Disconnected!");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT Event: Published");
            break;
        default:
            ESP_LOGE(TAG, "MQTT Event not handled - id:%d", event->event_id);
            break;
    }
}


static void config_mqtt_protocol() {
    printf("Initializing MQTT Protocol\nUpdated config: %d\n", mqtt_config_updated);
    printf("ID: %s\n", ID);
    printf("URL: %s\n\n", URL);

    if (client == NULL) {
        const esp_mqtt_client_config_t mqtt_cfg = {
            .broker.address.uri = CONFIG_BROKER_URL,
            .broker.verification.certificate = (const char *)ca_cert_pem_start,
            .broker.verification.certificate_len = ca_cert_pem_end - ca_cert_pem_start,
            .broker.verification.common_name = "localhost",
            .credentials = {
                .authentication = {
                    .certificate = (const char *)client_cert_pem_start,
                    .certificate_len = client_cert_pem_end - client_cert_pem_start,
                    .key = (const char *)client_key_pem_start,
                    .key_len = client_key_pem_end - client_key_pem_start,
                },
            }
        };

        client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        esp_mqtt_client_start(client);

    } else if (mqtt_config_updated) {
        esp_mqtt_client_destroy(client);

        const esp_mqtt_client_config_t mqtt_cfg = {
            .broker.address.uri = URL,
            .broker.verification.certificate = (const char *)ca_cert_pem_start,
            .broker.verification.certificate_len = ca_cert_pem_end - ca_cert_pem_start,
            .broker.verification.common_name = "localhost",
            .credentials = {
                .authentication = {
                    .certificate = (const char *)client_cert_pem_start,
                    .certificate_len = client_cert_pem_end - client_cert_pem_start,
                    .key = (const char *)client_key_pem_start,
                    .key_len = client_key_pem_end - client_key_pem_start,
                },
            }
        };

        client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        esp_mqtt_client_start(client);
        mqtt_config_updated = false;

    } else {
        esp_mqtt_client_reconnect(client);
    }
}


static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    switch (event_id) {
        case IP_EVENT_ETH_GOT_IP:
            ESP_LOGI(TAG, "~~~~~~~~~~~ Ethernet Got IP Address ~~~~~~~~~~~");
            ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
            ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
            ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
            ESP_LOGI(TAG, "~~~~~~~~~~~\n");
            /*  Disable WiFi backup when Ethernet is available */
            ip_acquired = true;
            if (wifi_is_backup_connected()) {
                ESP_LOGI(TAG, "Ethernet available - disabling WiFi backup");
                wifi_disconnect_backup();
            }
            /* Update MQTT protocol */
            config_mqtt_protocol();
            break;
        
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "~~~~~~~~~~~ WiFi Backup Got IP Address ~~~~~~~~~~~");
            ESP_LOGI(TAG, "WIFIIP:" IPSTR, IP2STR(&ip_info->ip));
            ESP_LOGI(TAG, "WIFIMASK:" IPSTR, IP2STR(&ip_info->netmask));
            ESP_LOGI(TAG, "WIFIGW:" IPSTR, IP2STR(&ip_info->gw));
            ESP_LOGI(TAG, "~~~~~~~~~~~\n");
            /* Use WIFI only if Ethernet is not available */
            if (!ip_acquired) {
                ip_acquired = true;
                config_mqtt_protocol();
            }
            break;
        default:
            ESP_LOGD(TAG, "IP event not handled");
            break;
    }
}


void init_ethernet_and_netif(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));
    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_config_t cfg_spi = {
        .base = &esp_netif_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };

    char if_key_str[10];
    char if_desc_str[10];
    char num_str[3];
    for (int i = 0; i < eth_port_cnt; i++) {
        itoa(i, num_str, 10);
        strcat(strcpy(if_key_str, "ETH_"), num_str);
        strcat(strcpy(if_desc_str, "eth"), num_str);
        esp_netif_config.if_key = if_key_str;
        esp_netif_config.if_desc = if_desc_str;
        esp_netif_config.route_prio -= i*5;
        esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);

        /* Enable DHCP client */
        ESP_ERROR_CHECK(esp_netif_dhcpc_start(eth_netif));

        /* attach the Ethernet driver to TCP/IP stack */
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
    }

    /* Initialize WiFi in AP+STA mode */
    wifi_init_ap_sta_mode();

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, NULL));

    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }
}


/*
 * @brief Create a board id from last part of MAC address
 */
char *get_mqtt_board_id()
{
    uint8_t mac_addr[BOARD_ID_LEN];
    char *board_id;
    esp_netif_t *esp_netif = esp_netif_next_unsafe(NULL);
    esp_eth_handle_t eth_hndl = esp_netif_get_io_driver(esp_netif);

    esp_eth_ioctl(eth_hndl, ETH_CMD_G_MAC_ADDR, mac_addr);

    board_id = (char *)malloc(sizeof(char) * (BOARD_ID_LEN));
    snprintf(board_id, (BOARD_ID_LEN + 1), "%02x%02x%02x", mac_addr[3], mac_addr[4], mac_addr[5]);

    return board_id;
}


void task_comms(void* msg_queue)
{
    char mqttdata[11];
    char topic_fmt[] = "/sensor_%s/%s";
    char topic[40];
    int msg_id;
    sensq data;
    const TickType_t xTicksToWait = pdMS_TO_TICKS(1000);
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    bool added_to_wdt = false;

    init_ethernet_and_netif();

    start_http_server();
    
    ESP_LOGI(TAG, "Board ID: %s", ID);
    /* Wait for main to finish initialization */
    vTaskDelay(pdMS_TO_TICKS(200));
    
    /* Try to add task to watchdog monitoring */
    esp_err_t err = esp_task_wdt_add(current_task);
    if (err == ESP_OK) {
        added_to_wdt = true;
        ESP_LOGI(TAG, "Comms task added to watchdog monitoring");
    } else {
        ESP_LOGW(TAG, "Could not add comms task to watchdog: %s", esp_err_to_name(err));
    }

    /* Main Loop */
    while(1){
        /* Feed the watchdog only if is active */
        if (added_to_wdt) {
            esp_task_wdt_reset();
            ESP_LOGD(TAG, "Comms task watchdog fed");
        }
        
        if (mqtt_config_updated) {
            config_mqtt_protocol();
        }

        if (xQueueReceive(*(QueueHandle_t*)msg_queue, (void *)&data, xTicksToWait) == pdTRUE) 
        {
            if(ip_acquired == false)
            {
                ESP_LOGW(TAG, "Received data = %.2f(%d), ignoring (network not ready)", data.value, (int)data.type);
                continue;
            } else if (mqtt_is_connected == false) {
                ESP_LOGW(TAG, "Received data = %.2f(%d), ignoring (mqtt not ready)", data.value, (int)data.type);
                continue;
            }

            /* Prepare topic and data to send */
            snprintf(mqttdata, sizeof(mqttdata), "%.2f", data.value);
            snprintf(topic, sizeof(topic), topic_fmt, ID, sensq_string[data.type]);

            ESP_LOGI(TAG, "Received data = %.2f (type=%d), sending to %s", data.value, (int)data.type, topic);
            msg_id = esp_mqtt_client_publish(client, topic, mqttdata, 0, 1, 0);
            if (msg_id == -1) {
                ESP_LOGE(TAG, "Error publishing! Queue might be full or client not connected.");
            } else {
                ESP_LOGD(TAG, "Sent publish, msg_id=%d", msg_id);
            }
        } 
    }
    
    esp_mqtt_client_destroy(client);
    vTaskDelete(NULL);
}