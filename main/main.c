#include "led_control.h"
#include "button.h"
#include "zigbee.h"
#include "nvs_flash.h"

#define BUTTON_ON_OFF_GPIO   GPIO_NUM_2
#define BUTTON_ZIGBEE_GPIO   GPIO_NUM_17
#define LONG_PRESS_MS        5000

static void on_off_button_pressed(void)
{
    bool new_state = !led_get_on_off();
    led_set_on_off(new_state);
    zigbee_on_off_updated(new_state);
}

static void on_zigbee_on_off(bool on)
{
    led_set_on_off(on);
}

static void on_zigbee_level(uint8_t level)
{
    led_set_brightness(level);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    led_init();
    button_init(BUTTON_ON_OFF_GPIO, on_off_button_pressed);
    button_init_long_press(BUTTON_ZIGBEE_GPIO, zigbee_start_steering,
                           zigbee_factory_reset, LONG_PRESS_MS);
    zigbee_init(on_zigbee_on_off, on_zigbee_level);
}
