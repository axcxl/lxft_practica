#include "h/dns_server.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

static const char *TAG = "__DNS__";

/* DNS server response buffer */
#define DNS_MAX_LEN 512
static TaskHandle_t dns_task_handle = NULL;
/* The IP address to redirect all DNS queries to (ESP32's AP IP) */
static uint32_t redirect_ip_addr = 0;


/* DNS Protocol Structures - ensures no padding between fields */
typedef struct __attribute__((__packed__))
{
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

typedef struct __attribute__((__packed__))
{
    uint16_t type;
    uint16_t class;
} dns_question_t;

typedef struct __attribute__((__packed__))
{
    uint16_t ptr_offset;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t addr_len;
    uint32_t ip_addr;
} dns_answer_t;


static void dns_server_task(void *pvParameters)
{
    char rx_buffer[DNS_MAX_LEN];
    char tx_buffer[DNS_MAX_LEN];
    uint8_t domain[128];
    
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(53);  // DNS port

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS server started on port 53");

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);

        if (len < sizeof(dns_header_t)) {
            continue;
        }

        /* Copy the request to the response buffer and remove the header */
        memcpy(tx_buffer, rx_buffer, len);
        dns_header_t *header = (dns_header_t *)tx_buffer;

        uint16_t offset = sizeof(dns_header_t);
        uint8_t *question = (uint8_t *)(tx_buffer + offset);
        
        /* _______ Extract domain name from question  _______
            DNS format: [3][w][w][w][7][e][s][p][c][o][n][f][3][c][o][m][0] --> "www.espconf.com"
        */
        uint8_t length;
        int domain_pos = 0;

        while ((length = question[0]) != 0) {
            offset++;
            /* Extract section */
            memcpy(&domain[domain_pos], &question[1], length);
            domain_pos += length;
            domain[domain_pos++] = '.';
            /* Move to the next part */
            question = (uint8_t *)(tx_buffer + offset + length);
            offset += length;
        }
        
        if (domain_pos > 0) {
            domain[domain_pos - 1] = '\0';
        } else {
            domain[0] = '\0';
        }
        offset++;  // Skip the 0 length byte at end of QNAME
        ESP_LOGD(TAG, "DNS Query for domain: %s", domain);

        /* _______ Modify the header for the response _______ */
        header->flags = htons(0x8180);  // Standard response, no error
        header->an_count = htons(1);    // One answer

        offset += sizeof(dns_question_t);   // Skip over the question section

        /* Create the answer section */
        dns_answer_t *answer = (dns_answer_t *)(tx_buffer + offset);
        answer->ptr_offset = htons(0xC00C); // Compressed name pointer to the question
        answer->type = htons(1);
        answer->class = htons(1);
        answer->ttl = htonl(60);            // TTL 60 seconds
        answer->addr_len = htons(4);
        answer->ip_addr = redirect_ip_addr; // Our redirection IP

        int response_len = offset + sizeof(dns_answer_t);

        /* Send the DNS response */
        sendto(sock, tx_buffer, response_len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
    }

    close(sock);
    vTaskDelete(NULL);
}

esp_err_t start_dns_server(esp_netif_t *ap_netif)
{
    /* Get the IP address of the AP interface */
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(ap_netif, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP IP info");
        return ret;
    }

    /* Store the IP address to redirect all DNS queries */
    redirect_ip_addr = ip_info.ip.addr;

    /* Start the DNS server task */
    if (dns_task_handle == NULL) {
        xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
        if (dns_task_handle == NULL) {
            ESP_LOGE(TAG, "Failed to create DNS server task");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}
