#pragma once

#include "driver/gpio.h"

typedef void (*button_callback_t)(void);

void button_init(gpio_num_t gpio, button_callback_t callback);
