#include "button.h"
#include "esp_attr.h"
#include "esp_timer.h"

#define DEBOUNCE_TIME_US  (50 * 1000)  // 50 ms
#define MAX_BUTTONS       2

typedef struct {
    esp_timer_handle_t debounce_timer;
    button_callback_t  callback;
} button_state_t;

static button_state_t buttons[MAX_BUTTONS];
static int button_count = 0;
static bool isr_service_installed = false;

static void debounce_timer_cb(void *arg)
{
    button_state_t *btn = (button_state_t *)arg;
    btn->callback();
}

static void IRAM_ATTR button_isr(void *arg)
{
    button_state_t *btn = (button_state_t *)arg;
    esp_timer_start_once(btn->debounce_timer, DEBOUNCE_TIME_US);
}

void button_init(gpio_num_t gpio, button_callback_t callback)
{
    if (button_count >= MAX_BUTTONS) {
        return;
    }

    button_state_t *btn = &buttons[button_count++];
    btn->callback = callback;

    esp_timer_create_args_t timer_args = {
        .callback = debounce_timer_cb,
        .arg      = btn,
        .name     = "btn_debounce",
    };
    esp_timer_create(&timer_args, &btn->debounce_timer);

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    if (!isr_service_installed) {
        gpio_install_isr_service(0);
        isr_service_installed = true;
    }
    gpio_isr_handler_add(gpio, button_isr, btn);
}
