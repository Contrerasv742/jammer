/**
 * @file main.c
 * @brief HUSB238 Basic Example - Minimal setup with controller API
 *
 * This example shows the simplest way to use the HUSB238 driver.
 * The controller handles all device management automatically.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "husb238.h"

static const char *TAG = "husb238_basic";

/* Callbacks - called when voltage or state changes */
static void on_voltage_change(uint16_t voltage_mv, uint16_t current_ma, void *user_data)
{
    ESP_LOGI(TAG, "Voltage changed: %d.%02dV @ %d.%02dA",
             voltage_mv / 1000, (voltage_mv % 1000) / 10,
             current_ma / 1000, (current_ma % 1000) / 10);
}

static void on_state_change(husb238_state_t state, void *user_data)
{
    static const char *state_names[] = {
        "NOT_PRESENT", "INITIALIZING", "WAITING_PD", "CONNECTED", "ERROR"
    };
    ESP_LOGI(TAG, "State: %s", state_names[state]);
}

void app_main(void)
{
    ESP_LOGI(TAG, "HUSB238 Basic Example");

    /* Configure controller - let it manage I2C internally */
    husb238_controller_config_t config = {
        .sda_gpio = CONFIG_HUSB238_I2C_SDA_GPIO,
        .scl_gpio = CONFIG_HUSB238_I2C_SCL_GPIO,
        .i2c_freq_hz = CONFIG_HUSB238_I2C_FREQ_HZ,
        .force_5v_on_connect = CONFIG_HUSB238_FORCE_5V_ON_CONNECT,
        .on_voltage_change = on_voltage_change,
        .on_state_change = on_state_change,
    };

    husb238_controller_handle_t ctrl;
    esp_err_t ret = husb238_controller_init(&config, &ctrl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Controller started");
    ESP_LOGI(TAG, "Connect a USB-C PD power supply to see available voltages");

    /* Main loop - demonstrate programmatic voltage control */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        /* Only interact when connected */
        if (husb238_controller_get_state(ctrl) == HUSB238_STATE_CONNECTED) {
            int count = husb238_controller_get_voltage_count(ctrl);
            int current = husb238_controller_get_current_index(ctrl);

            ESP_LOGI(TAG, "Available: %d voltages, current index: %d", count, current);

            /* Print available voltages */
            for (int i = 0; i < count; i++) {
                husb238_voltage_info_t info;
                if (husb238_controller_get_voltage_info(ctrl, i, &info) == ESP_OK) {
                    ESP_LOGI(TAG, "  [%d] %dV @ %dmA %s",
                             i, info.voltage_mv / 1000, info.max_current_ma,
                             (i == current) ? "<-- current" : "");
                }
            }
        }
    }
}
