#include "led_control.h"
#include <math.h>
#include "driver/ledc.h"

#define LED_GPIO          6
#define LEDC_TIMER        LEDC_TIMER_0
#define LEDC_CHANNEL      LEDC_CHANNEL_0
#define LEDC_SPEED_MODE   LEDC_LOW_SPEED_MODE
#define LEDC_RESOLUTION   LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY    4000
#define LEDC_MAX_DUTY     8191  // (1 << 13) - 1
#define DUTY_FULLY_OFF    LEDC_MAX_DUTY

// Gamma LUT: index 0 = level 0 (off), indices 1-254 = gamma-corrected inverted duty
static uint32_t gamma_lut[255];

static uint8_t  current_brightness = 254;
static bool     current_on_off     = false;

static void build_gamma_lut(void)
{
    gamma_lut[0] = DUTY_FULLY_OFF;
    for (int i = 1; i <= 254; i++) {
        uint32_t gamma_duty = (uint32_t)(pow((double)i / 254.0, 2.2) * (double)LEDC_MAX_DUTY);
        gamma_lut[i] = LEDC_MAX_DUTY - gamma_duty;
    }
}

static void apply_duty(uint32_t duty)
{
    ledc_set_duty(LEDC_SPEED_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_SPEED_MODE, LEDC_CHANNEL);
}

void led_init(void)
{
    build_gamma_lut();

    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_SPEED_MODE,
        .duty_resolution = LEDC_RESOLUTION,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = LEDC_FREQUENCY,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t channel_cfg = {
        .gpio_num   = LED_GPIO,
        .speed_mode = LEDC_SPEED_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .duty       = DUTY_FULLY_OFF,
        .hpoint     = 0,
    };
    ledc_channel_config(&channel_cfg);
}

void led_set_brightness(uint8_t level)
{
    current_brightness = level;
    if (current_on_off) {
        apply_duty(gamma_lut[level]);
    }
}

void led_set_on_off(bool on)
{
    current_on_off = on;
    if (on) {
        apply_duty(gamma_lut[current_brightness]);
    } else {
        apply_duty(DUTY_FULLY_OFF);
    }
}

uint8_t led_get_brightness(void)
{
    return current_brightness;
}

bool led_get_on_off(void)
{
    return current_on_off;
}
