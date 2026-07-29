/**
 * @file main.cpp
 * @brief HUSB238 Arduino-as-Component Example
 *
 * Demonstrates using HUSB238 with Arduino-ESP32 as a component.
 * Uses Arduino Wire library for I2C communication.
 *
 * Requirements:
 * - arduino-esp32 as ESP-IDF component
 * - See: https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html
 */

#include "Arduino.h"
#include "Wire.h"

extern "C" {
#include "husb238.h"
}

/* Pin configuration */
#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22

static husb238_controller_handle_t ctrl = NULL;

/* Callbacks */
extern "C" void on_voltage_change(uint16_t voltage_mv, uint16_t current_ma, void *user_data)
{
    Serial.printf("Voltage: %d.%02dV @ %d.%02dA\n",
                  voltage_mv / 1000, (voltage_mv % 1000) / 10,
                  current_ma / 1000, (current_ma % 1000) / 10);
}

extern "C" void on_state_change(husb238_state_t state, void *user_data)
{
    const char *states[] = {"NOT_PRESENT", "INITIALIZING", "WAITING_PD", "CONNECTED", "ERROR"};
    Serial.printf("State: %s\n", states[state]);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("\nHUSB238 Arduino Example");

    /* Initialize I2C with Arduino Wire */
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000);

    Serial.printf("I2C initialized on SDA=%d, SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);

    /*
     * Note: When using Arduino Wire, you need to create an ESP-IDF I2C bus
     * that the HUSB238 controller can use. Arduino Wire and ESP-IDF I2C
     * use different APIs, so we create a separate bus for HUSB238.
     *
     * For shared bus scenarios, use the low-level HUSB238 API directly
     * with Wire.beginTransmission/endTransmission for communication.
     */

    /* Configure controller - let it create its own I2C bus */
    husb238_controller_config_t config = {
        .sda_gpio = I2C_SDA_PIN,
        .scl_gpio = I2C_SCL_PIN,
        .i2c_freq_hz = 100000,
        .i2c_bus = NULL,                /* Create internally */
        .force_5v_on_connect = true,
        .on_voltage_change = on_voltage_change,
        .on_state_change = on_state_change,
        .user_data = NULL,
    };

    esp_err_t ret = husb238_controller_init(&config, &ctrl);
    if (ret != ESP_OK) {
        Serial.printf("Failed to init: %s\n", esp_err_to_name(ret));
        return;
    }

    Serial.println("Controller started!");
    Serial.println("Use husb238_controller_next_voltage() to cycle voltages");
}

void loop()
{
    /* Periodically print status */
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 5000) {
        lastPrint = millis();

        if (ctrl && husb238_controller_get_state(ctrl) == HUSB238_STATE_CONNECTED) {
            int count = husb238_controller_get_voltage_count(ctrl);
            int current = husb238_controller_get_current_index(ctrl);
            uint16_t mv = husb238_controller_get_voltage_mv(ctrl);

            Serial.printf("Available: %d voltages | Current: %dmV (index %d)\n",
                          count, mv, current);

            /* Print all available voltages */
            Serial.println("Voltages:");
            for (int i = 0; i < count; i++) {
                husb238_voltage_info_t info;
                if (husb238_controller_get_voltage_info(ctrl, i, &info) == ESP_OK) {
                    Serial.printf("  [%d] %dV @ %dmA %s\n",
                                  i, info.voltage_mv / 1000, info.max_current_ma,
                                  (i == current) ? "*" : "");
                }
            }
        }
    }

    delay(100);
}
