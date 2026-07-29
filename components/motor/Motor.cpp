#include "Motor.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_servo.h"
#include "sdkconfig.h"
#include <math.h>
#include <stdio.h>

#define TAG "Motor"

Servo::Servo(uint16_t max_angle, uint16_t min_width_us, 
             uint16_t max_width_us, uint32_t freq, ledc_mode_t speed_mode, 
             ledc_timer_t timer_number, ledc_channel_t channel, 
             gpio_num_t gpio_num) : gpio_num_(gpio_num)
{
  servo_config_t config = {
      .max_angle = max_angle,
      .min_width_us = min_width_us,
      .max_width_us = max_width_us,
      .freq = freq,
      .speed_mode = speed_mode,
      .timer_number = timer_number,
      .channel = channel,
      .gpio_num = gpio_num,
  };

  iot_servo_new(&config, &handle_);
}

/*
 * TODO: Gather Calibration values for the servos
 * */
static uint16_t calibration_value_0 = 30;    // Real 0 degree angle
static uint16_t calibration_value_180 = 195; // Real 180 degree angle

void Servo::test_task(void) {
  ESP_LOGI(TAG, "Servo Test Task");
  while (1) {
    // Set the angle of the servo
    for (int i = calibration_value_0; i <= calibration_value_180; i += 1) {
      iot_servo_write_angle(handle_, i);
      vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    // Return to the initial position
    iot_servo_write_angle(handle_, calibration_value_0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void Servo::init(void) {
    ESP_LOGI(TAG, "Servo Control");

    // Configure the servo
    servo_config_t servo_cfg = 
        SERVO_CONFIG_DEFAULT(
              LEDC_LOW_SPEED_MODE,
              LEDC_TIMER_0, 
              LEDC_CHANNEL_0, 
              gpio_num_
    );
    servo_cfg.max_angle = calibration_value_180;

    // Initialize the servo
    ESP_ERROR_CHECK(iot_servo_new(&servo_cfg, &handle_));
}
