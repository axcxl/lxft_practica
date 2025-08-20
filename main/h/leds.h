#ifndef LEDS_H
#define LEDS_H

#include "driver/gpio.h"

/* LED GPIO definitions for ESP32-POE-ISO */
#define LED1_GPIO GPIO_NUM_32
#define LED2_GPIO GPIO_NUM_33

void leds_init(void);
void led_on(gpio_num_t led_gpio);
void led_off(gpio_num_t led_gpio);
void led_toggle(gpio_num_t led_gpio);

#endif