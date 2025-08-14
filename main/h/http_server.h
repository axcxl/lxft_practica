#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_http_server.h"

#define ID_LEN 6
#define URL_LEN 64

extern char ID[ID_LEN + 1];
extern char URL[URL_LEN + 1];
extern bool mqtt_config_updated;


/*
 * Macro that receives as argument a string (char*) converts the + in ' '
 * and any %XX sequences from hex in their equivalent ascii characters
 */
#define URL_DECODE(s) do { \
    char *p = s, *q = s; \
    while (*p) { \
        if (*p == '+') { \
            p++; \
        } else if (*p == '%' && p[1] && p[2]) { \
            unsigned int val = 0; \
            sscanf(p + 1, "%2x", &val); \
            *q++ = (char)val; \
            p += 3; \
        } else { \
            *q++ = *p++; \
        } \
    } \
    *q = '\0'; \
} while (0)



/**
 * @brief Start HTTP server for esp configuration through hotspot
 * @return httpd_handle_t Handle to the HTTP server, or NULL on failure
 */
httpd_handle_t start_http_server(void);

/**
 * @brief LOG message for console
 */
void print_http_info(void);

#endif
