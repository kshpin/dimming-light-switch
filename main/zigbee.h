#pragma once

#include <stdbool.h>

void zigbee_init(void);
void zigbee_on_off_updated(bool on);
void zigbee_start_steering(void);
void zigbee_factory_reset(void);
