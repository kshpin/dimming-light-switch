#include "button.h"
#include "esp_attr.h"
#include "esp_timer.h"

#define DEBOUNCE_TIME_US  (50 * 1000)  // 50 ms
#define MAX_BUTTONS       2

typedef struct {
    esp_timer_handle_t debounce_timer;
    button_callback_t  callback;
    button_callback_t  long_callback;
    gpio_num_t         gpio;
    int64_t            press_time_us;
    int64_t            long_press_us;
    bool               has_long_press;
} button_state_t;

static button_state_t buttons[MAX_BUTTONS];
static int button_count = 0;
static bool isr_service_installed = false;

static void debounce_timer_cb(void *arg)
{
    button_state_t *btn = (button_state_t *)arg;
    btn->callback();
}

static void release_debounce_timer_cb(void *arg)
{
    button_state_t *btn = (button_state_t *)arg;
    int64_t held_us = esp_timer_get_time() - btn->press_time_us;
    if (held_us >= btn->long_press_us) {
        btn->long_callback();
    } else {
        btn->callback();
    }
}

static void IRAM_ATTR button_isr(void *arg)
{
    button_state_t *btn = (button_state_t *)arg;
    esp_timer_start_once(btn->debounce_timer, DEBOUNCE_TIME_US);
}

static void IRAM_ATTR long_press_anyedge_isr(void *arg)
{
    button_state_t *btn = (button_state_t *)arg;
    if (gpio_get_level(btn->gpio) == 0) {
        btn->press_time_us = esp_timer_get_time();
    } else {
        esp_timer_start_once(btn->debounce_timer, DEBOUNCE_TIME_US);
    }
}

static void install_isr_service(void)
{
    if (!isr_service_installed) {
        gpio_install_isr_service(0);
        isr_service_installed = true;
    }
}

void button_init(gpio_num_t gpio, button_callback_t callback)
{
    if (button_count >= MAX_BUTTONS) {
        return;
    }

    button_state_t *btn = &buttons[button_count++];
    btn->callback       = callback;
    btn->long_callback  = NULL;
    btn->gpio           = gpio;
    btn->has_long_press = false;
    btn->long_press_us  = 0;
    btn->press_time_us  = 0;

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

    install_isr_service();
    gpio_isr_handler_add(gpio, button_isr, btn);
}

void button_init_long_press(gpio_num_t gpio, button_callback_t short_cb,
                            button_callback_t long_cb, uint32_t long_press_ms)
{
    if (button_count >= MAX_BUTTONS) {
        return;
    }

    button_state_t *btn = &buttons[button_count++];
    btn->callback       = short_cb;
    btn->long_callback  = long_cb;
    btn->gpio           = gpio;
    btn->has_long_press = true;
    btn->long_press_us  = (int64_t)long_press_ms * 1000;
    btn->press_time_us  = 0;

    esp_timer_create_args_t timer_args = {
        .callback = release_debounce_timer_cb,
        .arg      = btn,
        .name     = "btn_longpress",
    };
    esp_timer_create(&timer_args, &btn->debounce_timer);

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io_conf);

    install_isr_service();
    gpio_isr_handler_add(gpio, long_press_anyedge_isr, btn);
}
