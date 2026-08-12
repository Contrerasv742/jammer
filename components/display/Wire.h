#pragma once

#include <stdio.h>
#include <cstdint>
#include <cstddef>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000
#define WIRE_BUFFER_SIZE            128

class TwoWire {
private:
    static constexpr const char* TAG = "TwoWire";

    i2c_master_bus_handle_t _bus_handle;
    i2c_master_bus_config_t _bus_config;

    i2c_master_dev_handle_t _dev_handle;
    i2c_device_config_t _dev_config;

    uint8_t _buffer[WIRE_BUFFER_SIZE]; 
    size_t _buffer_index;
    
    // Track current device address
    uint8_t _current_device_address;  

public:
    TwoWire() : _bus_handle(nullptr), _bus_config{},
                _dev_handle(nullptr), _dev_config{}, 
                _buffer{0}, _buffer_index(0) {}

    void begin();

    void beginTransmission(uint8_t device_address);

    /* @brief Writes data to the address started by begin Transmission
     * @param value transmit 1 byte
     * @return the number of bytes written (Optional)
     * */
    int write(uint8_t value);

    /* @brief Writes data to the address started by begin Transmission
     * @param data (string)
     * @return the number of bytes written (Optional)
     * */
    int write(const char *data); 

    /* @brief Writes data to the address started by begin Transmission
     * @param data 
     * @param length
     * @return the number of bytes written (Optional)
     * */
    int write(const uint8_t *data, size_t length); 

    void endTransmission();

    ~TwoWire() {
        if (_dev_handle) {
            ESP_ERROR_CHECK(i2c_master_bus_rm_device(_dev_handle));
        }
        if (_bus_handle) {
            ESP_ERROR_CHECK(i2c_del_master_bus(_bus_handle));
        }
        ESP_LOGI(TAG, "I2C de-initialized successfully");
    }
};

extern TwoWire Wire;
