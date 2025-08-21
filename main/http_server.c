#include "h/http_server.h"
#include "h/task_sensors.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include <string.h>

#define OTA_BUFSIZE 1024

static const char *TAG = "__HTTP__";


char ID[ID_LEN + 1] = "ESP-1";
char URL[URL_LEN + 1] = CONFIG_BROKER_URL;
bool mqtt_config_updated = false;


void print_http_info(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║ HTTP Configuration Portal: http://192.168.11.111 ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
}


/* Portal detection URLs */
static const char *CAPTIVE_PORTAL_URLS[] = {
    "/generate_204",            // Android
    "/gen_204",                 // Android
    "/generate204",             // Android (alternative)
    "/canonical.html",          // Ubuntu/Linux
    "/connecttest.txt",         // Windows
    "/redirect",                // Windows
    "/hotspot-detect.html",     // iOS
    "/library/test/success.html", // iOS
    "/success.txt",             // MacOS
    "/ncsi.txt",                // Windows
    "/fwlink/",                 // Windows
    "/mobile/status.php",       // Some Android variants
    "/check_network_status.txt", // Some devices
    NULL                        // End marker
};


/* Check if a URL is a captive portal detection URL */
static bool is_captive_portal_url(const char *uri)
{
    for (int i = 0; CAPTIVE_PORTAL_URLS[i]; i++) {
        if (strncmp(uri, CAPTIVE_PORTAL_URLS[i], strlen(CAPTIVE_PORTAL_URLS[i])) == 0) {
            return true;
        }
    }
    return false;
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

/* Configuration handler for root URL */
static esp_err_t config_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Config page request");
    
    char html_buffer[2048];

    /* MAIN HTML page */
    snprintf(html_buffer, sizeof(html_buffer),
        "<!DOCTYPE html><html><head><title>ESP32 Control</title>"
        "<style>"
        "body{font-family:system-ui,sans-serif;background:#f0f2f5;margin:0;padding:1rem}"
        "div{background:#fff;padding:1.5rem;border-radius:8px;box-shadow:0 4px 10px rgba(0,0,0,.1);max-width:500px;margin:0 auto 1rem auto}"
        "h1{color:#0056b3;margin:0 0 1rem 0;padding-bottom:.5rem;border-bottom:1px solid #eee}"
        "b{display:block;margin-bottom:.3rem;color:#555;font-weight:600}"
        "p b{display:inline;font-weight:600;margin-right:.5rem}" /* Style for sensor labels */
        "input[type=text]{width:100%%;padding:.5rem;border:1px solid #ccc;border-radius:4px;box-sizing:border-box;margin-bottom:1rem}"
        "input[type=submit]{background:#007bff;color:#fff;border:0;padding:.7rem 1.2rem;border-radius:5px;cursor:pointer;font-size:1em;transition:background .2s;width:100%%;margin-top:.5rem}"
        "input[type=submit]:hover{background:#0056b3}"
        "input[type=file]{width:100%%;color:#555}"
        "input[type=file]::file-selector-button{background:#5c677d;color:#fff;border:0;padding:.6rem 1rem;border-radius:5px;cursor:pointer;transition:background .2s;margin-right:1rem}"
        "input[type=file]::file-selector-button:hover{background:#4a5467}"
        "</style></head>"
        "<body>"
        "<div><h1>Sensor</h1>"
        "<p><b>Temperature:</b>%.1f &deg;C</p>"
        "<p><b>Humidity:</b>%.1f %%</p>"
        "<p><b>Pressure:</b>%.1f hPa</p>"
        "</div>"
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
        http_temp, http_hum, http_pres, ID, URL);

    /* Send the response */
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_send(req, html_buffer, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t update_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        ESP_LOGE(TAG, "POST content too long");
        send_response_page(req, "400 Bad Request", "Request Too Large", "POST content too long");
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
    if (httpd_query_key_value(buf, "ID", temp_val, sizeof(temp_val)) == ESP_OK) {
        URL_DECODE(temp_val);
        if (strlen(temp_val) == 0) {
            ESP_LOGW(TAG, "Received empty ID, skipping update");
            send_response_page(req, "400 Bad Request", "ID Update Failed", "Empty ID field");
            return ESP_OK;
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
            ESP_LOGW(TAG, "Received empty URL, skipping update");
            send_response_page(req, "400 Bad Request", "URL Update Failed", "Empty URL field");
            return ESP_OK;
        } else {
            ESP_LOGI(TAG, "Received URL: '%s'", temp_val);
            strncpy(URL, temp_val, URL_LEN);
            URL[URL_LEN] = '\0';
            mqtt_config_updated = true;
        }
    } else {
        ESP_LOGE(TAG, "URL not found in POST request");
    }

    /* Send a success response */
    char html_response[] = "<!DOCTYPE html><html><head><title>Update Successful</title>"
                          "<meta http-equiv=\"refresh\" content=\"2;URL='/'\">"
                          "<style>body{font-family:system-ui,sans-serif;display:grid;place-content:center;min-height:100vh;background:#f0f2f5}"
                          "div{background:#fff;padding:2rem;border-radius:8px;box-shadow:0 4px 12px rgba(0,0,0,0.1);text-align:center}"
                          "h1{color:#28a745}</style></head>"
                          "<body><div><h1>Update Successful!</h1>"
                          "<p>Configuration has been updated. Returning to the main page...</p></div></body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, html_response, HTTPD_RESP_USE_STRLEN);

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
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    bool added_to_wdt = false;

    ESP_LOGI(TAG, "Starting OTA update...");

    /*  Try to add current task to watchdog monitoring */
    err = esp_task_wdt_add(current_task);
    if (err == ESP_OK) {
        added_to_wdt = true;
        esp_task_wdt_reset();
        ESP_LOGD(TAG, "OTA task added to watchdog monitoring");
    } else {
        ESP_LOGW(TAG, "Could not add OTA task to watchdog: %s", esp_err_to_name(err));
    }

    /* Find the OTA partition */
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "OTA partition not found");
        if (added_to_wdt) esp_task_wdt_delete(current_task);
        send_response_page(req, "400 Bad Request", "Firmware Update Failed", "OTA partition not found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);

    /* Begin the OTA update process */
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        snprintf(error, sizeof(error), "esp_ota_begin failed (%s)", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", error);
        if (added_to_wdt) esp_task_wdt_delete(current_task);
        send_response_page(req, "400 Bad Request", "Firmware Update Failed", error);
        return ESP_FAIL;
    }

    int content_received = 0;
    int binary_file_len = 0;
    bool header_found = false;
    int chunk_count = 0;

    /* Read the request body in chunks */
    while (true) {
        /* Feed watchdog every 20 chunks to prevent timeout during large uploads */
        if (added_to_wdt && (++chunk_count % 20 == 0)) {
            esp_task_wdt_reset();
            ESP_LOGD(TAG, "OTA watchdog fed (chunk %d)", chunk_count);
        }

        int recv_len = httpd_req_recv(req, ota_write_buf, OTA_BUFSIZE);
        if (recv_len < 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Socket timeout, can retry */
                continue;
            }
            esp_ota_abort(ota_handle);
            ESP_LOGE(TAG, "Firmware reception failed");
            if (added_to_wdt) esp_task_wdt_delete(current_task);
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
                    if (added_to_wdt) esp_task_wdt_delete(current_task);
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
                if (added_to_wdt) esp_task_wdt_delete(current_task);
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

    /* Feed watchdog before finalizing OTA */
    if (added_to_wdt) {
        esp_task_wdt_reset();
    }

    /* Finish the OTA update */
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        if (added_to_wdt) esp_task_wdt_delete(current_task);
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
        if (added_to_wdt) esp_task_wdt_delete(current_task);
        send_response_page(req, "400 Bad Request", "Firmware Write Failed", error);
        return ESP_FAIL;
    }

    /* Send a success response and then restart */
    ESP_LOGI(TAG, "OTA Update successful! Rebooting...");
    const char *success_msg = "Firmware update successful. Rebooting now...";
    httpd_resp_send(req, success_msg, HTTPD_RESP_USE_STRLEN);
    
    /* Clean up watchdog before restart */
    if (added_to_wdt) {
        esp_task_wdt_delete(current_task);
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

/* Favicon handler - prevents 404 errors */
static esp_err_t favicon_handler(httpd_req_t *req)
{
    /* Return a minimal 1x1 transparent PNG */
    const uint8_t favicon_ico[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
        0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x37, 0x6E, 0xF9, 0x24, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
    };

    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
    httpd_resp_send(req, (const char*)favicon_ico, sizeof(favicon_ico));
    return ESP_OK;
}

/* Default handler for all HTTP requests - redirects to root */
static esp_err_t http_handler(httpd_req_t *req)
{
    ESP_LOGD(TAG, "HTTP request for: %s", req->uri);  // Changed to DEBUG level to reduce noise
    
    /* Simple HTML response that redirects to our HTTP server */
    const char *html_response = "<!DOCTYPE html>"
                               "<html>"
                               "<head>"
                               "<title>ESP32 Configuration Portal</title>"
                               "<meta http-equiv=\"refresh\" content=\"0;URL='http://192.168.11.111/'\">"
                               "</head>"
                               "<body>"
                               "<p>Redirecting to ESP32 Configuration Portal...</p>"
                               "<p>If you are not redirected automatically, <a href='http://192.168.11.111/'>click here</a>.</p>"
                               "</body>"
                               "</html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    
    /* For captive portal detection URLs, set appropriate headers */
    if (is_captive_portal_url(req->uri)) {
        /* Some devices look for specific status codes */
        httpd_resp_set_status(req, "200 OK");
    } else {
        /* For all other URLs, do a standard redirect */
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.11.111/");
    }
    
    httpd_resp_send(req, html_response, strlen(html_response));
    return ESP_OK;
}

httpd_uri_t uri_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = config_handler
};
    
httpd_uri_t uri_update = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = update_handler
};

httpd_uri_t uri_ota = {
    .uri = "/ota",
    .method = HTTP_POST,
    .handler = ota_update_handler
};

httpd_uri_t uri_favicon = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_handler
};

