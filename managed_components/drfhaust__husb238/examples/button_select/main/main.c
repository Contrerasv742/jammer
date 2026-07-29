/**
 * @file main.c
 * @brief HUSB238 Button Voltage Select Example
 *
 * Demonstrates button-based voltage cycling with user-managed I2C bus.
 * Press the button to cycle through available PD voltages.
 *
 * Button handling is done in the application - the HUSB238 component
 * provides the API to change voltages.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "husb238.h"

static const char *TAG = "husb238_button";

/* Pin configuration */
#define I2C_SDA_GPIO    CONFIG_HUSB238_I2C_SDA_GPIO
#define I2C_SCL_GPIO    CONFIG_HUSB238_I2C_SCL_GPIO
#define BUTTON_GPIO     0   /* Change to your button GPIO */
#define DEBOUNCE_MS     200

/* Global controller handle for ISR access */
static husb238_controller_handle_t g_ctrl = NULL;
static volatile bool g_button_pressed = false;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    g_button_pressed = true;
}

static void on_voltage_change(uint16_t voltage_mv, uint16_t current_ma, void *user_data)
{
    ESP_LOGI(TAG, ">>> Output: %d.%dV @ %d.%02dA <<<",
             voltage_mv / 1000, (voltage_mv % 1000) / 100,
             current_ma / 1000, (current_ma % 1000) / 10);
}

static void on_state_change(husb238_state_t state, void *user_data)
{
    switch (state) {
        case HUSB238_STATE_NOT_PRESENT:
            ESP_LOGW(TAG, "HUSB238 not detected - check wiring");
            break;
        case HUSB238_STATE_WAITING_PD:
            ESP_LOGI(TAG, "Waiting for USB-C PD source...");
            break;
        case HUSB238_STATE_CONNECTED:
            ESP_LOGI(TAG, "PD source connected! Press button to change voltage.");
            break;
        case HUSB238_STATE_ERROR:
            ESP_LOGE(TAG, "Communication error - reconnecting...");
            break;
        default:
            break;
    }
}

static void init_button(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
    ESP_LOGI(TAG, "Button initialized on GPIO %d", BUTTON_GPIO);
}

void app_main(void)
{
    ESP_LOGI(TAG, "HUSB238 Button Voltage Select Example");
    ESP_LOGI(TAG, "I2C: SDA=%d, SCL=%d | Button: GPIO %d",
             I2C_SDA_GPIO, I2C_SCL_GPIO, BUTTON_GPIO);

    /* Create I2C bus - user manages it (allows sharing with other devices) */
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_GPIO,
        .sda_io_num = I2C_SDA_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return;
    }

    /* Configure controller with user-provided I2C bus */
    husb238_controller_config_t config = {
        .i2c_bus = i2c_bus,
        .force_5v_on_connect = true,
        .on_voltage_change = on_voltage_change,
        .on_state_change = on_state_change,
    };

    ret = husb238_controller_init(&config, &g_ctrl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init controller: %s", esp_err_to_name(ret));
        return;
    }

    /* Initialize button (application responsibility) */
    init_button();

    ESP_LOGI(TAG, "Controller started - press button to cycle voltages");

    static uint32_t last_button_time = 0;

    while (1) {
        /* Handle button press with debouncing */
        if (g_button_pressed) {
            g_button_pressed = false;
            uint32_t now = xTaskGetTickCount();

            if ((now - last_button_time) > pdMS_TO_TICKS(DEBOUNCE_MS)) {
                last_button_time = now;

                if (husb238_controller_get_state(g_ctrl) == HUSB238_STATE_CONNECTED) {
                    ESP_LOGI(TAG, "Button pressed - cycling voltage");
                    husb238_controller_next_voltage(g_ctrl);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
