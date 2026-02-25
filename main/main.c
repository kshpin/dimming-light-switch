#include "led_control.h"
#include "button.h"

#define BUTTON_ON_OFF_GPIO  GPIO_NUM_2

static void on_off_button_pressed(void)
{
    led_set_on_off(!led_get_on_off());
}

void app_main(void)
{
    led_init();
    button_init(BUTTON_ON_OFF_GPIO, on_off_button_pressed);
}
