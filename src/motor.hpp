#include "driver/ledc.h"

#define SERVO_GPIO      18
#define SERVO_MODE      LEDC_LOW_SPEED_MODE
#define SERVO_TIMER     LEDC_TIMER_0
#define SERVO_CHANNEL   LEDC_CHANNEL_0
#define SERVO_RES       LEDC_TIMER_13_BIT   // 8192 counts over the 20 ms period
#define SERVO_FREQ_HZ   50                  // 20 ms period

#define SERVO_MIN_US    1000   // ~ one extreme   (datasheet: ~1 ms)
#define SERVO_MID_US    1500   //   center
#define SERVO_MAX_US    2000   // ~ other extreme (datasheet: ~2 ms)

static uint32_t servo_us_to_duty(uint32_t us) {
    return (us * 8192) / 20000;   // 20 ms period, 13-bit resolution
}

static void servo_write_us(uint32_t us) {
    ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, servo_us_to_duty(us));
    ledc_update_duty(SERVO_MODE, SERVO_CHANNEL);
}

static void servo_init(void) {
    ledc_timer_config_t t = {
        .speed_mode      = SERVO_MODE,
        .duty_resolution = SERVO_RES,
        .timer_num       = SERVO_TIMER,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
        .deconfigure     = false,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));

    ledc_channel_config_t c = {
        .gpio_num   = SERVO_GPIO,
        .speed_mode = SERVO_MODE,
        .channel    = SERVO_CHANNEL,
        .intr_type  = (ledc_intr_type_t) 0,
        .timer_sel  = SERVO_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .sleep_mode = (ledc_sleep_mode_t) 0,
        .flags      = {},
        .deconfigure = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&c));
}
