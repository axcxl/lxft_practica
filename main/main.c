#include "h/task_comms.h"
#include "h/task_sensors.h"
#include "h/sensor_queue.h"
#include "h/wifi.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_partition.h"
#include "esp_task_wdt.h"

const static char *TAG = "main";

#define TASK_PRIO_3     3
#define CORE0           0
#define CORE1           ((CONFIG_FREERTOS_NUMBER_OF_CORES > 1) ? 1 : tskNO_AFFINITY)

/* Task handles for watchdog monitoring */
TaskHandle_t sensor_task_handle = NULL;
TaskHandle_t comms_task_handle = NULL;


void print_partition_table(void)
{
    ESP_LOGI("PART_TABLE", "--- Printing Partition Table ---");
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);

    while (it != NULL) {
        const esp_partition_t *part = esp_partition_get(it);
        ESP_LOGI("PART_TABLE", "Name: %-10s | Type: %2d | Subtype: %2d | Offset: 0x%08lx | Size: %lu bytes",
                 part->label, part->type, part->subtype, part->address, part->size);
        it = esp_partition_next(it);
    }

    esp_partition_iterator_release(it);
    ESP_LOGI("PART_TABLE", "--------------------------------");
}


void app_main(void)
{
    static QueueHandle_t msg_queue;

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Deinitialize the watchdog and then reintitialize it with the custom config */
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 10000,  // 10 second timeout
        .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1,
        .trigger_panic = true,  // Trigger panic on timeout
    };
    
    ESP_ERROR_CHECK(esp_task_wdt_deinit());
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
    ESP_LOGI(TAG, "TWDT configured with 10s timeout");

    /* Create main queue for passing info from sensor reading to the comms part */
    msg_queue = xQueueGenericCreate(SENSQ_LEN, sizeof(sensq), queueQUEUE_TYPE_SET);
    if (msg_queue == NULL) {
        ESP_LOGE(TAG, "Error creating queue. Stopping!");
        return;
    }

    print_partition_table();

    xTaskCreatePinnedToCore(task_sensors, "core0_sensors", 4096, (void*)&msg_queue, TASK_PRIO_3, &sensor_task_handle, CORE0);
    xTaskCreatePinnedToCore(task_comms, "core1_comms", 4096, (void*)&msg_queue, TASK_PRIO_3, &comms_task_handle, CORE1);

    ESP_LOGI(TAG, "Tasks created - they will self-register with watchdog");
}