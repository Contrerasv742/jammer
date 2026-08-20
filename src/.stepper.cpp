/*
 * L298N stepper example for ESP-IDF
 * ---------------------------------
 * Adapted from the DRV8825 RMT "smooth controller" example.
 *
 * Why this looks different from the DRV8825 version:
 *   The DRV8825 is a STEP/DIR driver -- you send one pulse per step and the
 *   chip does the coil commutation itself, so that example uses RMT to
 *   generate STEP pulses.  The L298N is just two raw H-bridges; it has no
 *   STEP input and no sequencer, so WE must drive the four inputs (IN1..IN4)
 *   through the coil pattern ourselves.  RMT and the two encoders are gone;
 *   a GPTimer advances the step sequence and varies the interval to get the
 *   same Acceleration / Uniform / Deceleration profile.
 *
 * Wiring (change GPIOs to match your board):
 *   ESP IN1_GPIO -> L298N IN1  \  Coil A on OUT1/OUT2
 *   ESP IN2_GPIO -> L298N IN2  /
 *   ESP IN3_GPIO -> L298N IN3  \  Coil B on OUT3/OUT4
 *   ESP IN4_GPIO -> L298N IN4  /
 *   L298N ENA, ENB : leave jumpered to 5V (always enabled)
 *   L298N 12V/GND  : motor supply.  GND must be COMMON with the ESP's GND.
 */

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"

static const char *TAG = "l298n_stepper";

/* ---- The four L298N inputs (edit to match your wiring) ---- */
#define IN1_GPIO  GPIO_NUM_16   /* Coil A */
#define IN2_GPIO  GPIO_NUM_17
#define IN3_GPIO  GPIO_NUM_18   /* Coil B */
#define IN4_GPIO  GPIO_NUM_19

/*
 * Full-step, two-phase-on sequence (both coils always energized -> max torque).
 * Each row is {IN1, IN2, IN3, IN4}.
 *
 * For half-stepping (smoother, half the step angle) replace this 4-row table
 * with the 8-row table at the bottom of this file and set SEQ_LEN to 8.
 */
static const uint8_t step_seq[4][4] = {
    {1, 0, 1, 0},
    {0, 1, 1, 0},
    {0, 1, 0, 1},
    {1, 0, 0, 1},
};
#define SEQ_LEN 4

/* ---- Motion profile (mirrors the DRV8825 example: 500 + 5000 + 500) ---- */
#define ACCEL_STEPS     500
#define UNIFORM_STEPS   5000
#define DECEL_STEPS     500
#define TOTAL_STEPS     (ACCEL_STEPS + UNIFORM_STEPS + DECEL_STEPS)
#define START_DELAY_US  5000    /* slowest step interval (start of accel)  */
#define MIN_DELAY_US    1200    /* fastest step interval (uniform speed)   */

typedef struct {
    int  seq_idx;      /* current index into step_seq[]          */
    int  step_count;   /* steps done in the current profile run  */
    bool dir_forward;  /* direction; flips at the end of a run   */
} motor_t;

static motor_t motor = { .dir_forward = true };

static inline void apply_step(int idx)
{
    gpio_set_level(IN1_GPIO, step_seq[idx][0]);
    gpio_set_level(IN2_GPIO, step_seq[idx][1]);
    gpio_set_level(IN3_GPIO, step_seq[idx][2]);
    gpio_set_level(IN4_GPIO, step_seq[idx][3]);
}

/* Interval to wait before executing step number n of the profile.
   Linear ramp down during accel, flat during uniform, ramp up during decel. */
static uint32_t delay_for_step(int n)
{
    if (n < ACCEL_STEPS) {
        return START_DELAY_US
             - (uint32_t)(START_DELAY_US - MIN_DELAY_US) * n / ACCEL_STEPS;
    } else if (n < ACCEL_STEPS + UNIFORM_STEPS) {
        return MIN_DELAY_US;
    } else {
        int d = n - (ACCEL_STEPS + UNIFORM_STEPS);
        return MIN_DELAY_US
             + (uint32_t)(START_DELAY_US - MIN_DELAY_US) * d / DECEL_STEPS;
    }
}

/* Fires once per step. Advances the sequence, then re-arms for the next
   interval. Runs continuously, flipping direction after each full profile. */
static bool IRAM_ATTR on_alarm(gptimer_handle_t timer,
                               const gptimer_alarm_event_data_t *edata,
                               void *ctx)
{
    motor_t *m = (motor_t *)ctx;

    m->seq_idx = m->dir_forward ? (m->seq_idx + 1) % SEQ_LEN
                                : (m->seq_idx + SEQ_LEN - 1) % SEQ_LEN;
    apply_step(m->seq_idx);

    if (++m->step_count >= TOTAL_STEPS) {   /* run complete -> reverse & repeat */
        m->step_count = 0;
        m->dir_forward = !m->dir_forward;
    }

    gptimer_alarm_config_t next = {
        .alarm_count  = delay_for_step(m->step_count),
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true, /* counter resets to 0 each alarm */
    };
    gptimer_set_alarm_action(timer, &next);
    return false;                            /* no task woken */
}

void app_main(void)
{
    /* Configure the four L298N inputs as outputs */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << IN1_GPIO) | (1ULL << IN2_GPIO) |
                        (1ULL << IN3_GPIO) | (1ULL << IN4_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    apply_step(0);

    ESP_LOGI(TAG, "L298N stepper: %d steps per run (%d accel + %d uniform + %d decel)",
             TOTAL_STEPS, ACCEL_STEPS, UNIFORM_STEPS, DECEL_STEPS);

    /* Create a 1 MHz timer (1 tick = 1 us) to pace the steps */
    gptimer_handle_t timer = NULL;
    gptimer_config_t tcfg = {
        .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
        .direction     = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&tcfg, &timer));

    gptimer_event_callbacks_t cbs = { .on_alarm = on_alarm };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, &cbs, &motor));

    gptimer_alarm_config_t first = {
        .alarm_count  = START_DELAY_US,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer, &first));
    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));

    /* The timer drives the motor in the background; nothing to do here. */
}

/*
 * Optional half-step table (replace step_seq[][] above and set SEQ_LEN 8):
 *
 * static const uint8_t step_seq[8][4] = {
 *     {1, 0, 1, 0},
 *     {1, 0, 0, 0},
 *     {1, 0, 0, 1},
 *     {0, 0, 0, 1},
 *     {0, 1, 0, 1},
 *     {0, 1, 0, 0},
 *     {0, 1, 1, 0},
 *     {0, 0, 1, 0},
 * };
 */
