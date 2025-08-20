#include "h/leds.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"

static const char *TAG = "__LEDS__";

void leds_init(void) {
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    /* Configure GPIO33 as output */
    io_conf.pin_bit_mask = (1ULL << LED1_GPIO);
    gpio_config(&io_conf);
    
    /* Configure GPIO33 as output */
    io_conf.pin_bit_mask = (1ULL << LED2_GPIO);
    gpio_config(&io_conf);
    
    /* Initialize LEDs to OFF state */
    gpio_set_level(LED1_GPIO, 0);
    gpio_set_level(LED2_GPIO, 0);
    
    ESP_LOGI(TAG, "LEDs initialized on GPIO%d and GPIO%d", LED1_GPIO, LED2_GPIO);
}

void led_on(gpio_num_t led_gpio) {
    gpio_set_level(led_gpio, 1);
}

void led_off(gpio_num_t led_gpio) {
    gpio_set_level(led_gpio, 0);
}

void led_toggle(gpio_num_t led_gpio) {
    int current_level = gpio_get_level(led_gpio);
    gpio_set_level(led_gpio, !current_level);
}