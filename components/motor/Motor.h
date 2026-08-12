/*!
 * @file Motor.h
 * @brief MG996R Motor Driving Files
 * @licence     The MIT License (MIT)
 * @maintainer [yangfeng](feng.yang@dfrobot.com)
 * @version  V1.0
 * @date  2021-09-24
 * @url https://github.com/DFRobot/DFRobot_RGBLCD1602
 */

#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_servo.h"
#include "sdkconfig.h"
#include <math.h>
#include <stdio.h>

class Servo {
public:
    servo_handle_t handle_{nullptr};
    gpio_num_t gpio_num_{};
    
    /*
     * @param max_angle    Servo max angle (i.e. 180)
     * @param min_width_us Pulse width corresponding to minimum
     *                     angle, which is usually 500us
     * @param max_width_us Pulse width corresponding to maximum
     *                     angle, which is usually 2500us
     * @param freq         PWM frequency
     * @param speed_mode   LEDC channel group with
     *                     specified speed mode
     * @param timer_number Timer number of ledc
     * @param channel      LEDC channel to use
     * @param gpio_num     GPIO number of PWM output
     */
    Servo(uint16_t max_angle, uint16_t min_width_us, uint16_t max_width_us,
          uint32_t freq, ledc_mode_t speed_mode, ledc_timer_t timer_number,
          ledc_channel_t channel, gpio_num_t gpio_num);
    

    void test_task(void);
    void init(void);
};
