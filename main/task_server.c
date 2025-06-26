#include <string.h>
#include <stdlib.h> // Required for itoa

#include "esp_log.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "ethernet_init.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"

#include "h/https_utils.h"

static const char *TAG = "HTTPS";
char ID[ID_LEN + 1];
char URL[URL_LEN + 1];

static esp_err_t get_handler(httpd_req_t *req)
{
    char html_buffer[512];

    // Construct the HTML page
    snprintf(html_buffer, sizeof(html_buffer),
        "<!DOCTYPE html><html><head><title>ESP32 Control</title></head>"
        "<body><h1>ESP32 - MQTT Config</h1>"
        "<h3>Parameters</h3>"
        "<form method=\"post\" action=\"/update\">"
        "<b>ID: </b> <input type=\"text\" size=\"6\" maxlength=\"6\" name=\"ID\" value=\"%s\"><br><br>"
        "<b>URL: </b> <input type=\"text\" size=\"64\" maxlength=\"64\" name=\"URL\" value=\"%s\"><br><br>"
        "<input type=\"submit\" value=\"Update\">"
        "</form></body></html>",
        ID, URL);

    // Send the response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_buffer, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


/*
 * Handler for the form submission (POST /update)
 * This receives the new values, updates the global variables, and redirects back to the root page
 */
static esp_err_t post_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        ESP_LOGE(TAG, "POST content too long");
        return ESP_FAIL;
    }

    if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    /* Update the Parameters */
    char new_id[ID_LEN+1] = {0}; // ID
    if (httpd_query_key_value(buf, "ID", new_id, sizeof(new_id)) == ESP_OK) {
        printf("Found ID query parameter => %s\n", new_id);
        strncpy(ID, new_id, sizeof(ID) - 1);
        ID[sizeof(ID) - 1] = '\0';
    } else {
        ESP_LOGE(TAG, "ID query parameter not found or too long");
    }

    char new_url[URL_LEN+1] = {0}; // URL
    if (httpd_query_key_value(buf, "URL", new_url, sizeof(new_url)) == ESP_OK) {
        printf("Found URL query parameter => %s\n", new_url);
        strncpy(URL, new_url, sizeof(URL) - 1);
        URL[sizeof(URL) - 1] = '\0';
    } else {
        ESP_LOGE(TAG, "URL query parameter not found or too long");
    }

    // Redirect the browser back to the root page
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

static const httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_handler
};

static const httpd_uri_t uri_post = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = post_handler
};


/*
 * Function to start the web server
 */
httpd_handle_t start_webserver(void)
{
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    httpd_handle_t server = NULL;

    // Update config for HTTPS
    extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
    extern const unsigned char servercert_end[]   asm("_binary_servercert_pem_end");
    config.servercert = servercert_start;
    config.servercert_len = servercert_end - servercert_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    config.prvtkey_pem = prvtkey_pem_start;
    config.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    ESP_LOGI(TAG, "Starting HTTPS server on port: '%d'", config.port_secure);
    esp_err_t ret = httpd_ssl_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error starting server!");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_post);
    return server;
}
