#include <stdio.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#include "Wire.h"

TwoWire Wire;

/* @brief Begins the i2c initialization */
void TwoWire::begin() {
    // Configure the bus
    i2c_master_bus_config_t bus_config = {
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

    _bus_config = bus_config; 

    ESP_ERROR_CHECK(i2c_new_master_bus(&_bus_config, &_bus_handle));

    ESP_LOGI(TAG, "I2C Bus initialized successfully");
}

void TwoWire::beginTransmission(uint8_t device_address) {
    // Reset buffer for new transmission
    _buffer_index = 0;

    if (_current_device_address != device_address) {
        if (_dev_handle != nullptr) {
            i2c_master_bus_rm_device(_dev_handle);
        }
        
        // Create a new device
        _dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = device_address,
            .scl_speed_hz = I2C_MASTER_FREQ_HZ,
            .scl_wait_us = 0,
            .flags = {},
        };

        ESP_ERROR_CHECK(i2c_master_bus_add_device(_bus_handle, &_dev_config, &_dev_handle));
        _current_device_address = device_address;
        ESP_LOGI(TAG, "I2C Device initialized successfully");
    }
}

int TwoWire::write(uint8_t value) {
    if (_buffer_index >= WIRE_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Buffer overflow!");
        return 0;
    }
    _buffer[_buffer_index++] = value;
    return 1;
}

int TwoWire::write(const char* data) {
    size_t len = strlen(data);
    return write((const uint8_t*)data, len);
}

int TwoWire::write(const uint8_t* data, size_t length) {
    if (_buffer_index + length > WIRE_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Buffer overflow! Attempting to write %d bytes, only %d available", 
                 length, WIRE_BUFFER_SIZE - _buffer_index);
        return 0;
    }
    
    memcpy(&_buffer[_buffer_index], data, length);
    _buffer_index += length;
    return length;
}

void TwoWire::endTransmission() {
    if (_buffer_index == 0) {
        ESP_LOGW(TAG, "Nothing to transmit");
        return;
    }
    
    // Transmit all buffered bytes
    esp_err_t ret = i2c_master_transmit(_dev_handle, _buffer, _buffer_index, I2C_MASTER_TIMEOUT_MS);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C transmission failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Transmitted %d bytes successfully", _buffer_index);
    }
    
    // Reset buffer for next transmission
    _buffer_index = 0;
}
