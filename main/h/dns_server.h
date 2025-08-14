#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include "esp_netif.h"

/**
 * @brief Start a simple DNS server that redirects all queries to the specified IP
 * @param ap_netif The network interface for the access point
 * @return esp_err_t ESP_OK on success
 */
esp_err_t start_dns_server(esp_netif_t *ap_netif);

#endif
