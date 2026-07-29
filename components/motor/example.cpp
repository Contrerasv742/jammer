/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_servo.h"
#include "sdkconfig.h"
#include <math.h>
#include <stdio.h>

const int SERVO_GPIO{2};

static const char *TAG = "Servo Control";

static uint16_t calibration_value_0 = 30;    // Real 0 degree angle
static uint16_t calibration_value_180 = 195; // Real 180 degree angle
static servo_handle_t s_servo = NULL;

// Task to test the servo
static void servo_test_task(void *arg) {
  ESP_LOGI(TAG, "Servo Test Task");
  while (1) {
    // Set the angle of the servo
    for (int i = calibration_value_0; i <= calibration_value_180; i += 1) {
      iot_servo_write_angle(s_servo, i);
      vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    // Return to the initial position
    iot_servo_write_angle(s_servo, calibration_value_0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

static void servo_init(void) {
  ESP_LOGI(TAG, "Servo Control");

  // Configure the servo
  servo_config_t servo_cfg =
      SERVO_CONFIG_DEFAULT(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, LEDC_CHANNEL_0,
                           (gpio_num_t)SERVO_GPIO);
  servo_cfg.max_angle = calibration_value_180;

  // Initialize the servo
  ESP_ERROR_CHECK(iot_servo_new(&servo_cfg, &s_servo));
}
