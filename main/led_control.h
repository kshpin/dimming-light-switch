#pragma once

#include <stdbool.h>
#include <stdint.h>

void led_init(void);
void led_set_brightness(uint8_t level);
void led_set_on_off(bool on);
uint8_t led_get_brightness(void);
bool led_get_on_off(void);
