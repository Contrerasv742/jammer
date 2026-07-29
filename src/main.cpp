// Components
#include "Display.h"
#include "hsm.hpp"
#include "config.h"
#include "motor.hpp"
#include "husb238.h"

// ESP-IDF
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG { "Debug" };

const int colorR { 80 };
const int colorG { 10 };
const int colorB { 200 };

/* @brief Convert microseconds to milliseconds */
uint32_t millis() {
    return esp_timer_get_time() / 1000;  
}

Display display(RGB_ADDR, LCD_COLS, LCD_ROWS);

/* @brief initialize the LCD and master IIC */
void setup() {
    display.init();
    display.setRGB(colorR, colorG, colorB);

    ESP_LOGD(TAG, "hello, world!");

    vTaskDelay(pdMS_TO_TICKS(100));
}

/*
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Initializing LCD...");
    display.init();
    display.setRGB(colorR, colorG, colorB);
    ESP_LOGI(TAG, "LCD initialized!");
 
    // Hand the display to the state machine and run it forever.
    hsm_init(display);
    while (true) {
        hsm_run();
    }
}
*/

#include "sdkconfig.h"
#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */

// extern "C" void app_main(void) {
//     husb238_controller_config_t config = {
//         .sda_gpio = I2C_MASTER_SDA_IO,
//         .scl_gpio = I2C_MASTER_SCL_IO,
//         .i2c_freq_hz = I2C_MASTER_FREQ_HZ,
//         .i2c_bus = (i2c_master_bus_t *) 10,
//         .force_5v_on_connect = true,
//         .i2c_addr = 
//     };
//
//     husb238_controller_handle_t ctrl;
//     husb238_controller_init(&config, &ctrl);
//
//     // Change voltage programmatically
//     husb238_controller_next_voltage(ctrl);
//     // Or select specific voltage index
//     husb238_controller_select_voltage(ctrl, 2);
// }

// #include "husb238.h"

// static const char *TAG = "husb238_basic";

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
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "HUSB238 Basic Example");

    i2c_master_bus_handle_t bus_handle { };

    i2c_master_bus_config_t bus_config {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = (gpio_num_t) I2C_MASTER_SDA_IO,
        .scl_io_num = (gpio_num_t) I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = true,
            .allow_pd = false,
        },
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    /* Configure controller - let it manage I2C internally */
    husb238_controller_config_t config = {
        .sda_gpio = CONFIG_HUSB238_I2C_SDA_GPIO,
        .scl_gpio = CONFIG_HUSB238_I2C_SCL_GPIO,
        .i2c_freq_hz = CONFIG_HUSB238_I2C_FREQ_HZ,
        .i2c_bus = bus_handle,
        .i2c_addr = HUSB238_I2CADDR_DEFAULT,
        .force_5v_on_connect = CONFIG_HUSB238_FORCE_5V_ON_CONNECT,
        .on_voltage_change = on_voltage_change,
        .on_state_change = on_state_change,
        .user_data = 0,
    };

    husb238_controller_handle_t ctrl;
    esp_err_t ret = husb238_controller_init(&config, &ctrl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Controller started");

    servo_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));

        if (husb238_controller_get_state(ctrl) == HUSB238_STATE_CONNECTED) {
            esp_err_t error = husb238_controller_request_voltage(ctrl, 5000);
            ESP_LOGI(TAG, "Requested 5V: %s", esp_err_to_name(error));

            // Only command the servo once the rail is actually at 5V
            if (husb238_controller_get_voltage_mv(ctrl) == 5000) {
                servo_write_us(SERVO_MIN_US);
                vTaskDelay(pdMS_TO_TICKS(800));
                servo_write_us(SERVO_MID_US);
                vTaskDelay(pdMS_TO_TICKS(800));
                servo_write_us(SERVO_MAX_US);
                vTaskDelay(pdMS_TO_TICKS(800));
            }
        }
    }

}
