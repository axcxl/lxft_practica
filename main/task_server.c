#include <string.h>

#include "esp_log.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "ethernet_init.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "h/https_utils.h"

#include "esp_ota_ops.h"
#include "esp_system.h"
#define OTA_BUFSIZE 1024

static const char *TAG = "HTTPS";

char ID[ID_LEN + 1] = "ESP";
char URL[URL_LEN + 1];
bool mqtt_config_updated = false;


static esp_err_t get_handler(httpd_req_t *req)
{
    char html_buffer[2048];

    /* Construct the HTML page */
    snprintf(html_buffer, sizeof(html_buffer),
            "<!DOCTYPE html><html><head><title>ESP32 Control</title>"
            "<style>"
            "body{font-family:system-ui,sans-serif;background:#f0f2f5;margin:0;padding:1rem}"
            "div{background:#fff;padding:1.5rem;border-radius:8px;box-shadow:0 4px 10px rgba(0,0,0,.1);max-width:500px;margin:0 auto 1rem auto}"
            "h1{color:#0056b3;margin:0 0 1rem 0;padding-bottom:.5rem;border-bottom:1px solid #eee}"
            "b{display:block;margin-bottom:.3rem;color:#555;font-weight:600}"
            "input[type=text]{width:100%%;padding:.5rem;border:1px solid #ccc;border-radius:4px;box-sizing:border-box;margin-bottom:1rem}"
            "input[type=submit]{background:#007bff;color:#fff;border:0;padding:.7rem 1.2rem;border-radius:5px;cursor:pointer;font-size:1em;transition:background .2s;width:100%%;margin-top:.5rem}"
            "input[type=submit]:hover{background:#0056b3}"
            "input[type=file]{width:100%%;border:1px solid #ccc;padding:.4rem;border-radius:4px}"
            "</style></head>"
            "<body>"
            "<div><h1>MQTT Config</h1>"
            "<form method=\"post\" action=\"/update\">"
            "<b>ID:</b><input type=\"text\" size=\"6\" maxlength=\"6\" name=\"ID\" value=\"%s\">"
            "<b>URL:</b><input type=\"text\" size=\"64\" maxlength=\"64\" name=\"URL\" value=\"%s\">"
            "<input type=\"submit\" value=\"Update Parameters\">"
            "</form></div>"
            "<div><h1>OTA Update</h1>"
            "<form method=\"post\" action=\"/ota\" enctype=\"multipart/form-data\">"
            "<input type=\"file\" name=\"firmware\">"
            "<input type=\"submit\" value=\"Update Firmware\">"
            "</form></div>"
            "</body></html>",
            ID, URL);

    /* Send the response */
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_buffer, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


/*
 *  Used to inform the user about request errors 
 */
static void send_response_page(httpd_req_t *req, const char *http_status, const char *title, const char *message)
{
    char html_buffer[1024];
    snprintf(html_buffer, sizeof(html_buffer),
            "<!DOCTYPE html><html><head><title>%s</title>"
            "<style>"
            "body{margin:0;font-family:system-ui,sans-serif;display:grid;place-content:center;min-height:100vh;background:#f0f2f5}"
            "div{background:#fff;padding:2rem;border-radius:8px;box-shadow:0 4px 12px rgba(0,0,0,0.1);text-align:center;max-width:400px}"
            "h1{margin-top:0;color:#0056b3}"
            "p{color:#555;line-height:1.6;margin-bottom:1.5rem}"
            "a{text-decoration:none;background:#007bff;color:#fff;padding:.7rem 1.2rem;border-radius:5px;transition:background .2s}"
            "a:hover{background:#0056b3}"
            "</style></head>"
            "<body><div><h1>%s</h1><p>%s</p>"
            "<a href=\"/\">Go Back</a></div></body></html>",
            title, title, message);

    httpd_resp_set_status(req, http_status);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_buffer, HTTPD_RESP_USE_STRLEN);
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
    char temp_val[URL_LEN + 1];

    /* Parse ID */
    printf("\n");
    if (httpd_query_key_value(buf, "ID", temp_val, sizeof(temp_val)) == ESP_OK) {
        URL_DECODE(temp_val);
        if (strlen(temp_val) == 0) {
            ESP_LOGW(TAG, "Received empty ID, skipping update\n");
            send_response_page(req, "400 Bad Request", "ID Update Failed", "Empty ID field");
        } else {
            ESP_LOGI(TAG, "Received ID: '%s'", temp_val);
            strncpy(ID, temp_val, ID_LEN);
            ID[ID_LEN] = '\0';
        }
    } else {
        ESP_LOGE(TAG, "ID not found in POST request");
    }

    /* Parse URL */
    if (httpd_query_key_value(buf, "URL", temp_val, sizeof(temp_val)) == ESP_OK) {
        URL_DECODE(temp_val);
        if (strlen(temp_val) == 0) {
            ESP_LOGW(TAG, "Received empty URL, skipping update\n");
            send_response_page(req, "400 Bad Request", "URL Update Failed", "Empty URL field");
        } else {
            ESP_LOGI(TAG, "Received URL: '%s'\n", temp_val);
            strncpy(URL, temp_val, URL_LEN);
            URL[URL_LEN] = '\0';
            mqtt_config_updated = true;
        }
    } else {
        ESP_LOGE(TAG, "URL not found in POST request");
    }

    /* Redirect the browser back to the root page */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}



