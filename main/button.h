#pragma once

#include <stdint.h>
#include "driver/gpio.h"

typedef void (*button_callback_t)(void);

void button_init(gpio_num_t gpio, button_callback_t callback);
void button_init_long_press(gpio_num_t gpio, button_callback_t short_cb,
                            button_callback_t long_cb, uint32_t long_press_ms);
