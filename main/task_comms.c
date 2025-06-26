#include <string.h>

#include "esp_log.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "ethernet_init.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "h/task_comms.h"
#include "h/sensor_queue.h"
#include "h/https_utils.h"

static esp_eth_handle_t *s_eth_handles = NULL;
static uint8_t s_eth_port_cnt = 0;
static bool mqtt_is_connected = false;
const static char *TAG = "comms";

void init_ethernet_and_netif(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_eth_init(&s_eth_handles, &s_eth_port_cnt));

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_config_t cfg_spi = {
        .base = &esp_netif_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };
    char if_key_str[10];
    char if_desc_str[10];
    char num_str[3];
    for (int i = 0; i < s_eth_port_cnt; i++) {
        itoa(i, num_str, 10);
        strcat(strcpy(if_key_str, "ETH_"), num_str);
        strcat(strcpy(if_desc_str, "eth"), num_str);
        esp_netif_config.if_key = if_key_str;
        esp_netif_config.if_desc = if_desc_str;
        esp_netif_config.route_prio -= i*5;
        esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);

        // attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(s_eth_handles[i])));
    }

    for (int i = 0; i < s_eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(s_eth_handles[i]));
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
        case MQTT_EVENT_CONNECTED:
            mqtt_is_connected = true;
            ESP_LOGI(TAG, "MQTT connected!");
            break;
        case MQTT_EVENT_DISCONNECTED:
            mqtt_is_connected = false;
            ESP_LOGI(TAG, "MQTT disconnected!");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

/*
 * @brief Create a board id from last part of MAC address
 *
 */
char *get_mqtt_board_id()
{
    uint8_t mac_addr[BOARD_ID_LEN];
    char *board_id;
    esp_netif_t *esp_netif = esp_netif_next_unsafe(NULL); // we can use esp_netif_next_unsafe since we one time initialize the network and we don't de-init
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
    char topic[21];
    int msg_id;
    sensq data;  // data type should be same as queue item type
    const TickType_t xTicksToWait = pdMS_TO_TICKS(1000); //read queue every 100ms
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };

    init_ethernet_and_netif();

    start_webserver();

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    while(1){
        if (xQueueReceive(*(QueueHandle_t*)msg_queue, (void *)&data, xTicksToWait) == pdTRUE) 
        {
            if(false == mqtt_is_connected)
            {
                ESP_LOGI(TAG, "received data = %.2f(%d), ignoring (mqtt not connected)", data.value, (int)data.type);
                continue;
            }

            /* Prepare topic and data to send */
            snprintf(mqttdata, sizeof(mqttdata), "%.2f",data.value);
            snprintf(topic, sizeof(topic), topic_fmt, get_mqtt_board_id(), sensq_string[data.type]);

            ESP_LOGI(TAG, "received data = %.2f (type=%d), sending to %s", data.value, (int)data.type, topic);
            msg_id = esp_mqtt_client_publish(client, topic, mqttdata, 0, 0, 0);
            switch(msg_id)
            {
                case -1:   
                    ESP_LOGI(TAG, "error publishing!");
                    break;
                case -2:
                    ESP_LOGI(TAG, "full outbox!");
                    break;
                default:
                    ESP_LOGD(TAG, "sent message %d", msg_id);
            }
        } 
    }
}