static esp_err_t ota_update_handler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = NULL;
    char ota_write_buf[OTA_BUFSIZE + 1] = { 0 };
    char *body_start_p = NULL;
    esp_err_t err;
    char error[100];

    ESP_LOGI(TAG, "Starting OTA update...");

    /* Find the OTA partition */
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "OTA partition not found");
        send_response_page(req, "400 Bad Request", "Firmware Update Failed", "OTA partition not found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);

    /* Begin the OTA update process. We use OTA_SIZE_UNKNOWN because the total request
       length includes headers, not just the binary */
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        snprintf(error, sizeof(error), "esp_ota_begin failed (%s)", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", error);
        send_response_page(req, "400 Bad Request", "Firmware Update Failed", error);
        return ESP_FAIL;
    }

    int content_received = 0;
    int binary_file_len = 0;
    bool header_found = false;

    /* Read the request body in chunks */
    while (true) {
        int recv_len = httpd_req_recv(req, ota_write_buf, OTA_BUFSIZE);
        if (recv_len < 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Socket timeout, can retry */
                continue;
            }
            esp_ota_abort(ota_handle);
            ESP_LOGE(TAG, "Firmware reception failed");
            send_response_page(req, "400 Bad Request", "Firmware Update failed", "Reception failed");
            return ESP_FAIL;
        }

        /* First chunk - remove the header */
        if (!header_found) {
            /* Find the boundary between headers and binary data */
            body_start_p = strstr(ota_write_buf, "\r\n\r\n");
            
            if (body_start_p != NULL) {
                /* Move the pointer past the "\r\n\r\n" */
                body_start_p += 4; 
                
                /* Calculate the length of the binary data in this first chunk */
                int data_len = recv_len - (body_start_p - ota_write_buf);
                
                /* Write the first chunk of binary data */
                err = esp_ota_write(ota_handle, (const void *)body_start_p, data_len);
                if (err != ESP_OK) {
                    snprintf(error, sizeof(error), "esp_ota_write failed (%s)", esp_err_to_name(err));
                    ESP_LOGE(TAG, "%s", error);
                    send_response_page(req, "400 Bad Request", "Firmware Write Failed", error);
                    return ESP_FAIL;
                }
                header_found = true;
                binary_file_len += data_len;
            }
        } else {
            /* This is a subsequent chunk, containing only binary data */
            err = esp_ota_write(ota_handle, (const void *)ota_write_buf, recv_len);
            if (err != ESP_OK) {
                snprintf(error, sizeof(error), "esp_ota_write failed (%s)", esp_err_to_name(err));
                ESP_LOGE(TAG, "%s", error);
                send_response_page(req, "400 Bad Request", "Firmware Write Failed", error);
                return ESP_FAIL;
            }
            binary_file_len += recv_len;
        }

        content_received += recv_len;
        ESP_LOGD(TAG, "Received %d bytes, written %d bytes of binary", content_received, binary_file_len);

        /* Check if we have finished receiving */
        if (recv_len == 0) {
            break;
        }
    }

    ESP_LOGI(TAG, "Total binary data written: %d bytes", binary_file_len);

    /* Finish the OTA update */
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted or incomplete");
            send_response_page(req, "400 Bad Request", "Firmware Update Failed", "Image validation failed, image is corrupted or incomplete");
        } else {
            snprintf(error, sizeof(error), "esp_ota_end failed (%s)!", esp_err_to_name(err));
            ESP_LOGE(TAG, "%s", error);
            send_response_page(req, "400 Bad Request", "Firmware Write Failed", error);
        }
        return ESP_FAIL;
    }

    /* Set the boot partition to the new one */
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        snprintf(error, sizeof(error), "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", error);
        send_response_page(req, "400 Bad Request", "Firmware Write Failed", error);
        return ESP_FAIL;
    }

    /* Send a success response and then restart */
    ESP_LOGI(TAG, "OTA Update successful! Rebooting...");
    const char *success_msg = "Firmware update successful. Rebooting now...";
    httpd_resp_send(req, success_msg, HTTPD_RESP_USE_STRLEN);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

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


static const httpd_uri_t uri_ota = {
    .uri = "/ota",
    .method = HTTP_POST,
    .handler = ota_update_handler
};


httpd_handle_t start_webserver(void)
{
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    httpd_handle_t server = NULL;

    /* Import the certificates */
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
    httpd_register_uri_handler(server, &uri_ota);
    return server;
}
