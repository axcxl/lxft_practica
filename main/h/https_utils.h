#ifndef HTTPS_UTILS_H
#define HTTPS_UTILS_H

#include "esp_http_server.h"
#include "esp_https_server.h"

#define ID_LEN 6
#define URL_LEN 64

httpd_handle_t start_webserver(void);

#endif