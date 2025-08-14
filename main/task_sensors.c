#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "bmp280.h"
#include "freertos/FreeRTOS.h"
#include "h/sensor_queue.h"
#include "h/task_sensors.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"

const static char *TAG = "__SENSORS__";

float http_temp = 0;
float http_hum = 0;
float http_pres = 0;

/*  NOTE: we have a BME280 sensor on board, but the
 *  driver is for both the BME and BMP. Will use BME280 
 *  all functions here, it is not a mistake.
 */
bmp280_t *init_bme280()
{
    bmp280_params_t params;
    bmp280_init_default_params(&params);
    bmp280_t *dev = (bmp280_t*)malloc(sizeof(bmp280_t));
    memset(dev, 0, sizeof(bmp280_t));

    /* On our boards, BME280 address is 0x77 */
    /* For I2C pins check the UEXT connector */
    ESP_ERROR_CHECK(bmp280_init_desc(dev, BMP280_I2C_ADDRESS_1, 0, GPIO_NUM_13, GPIO_NUM_16));
    ESP_ERROR_CHECK(bmp280_init(dev, &params));

    bool bme280p = dev->id == BME280_CHIP_ID;
    printf("BMP280: found %s\n", bme280p ? "BME280" : "BMP280");

    return dev;
}


void read_send_bme280(bmp280_t *dev, QueueHandle_t* queue)
{
    float pressure, temperature, humidity;
    sensq to_send;

    /* Read all info from sensor */
    if (bmp280_read_float(dev, &temperature, &pressure, &humidity) != ESP_OK)
    {
        ESP_LOGE(TAG, "Temperature/pressure reading failed");
        return;
    }

    /* Update data for http server */
    http_temp = temperature;
    http_hum = humidity;
    http_pres = pressure;

    /* Put it in the queue one at a time */
    to_send.value = temperature;
    to_send.type = TEMP;
    if (xQueueGenericSend(*(QueueHandle_t*)queue, (void *)&to_send, portMAX_DELAY, queueSEND_TO_BACK) != pdTRUE) 
    {
        ESP_LOGE(TAG, "Queue full");
    }

    to_send.value = pressure;
    to_send.type = PRES;
    if (xQueueGenericSend(*(QueueHandle_t*)queue, (void *)&to_send, portMAX_DELAY, queueSEND_TO_BACK) != pdTRUE) 
    {
        ESP_LOGE(TAG, "Queue full");
    }

    to_send.value = humidity;
    to_send.type = HUM;
    if (xQueueGenericSend(*(QueueHandle_t*)queue, (void *)&to_send, portMAX_DELAY, queueSEND_TO_BACK) != pdTRUE) 
    {
        ESP_LOGE(TAG, "Queue full");
    }

}


void task_sensors(void* msg_queue)
{ 
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xTimeIncrement = pdMS_TO_TICKS(5000); // run every 5 seconds
    bmp280_t *dev_bme280;
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    bool added_to_wdt = false;

    /* Suppress I2C master pull-up warning since everything works fine */
    esp_log_level_set("i2c.master", ESP_LOG_ERROR);

    ESP_ERROR_CHECK(i2cdev_init());
    dev_bme280 = init_bme280();
    
    /* Wait a bit for main to finish initialization */
    vTaskDelay(pdMS_TO_TICKS(200));
    
    /* Try to add task to watchdog monitoring */
    esp_err_t err = esp_task_wdt_add(current_task);
    if (err == ESP_OK) {
        added_to_wdt = true;
        ESP_LOGI(TAG, "Sensor task added to watchdog monitoring");
    } else {
        ESP_LOGW(TAG, "Could not add sensor task to watchdog: %s", esp_err_to_name(err));
    }
    
    while(1){
        /* Feed the watchdog only if is active */
        if (added_to_wdt) {
            esp_task_wdt_reset();
            ESP_LOGD(TAG, "Sensor task watchdog fed");
        }
        
        vTaskDelayUntil(&xLastWakeTime, xTimeIncrement);

        read_send_bme280(dev_bme280, msg_queue);
    }
}