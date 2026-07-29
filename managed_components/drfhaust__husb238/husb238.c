/**
 * @file husb238.c
 * @brief HUSB238 USB PD Sink Controller Driver Implementation
 *
 * Pure C port of Adafruit_HUSB238 library for ESP-IDF
 * Uses ESP-IDF i2c_master driver (v5.x API)
 */

#include "husb238.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "HUSB238";

// I2C timeout in ms
#define HUSB238_I2C_TIMEOUT_MS  100

// =====================================================
// Internal Helper Functions
// =====================================================

/**
 * @brief Read a single register
 */
static esp_err_t husb238_read_reg(husb238_handle_t *handle, uint8_t reg, uint8_t *value)
{
    if (!handle || !handle->i2c_dev || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit_receive(handle->i2c_dev, &reg, 1, value, 1, HUSB238_I2C_TIMEOUT_MS);
}

/**
 * @brief Write a single register
 */
static esp_err_t husb238_write_reg(husb238_handle_t *handle, uint8_t reg, uint8_t value)
{
    if (!handle || !handle->i2c_dev) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[2] = { reg, value };
    return i2c_master_transmit(handle->i2c_dev, buf, 2, HUSB238_I2C_TIMEOUT_MS);
}

/**
 * @brief Read-modify-write a register with bit mask
 */
static esp_err_t husb238_update_reg_bits(husb238_handle_t *handle, uint8_t reg,
                                          uint8_t mask, uint8_t shift, uint8_t value)
{
    uint8_t current;
    esp_err_t ret = husb238_read_reg(handle, reg, &current);
    if (ret != ESP_OK) {
        return ret;
    }

    current &= ~(mask << shift);            // Clear bits
    current |= ((value & mask) << shift);   // Set new value

    return husb238_write_reg(handle, reg, current);
}

/**
 * @brief Get the register address for a PD selection voltage
 */
static uint8_t husb238_pd_sel_to_reg(husb238_pd_selection_t pd_sel)
{
    switch (pd_sel) {
        case HUSB238_PD_SRC_5V:  return HUSB238_SRC_PDO_5V;
        case HUSB238_PD_SRC_9V:  return HUSB238_SRC_PDO_9V;
        case HUSB238_PD_SRC_12V: return HUSB238_SRC_PDO_12V;
        case HUSB238_PD_SRC_15V: return HUSB238_SRC_PDO_15V;
        case HUSB238_PD_SRC_18V: return HUSB238_SRC_PDO_18V;
        case HUSB238_PD_SRC_20V: return HUSB238_SRC_PDO_20V;
        default: return 0;
    }
}

// =====================================================
// Initialization / De-initialization
// =====================================================

esp_err_t husb238_init(const husb238_config_t *config, husb238_handle_t *handle_out)
{
    if (!config || !config->i2c_bus || !handle_out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(handle_out, 0, sizeof(husb238_handle_t));

    uint8_t addr = config->i2c_addr;
    if (addr == 0) {
        addr = HUSB238_I2CADDR_DEFAULT;
    }
    handle_out->addr = addr;

    uint32_t speed = config->scl_speed_hz;
    if (speed == 0) {
        speed = 100000;  // Default 100kHz
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = speed,
    };

    esp_err_t ret = i2c_master_bus_add_device(config->i2c_bus, &dev_config, &handle_out->i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add HUSB238 device to I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Verify device is present by reading status register
    uint8_t status;
    ret = husb238_read_reg(handle_out, HUSB238_PD_STATUS0, &status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HUSB238 not responding at address 0x%02X", addr);
        i2c_master_bus_rm_device(handle_out->i2c_dev);
        handle_out->i2c_dev = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "HUSB238 initialized at address 0x%02X", addr);
    return ESP_OK;
}

esp_err_t husb238_deinit(husb238_handle_t *handle)
{
    if (!handle || !handle->i2c_dev) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_bus_rm_device(handle->i2c_dev);
    handle->i2c_dev = NULL;
    return ret;
}

// =====================================================
// Status Functions
// =====================================================

esp_err_t husb238_get_cc_direction(husb238_handle_t *handle, bool *cc_direction)
{
    if (!cc_direction) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t status;
    esp_err_t ret = husb238_read_reg(handle, HUSB238_PD_STATUS1, &status);
    if (ret != ESP_OK) {
        return ret;
    }

    // Bit 7: CC direction (0 = CC1, 1 = CC2)
    *cc_direction = (status >> 7) & 0x01;
    return ESP_OK;
}

esp_err_t husb238_is_attached(husb238_handle_t *handle, bool *attached)
{
    if (!attached) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t status;
    esp_err_t ret = husb238_read_reg(handle, HUSB238_PD_STATUS1, &status);
    if (ret != ESP_OK) {
        return ret;
    }

    // Bit 6: Attached status
    *attached = (status >> 6) & 0x01;
    return ESP_OK;
}

esp_err_t husb238_get_pd_response(husb238_handle_t *handle, husb238_response_t *response)
{
    if (!response) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t status;
    esp_err_t ret = husb238_read_reg(handle, HUSB238_PD_STATUS1, &status);
    if (ret != ESP_OK) {
        return ret;
    }

    // Bits 3-5: Response code
    *response = (husb238_response_t)((status >> 3) & 0x07);
    return ESP_OK;
}

esp_err_t husb238_get_5v_contract_voltage(husb238_handle_t *handle, bool *contract)
{
    if (!contract) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t status;
    esp_err_t ret = husb238_read_reg(handle, HUSB238_PD_STATUS1, &status);
    if (ret != ESP_OK) {
        return ret;
    }

    // Bit 2: 5V contract indicator
    *contract = (status >> 2) & 0x01;
    return ESP_OK;
}

esp_err_t husb238_get_5v_contract_current(husb238_handle_t *handle, husb238_5v_current_t *current)
{
    if (!current) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t status;
    esp_err_t ret = husb238_read_reg(handle, HUSB238_PD_STATUS1, &status);
    if (ret != ESP_OK) {
        return ret;
    }

    // Bits 0-1: 5V current contract
    *current = (husb238_5v_current_t)(status & 0x03);
    return ESP_OK;
}

// =====================================================
// Current PD Contract
// =====================================================

esp_err_t husb238_get_pd_src_voltage(husb238_handle_t *handle, husb238_voltage_t *voltage)
{
    if (!voltage) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t status;
    esp_err_t ret = husb238_read_reg(handle, HUSB238_PD_STATUS0, &status);
    if (ret != ESP_OK) {
        return ret;
    }

    // Bits 4-7: Voltage setting
    *voltage = (husb238_voltage_t)((status >> 4) & 0x0F);
    return ESP_OK;
}

esp_err_t husb238_get_pd_src_current(husb238_handle_t *handle, husb238_current_t *current)
{
    if (!current) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t status;
    esp_err_t ret = husb238_read_reg(handle, HUSB238_PD_STATUS0, &status);
    if (ret != ESP_OK) {
        return ret;
    }

    // Bits 0-3: Current setting
    *current = (husb238_current_t)(status & 0x0F);
    return ESP_OK;
}

// =====================================================
// Source Capability Detection
// =====================================================

esp_err_t husb238_is_voltage_detected(husb238_handle_t *handle, husb238_pd_selection_t pd_sel, bool *detected)
{
    if (!detected) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t reg = husb238_pd_sel_to_reg(pd_sel);
    if (reg == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t value;
    esp_err_t ret = husb238_read_reg(handle, reg, &value);
    if (ret != ESP_OK) {
        return ret;
    }

    // Bit 7: Voltage detected flag
    *detected = (value >> 7) & 0x01;
    return ESP_OK;
}

esp_err_t husb238_get_current_for_voltage(husb238_handle_t *handle, husb238_pd_selection_t pd_sel, husb238_current_t *current)
{
    if (!current) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t reg = husb238_pd_sel_to_reg(pd_sel);
    if (reg == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t value;
    esp_err_t ret = husb238_read_reg(handle, reg, &value);
    if (ret != ESP_OK) {
        return ret;
    }

    // Bits 0-3: Current capability for this voltage
    *current = (husb238_current_t)(value & 0x0F);
    return ESP_OK;
}

// =====================================================
// PD Selection and Control
// =====================================================

esp_err_t husb238_get_selected_pd(husb238_handle_t *handle, husb238_pd_selection_t *selection)
{
    if (!selection) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t value;
    esp_err_t ret = husb238_read_reg(handle, HUSB238_SRC_PDO, &value);
    if (ret != ESP_OK) {
        return ret;
    }

    // Bits 4-7: Selected PD
    *selection = (husb238_pd_selection_t)((value >> 4) & 0x0F);
    return ESP_OK;
}

esp_err_t husb238_select_pd(husb238_handle_t *handle, husb238_pd_selection_t pd_sel)
{
    // Write to bits 4-7 of SRC_PDO register
    return husb238_update_reg_bits(handle, HUSB238_SRC_PDO, 0x0F, 4, (uint8_t)pd_sel);
}

esp_err_t husb238_request_pd(husb238_handle_t *handle)
{
    // Write 0b00001 to bits 0-4 of GO_COMMAND to request PD
    return husb238_update_reg_bits(handle, HUSB238_GO_COMMAND, 0x1F, 0, 0x01);
}

esp_err_t husb238_get_source_capabilities(husb238_handle_t *handle)
{
    // Write 0b00100 to bits 0-4 of GO_COMMAND to get source capabilities
    return husb238_update_reg_bits(handle, HUSB238_GO_COMMAND, 0x1F, 0, 0x04);
}

esp_err_t husb238_reset(husb238_handle_t *handle)
{
    // Write 0b10000 to bits 0-4 of GO_COMMAND to reset
    return husb238_update_reg_bits(handle, HUSB238_GO_COMMAND, 0x1F, 0, 0x10);
}

// =====================================================
// Utility Functions
// =====================================================

uint16_t husb238_current_to_ma(husb238_current_t current)
{
    static const uint16_t current_ma[] = {
        500,   // CURRENT_0_5_A
        700,   // CURRENT_0_7_A
        1000,  // CURRENT_1_0_A
        1250,  // CURRENT_1_25_A
        1500,  // CURRENT_1_5_A
        1750,  // CURRENT_1_75_A
        2000,  // CURRENT_2_0_A
        2250,  // CURRENT_2_25_A
        2500,  // CURRENT_2_5_A
        2750,  // CURRENT_2_75_A
        3000,  // CURRENT_3_0_A
        3250,  // CURRENT_3_25_A
        3500,  // CURRENT_3_5_A
        4000,  // CURRENT_4_0_A
        4500,  // CURRENT_4_5_A
        5000   // CURRENT_5_0_A
    };

    if (current > HUSB238_CURRENT_5_0_A) {
        return 0;
    }
    return current_ma[current];
}

uint16_t husb238_voltage_to_mv(husb238_voltage_t voltage)
{
    static const uint16_t voltage_mv[] = {
        0,      // UNATTACHED
        5000,   // 5V
        9000,   // 9V
        12000,  // 12V
        15000,  // 15V
        18000,  // 18V
        20000   // 20V
    };

    if (voltage > HUSB238_VOLTAGE_20V) {
        return 0;
    }
    return voltage_mv[voltage];
}

uint16_t husb238_5v_current_to_ma(husb238_5v_current_t current)
{
    switch (current) {
        case HUSB238_5V_CURRENT_DEFAULT: return 500;   // USB default
        case HUSB238_5V_CURRENT_1_5_A:   return 1500;
        case HUSB238_5V_CURRENT_2_4_A:   return 2400;
        case HUSB238_5V_CURRENT_3_A:     return 3000;
        default: return 0;
    }
}