httpd_uri_t uri_any = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = http_handler
};


httpd_handle_t start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    /* Optimize server config for better reliability and reduce recv errors */
    config.max_uri_handlers = 25;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 2;       // Reduced timeout
    config.send_wait_timeout = 2;       // Reduced timeout
    config.max_open_sockets = 4;        // Smaller to prevent issues
    config.backlog_conn = 2;            // Minimal backlog
    config.task_priority = 5;           // Lower priority
    config.stack_size =  8192;
    config.close_fn = NULL;             // Let system handle cleanup
    
    httpd_handle_t server = NULL;
    
    ESP_LOGI(TAG, "Starting HTTP server on port: %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Error starting HTTP server!");
        return NULL;
    }
    
    /* Register handlers - favicon first to avoid conflicts */
    httpd_register_uri_handler(server, &uri_favicon);
    httpd_register_uri_handler(server, &uri_root);
    httpd_register_uri_handler(server, &uri_update);
    httpd_register_uri_handler(server, &uri_ota);
    
    /* Register captive portal detection URLs (excluding favicon) */
    for (int i = 0; CAPTIVE_PORTAL_URLS[i]; i++) {
        httpd_uri_t uri = {
            .uri = CAPTIVE_PORTAL_URLS[i],
            .method = HTTP_GET,
            .handler = http_handler
        };
        httpd_register_uri_handler(server, &uri);
    }
    
    /* Register catch-all handler last */
    httpd_register_uri_handler(server, &uri_any);
    
    print_http_info();
    return server;
}
