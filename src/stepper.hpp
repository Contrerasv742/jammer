/*
 * stepper.hpp — L298N stepper sweep module
 * ----------------------------------------
 * Header-only, matching the static-inline style of motor.hpp.
 * Provides a simple back-and-forth sweep plus the base primitives you need
 * to build a scan/detect loop on top (single-step, position, release).
 *
 * The four L298N inputs are driven directly (no STEP/DIR); a GPTimer paces
 * the steps in the background so the sweep is non-blocking — your detection
 * loop can run in app_main and just read stepper_position() as it goes.
 *
 * Wiring:
 *   IN1..IN4 GPIO -> L298N IN1..IN4    Coil A = OUT1/OUT2, Coil B = OUT3/OUT4
 *   L298N ENA,ENB : jumpered to 5V (always enabled)
 *   L298N GND     : MUST be common with the ESP32 GND
 */
#pragma once

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"

/* ---- Wiring: four L298N inputs (avoid GPIO18 = servo, and the I2C pins) ---- */
#define STEP_IN1_GPIO   GPIO_NUM_32   /* Coil A */
#define STEP_IN2_GPIO   GPIO_NUM_33
#define STEP_IN3_GPIO   GPIO_NUM_25   /* Coil B */
#define STEP_IN4_GPIO   GPIO_NUM_26

/* ---- Motor / sweep parameters (tune to your motor) ---- */
#define STEP_STEPS_PER_REV   200      /* full-step count for one shaft rev (NEMA17=200) */
#define STEP_SWEEP_STEPS     100      /* how many steps wide the sweep is */
#define STEP_INTERVAL_US     2500     /* time between steps; smaller = faster */

/* Full-step, two-phase-on sequence: {IN1, IN2, IN3, IN4} */
static const uint8_t kStepSeq[4][4] = {
    {1, 0, 1, 0},
    {0, 1, 1, 0},
    {0, 1, 0, 1},
    {1, 0, 0, 1},
};
#define STEP_SEQ_LEN 4

/* ---- Internal state (ISR writes position; readers use stepper_position()) ---- */
static gptimer_handle_t s_step_timer = nullptr;
static volatile int32_t s_step_position = 0;   /* signed step count = your angle coordinate */
static int  s_step_seq_idx  = 0;
static bool s_step_forward  = true;

/* Drive the four inputs to one pattern in the sequence */
static inline void stepper_apply(int idx)
{
    gpio_set_level(STEP_IN1_GPIO, kStepSeq[idx][0]);
    gpio_set_level(STEP_IN2_GPIO, kStepSeq[idx][1]);
    gpio_set_level(STEP_IN3_GPIO, kStepSeq[idx][2]);
    gpio_set_level(STEP_IN4_GPIO, kStepSeq[idx][3]);
}

/* Timer callback: advance one step, bounce at the sweep bounds */
static bool IRAM_ATTR stepper_on_alarm(gptimer_handle_t timer,
                                       const gptimer_alarm_event_data_t *edata,
                                       void *ctx)
{
    int32_t pos = s_step_position;   /* read volatile once, work on a local */
    if (s_step_forward) {
        s_step_seq_idx = (s_step_seq_idx + 1) % STEP_SEQ_LEN;
        if (++pos >= STEP_SWEEP_STEPS) s_step_forward = false;
    } else {
        s_step_seq_idx = (s_step_seq_idx + STEP_SEQ_LEN - 1) % STEP_SEQ_LEN;
        if (--pos <= 0) s_step_forward = true;
    }
    s_step_position = pos;           /* single volatile write */
    stepper_apply(s_step_seq_idx);
    return false;   /* no task woken */
}

/* ---- Public API ---- */

/* Configure the four GPIOs and create (but don't start) the step timer. */
static inline void stepper_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << STEP_IN1_GPIO) | (1ULL << STEP_IN2_GPIO) |
                        (1ULL << STEP_IN3_GPIO) | (1ULL << STEP_IN4_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    stepper_apply(0);

    gptimer_config_t tcfg = {
        .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
        .direction     = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,   /* 1 tick = 1 us */
        .intr_priority = 0,
        .flags         = {},
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&tcfg, &s_step_timer));

    gptimer_event_callbacks_t cbs = { .on_alarm = stepper_on_alarm };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_step_timer, &cbs, nullptr));

    gptimer_alarm_config_t alarm = {
        .alarm_count  = STEP_INTERVAL_US,
        .reload_count = 0,
        .flags        = { .auto_reload_on_alarm = true },
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(s_step_timer, &alarm));
    ESP_ERROR_CHECK(gptimer_enable(s_step_timer));

    ESP_LOGI("stepper", "init: sweep %d steps, %d us/step", STEP_SWEEP_STEPS, STEP_INTERVAL_US);
}

/* Start the automatic bounce sweep (runs in the background via the timer). */
static inline void stepper_start_sweep(void)
{
    ESP_ERROR_CHECK(gptimer_start(s_step_timer));
}

/* Stop sweeping. Coils stay energized (holds position). */
static inline void stepper_stop(void)
{
    ESP_ERROR_CHECK(gptimer_stop(s_step_timer));
}

/* Advance exactly one step yourself — useful for a step/sample/step scan loop.
   Don't mix with the auto sweep; use one or the other. */
static inline void stepper_step_once(bool forward)
{
    s_step_seq_idx = forward ? (s_step_seq_idx + 1) % STEP_SEQ_LEN
                             : (s_step_seq_idx + STEP_SEQ_LEN - 1) % STEP_SEQ_LEN;
    s_step_position += forward ? 1 : -1;
    stepper_apply(s_step_seq_idx);
}

/* Current position in steps — tag your detections with this. */
static inline int32_t stepper_position(void)
{
    return s_step_position;
}

/* Convenience: current position expressed in degrees of shaft rotation. */
static inline float stepper_degrees(void)
{
    return (float)s_step_position * 360.0f / STEP_STEPS_PER_REV;
}

/* Cut current to all coils (stops holding torque, saves power, runs cool). */
static inline void stepper_release(void)
{
    gpio_set_level(STEP_IN1_GPIO, 0);
    gpio_set_level(STEP_IN2_GPIO, 0);
    gpio_set_level(STEP_IN3_GPIO, 0);
    gpio_set_level(STEP_IN4_GPIO, 0);
}
