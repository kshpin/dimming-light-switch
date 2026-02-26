#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*zigbee_on_off_cb_t)(bool on);
typedef void (*zigbee_level_cb_t)(uint8_t level);

void zigbee_init(zigbee_on_off_cb_t on_off_cb, zigbee_level_cb_t level_cb);
void zigbee_on_off_updated(bool on);
void zigbee_start_steering(void);
void zigbee_factory_reset(void);
