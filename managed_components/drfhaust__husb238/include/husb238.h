/**
 * @file husb238.h
 * @brief HUSB238 USB PD Sink Controller Driver for ESP-IDF
 *
 * A complete driver for the Hynetek HUSB238 USB Power Delivery sink controller.
 * Provides both low-level register access and a high-level controller API with
 * automatic device detection, reconnection handling, and voltage selection.
 *
 * @section features Features
 * - Full register-level API for direct hardware control
 * - High-level controller with automatic state management
 * - Hot-plug detection and graceful reconnection
 * - Callback support for state and voltage changes
 * - Thread-safe design with mutex protection
 * - Support for user-provided I2C bus (Arduino Wire compatible)
 *
 * @section usage Basic Usage
 * @code
 * #include "husb238.h"
 *
 * // Simple: Let controller manage everything
 * husb238_controller_config_t config = {
 *     .sda_gpio = 21,
 *     .scl_gpio = 22,
 *     .force_5v_on_connect = true,
 * };
 * husb238_controller_handle_t ctrl;
 * husb238_controller_init(&config, &ctrl);
 *
 * // Change voltage programmatically
 * husb238_controller_next_voltage(ctrl);
 * // Or select specific voltage index
 * husb238_controller_select_voltage(ctrl, 2);
 * @endcode
 *
 * @author Olaifa Oluwadara Daniel
 * @version 1.0.0
 * @date 2025
 * @license MIT
 *
 * @see https://github.com/drfhaust/husb238
 * @see https://www.hynetek.com/product/husb238
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ========================================================================== */

/** @brief Default I2C address for HUSB238 */
#define HUSB238_I2CADDR_DEFAULT     0x08

/* ============================================================================
 * Register Addresses
 * ========================================================================== */

/** @brief PD Status Register 0 - Current voltage/current contract */
#define HUSB238_PD_STATUS0          0x00
/** @brief PD Status Register 1 - Attachment and response status */
#define HUSB238_PD_STATUS1          0x01
/** @brief Source PDO 5V capabilities */
#define HUSB238_SRC_PDO_5V          0x02
/** @brief Source PDO 9V capabilities */
#define HUSB238_SRC_PDO_9V          0x03
/** @brief Source PDO 12V capabilities */
#define HUSB238_SRC_PDO_12V         0x04
/** @brief Source PDO 15V capabilities */
#define HUSB238_SRC_PDO_15V         0x05
/** @brief Source PDO 18V capabilities */
#define HUSB238_SRC_PDO_18V         0x06
/** @brief Source PDO 20V capabilities */
#define HUSB238_SRC_PDO_20V         0x07
/** @brief Source PDO selection register */
#define HUSB238_SRC_PDO             0x08
/** @brief GO command register */
#define HUSB238_GO_COMMAND          0x09

/* ============================================================================
 * Enumerations
 * ========================================================================== */

/**
 * @brief Current capability values (4-bit, from PDO registers)
 */
typedef enum {
    HUSB238_CURRENT_0_5_A   = 0b0000,   /**< 0.50 A */
    HUSB238_CURRENT_0_7_A   = 0b0001,   /**< 0.70 A */
    HUSB238_CURRENT_1_0_A   = 0b0010,   /**< 1.00 A */
    HUSB238_CURRENT_1_25_A  = 0b0011,   /**< 1.25 A */
    HUSB238_CURRENT_1_5_A   = 0b0100,   /**< 1.50 A */
    HUSB238_CURRENT_1_75_A  = 0b0101,   /**< 1.75 A */
    HUSB238_CURRENT_2_0_A   = 0b0110,   /**< 2.00 A */
    HUSB238_CURRENT_2_25_A  = 0b0111,   /**< 2.25 A */
    HUSB238_CURRENT_2_5_A   = 0b1000,   /**< 2.50 A */
    HUSB238_CURRENT_2_75_A  = 0b1001,   /**< 2.75 A */
    HUSB238_CURRENT_3_0_A   = 0b1010,   /**< 3.00 A */
    HUSB238_CURRENT_3_25_A  = 0b1011,   /**< 3.25 A */
    HUSB238_CURRENT_3_5_A   = 0b1100,   /**< 3.50 A */
    HUSB238_CURRENT_4_0_A   = 0b1101,   /**< 4.00 A */
    HUSB238_CURRENT_4_5_A   = 0b1110,   /**< 4.50 A */
    HUSB238_CURRENT_5_0_A   = 0b1111    /**< 5.00 A */
} husb238_current_t;

/**
 * @brief Voltage values from PD_STATUS0 register (bits 4-7)
 */
typedef enum {
    HUSB238_VOLTAGE_UNATTACHED  = 0b0000,   /**< Not attached */
    HUSB238_VOLTAGE_5V          = 0b0001,   /**< 5V */
    HUSB238_VOLTAGE_9V          = 0b0010,   /**< 9V */
    HUSB238_VOLTAGE_12V         = 0b0011,   /**< 12V */
    HUSB238_VOLTAGE_15V         = 0b0100,   /**< 15V */
    HUSB238_VOLTAGE_18V         = 0b0101,   /**< 18V */
    HUSB238_VOLTAGE_20V         = 0b0110    /**< 20V */
} husb238_voltage_t;

/**
 * @brief PD negotiation response codes (from PD_STATUS1 bits 3-5)
 */
typedef enum {
    HUSB238_RESPONSE_NO_RESPONSE        = 0b000,    /**< No response yet */
    HUSB238_RESPONSE_SUCCESS            = 0b001,    /**< Request successful */
    HUSB238_RESPONSE_INVALID_CMD_OR_ARG = 0b011,    /**< Invalid command/argument */
    HUSB238_RESPONSE_CMD_NOT_SUPPORTED  = 0b100,    /**< Command not supported */
    HUSB238_RESPONSE_TRANSACTION_FAIL   = 0b101     /**< Transaction failed */
} husb238_response_t;

/**
 * @brief 5V current contract values (from PD_STATUS1 bits 0-1)
 */
typedef enum {
    HUSB238_5V_CURRENT_DEFAULT  = 0b00,     /**< Default USB (500mA) */
    HUSB238_5V_CURRENT_1_5_A    = 0b01,     /**< 1.5A */
    HUSB238_5V_CURRENT_2_4_A    = 0b10,     /**< 2.4A */
    HUSB238_5V_CURRENT_3_A      = 0b11      /**< 3.0A */
} husb238_5v_current_t;

/**
 * @brief PD voltage selection for selectPD/getSelectedPD
 */
typedef enum {
    HUSB238_PD_NOT_SELECTED = 0b0000,   /**< No voltage selected */
    HUSB238_PD_SRC_5V       = 0b0001,   /**< Select 5V */
    HUSB238_PD_SRC_9V       = 0b0010,   /**< Select 9V */
    HUSB238_PD_SRC_12V      = 0b0011,   /**< Select 12V */
    HUSB238_PD_SRC_15V      = 0b1000,   /**< Select 15V */
    HUSB238_PD_SRC_18V      = 0b1001,   /**< Select 18V */
    HUSB238_PD_SRC_20V      = 0b1010    /**< Select 20V */
} husb238_pd_selection_t;

/* ============================================================================
 * Low-Level API Types
 * ========================================================================== */

/**
 * @brief Device handle for low-level API
 */
typedef struct {
    i2c_master_dev_handle_t i2c_dev;    /**< I2C device handle */
    uint8_t addr;                        /**< Device I2C address */
} husb238_handle_t;

/**
 * @brief Configuration for low-level device initialization
 */
typedef struct {
    i2c_master_bus_handle_t i2c_bus;    /**< I2C bus handle (required) */
    uint8_t i2c_addr;                    /**< Device address (0 = default 0x08) */
    uint32_t scl_speed_hz;               /**< SCL speed in Hz (0 = default 100kHz) */
} husb238_config_t;

/* ============================================================================
 * Low-Level API Functions
 * ========================================================================== */

/**
 * @brief Initialize HUSB238 device (low-level)
 *
 * @param[in] config Configuration structure
 * @param[out] handle_out Device handle
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid parameters
 *     - ESP_ERR_NOT_FOUND: Device not responding
 */
esp_err_t husb238_init(const husb238_config_t *config, husb238_handle_t *handle_out);

/**
 * @brief Deinitialize HUSB238 device
 *
 * @param[in] handle Device handle
 * @return ESP_OK on success
 */
esp_err_t husb238_deinit(husb238_handle_t *handle);

/**
 * @brief Get CC pin direction
 *
 * @param[in] handle Device handle
 * @param[out] cc_direction true = CC2, false = CC1
 * @return ESP_OK on success
 */
esp_err_t husb238_get_cc_direction(husb238_handle_t *handle, bool *cc_direction);

/**
 * @brief Check if USB-C cable is attached
 *
 * @param[in] handle Device handle
 * @param[out] attached true if attached
 * @return ESP_OK on success
 */
esp_err_t husb238_is_attached(husb238_handle_t *handle, bool *attached);

/**
 * @brief Get PD negotiation response code
 *
 * @param[in] handle Device handle
 * @param[out] response Response code
 * @return ESP_OK on success
 */
esp_err_t husb238_get_pd_response(husb238_handle_t *handle, husb238_response_t *response);

/**
 * @brief Check if 5V contract is active
 *
 * @param[in] handle Device handle
 * @param[out] contract true if 5V contract active
 * @return ESP_OK on success
 */
esp_err_t husb238_get_5v_contract_voltage(husb238_handle_t *handle, bool *contract);

/**
 * @brief Get 5V contract current capability
 *
 * @param[in] handle Device handle
 * @param[out] current Current capability
 * @return ESP_OK on success
 */
esp_err_t husb238_get_5v_contract_current(husb238_handle_t *handle, husb238_5v_current_t *current);

/**
 * @brief Get current PD source voltage
 *
 * @param[in] handle Device handle
 * @param[out] voltage Current voltage
 * @return ESP_OK on success
 */
esp_err_t husb238_get_pd_src_voltage(husb238_handle_t *handle, husb238_voltage_t *voltage);

/**
 * @brief Get current PD source current
 *
 * @param[in] handle Device handle
 * @param[out] current Current capability
 * @return ESP_OK on success
 */
esp_err_t husb238_get_pd_src_current(husb238_handle_t *handle, husb238_current_t *current);

/**
 * @brief Check if a specific voltage is available from the source
 *
 * @param[in] handle Device handle
 * @param[in] pd_sel Voltage to check
 * @param[out] detected true if available
 * @return ESP_OK on success
 */
esp_err_t husb238_is_voltage_detected(husb238_handle_t *handle, husb238_pd_selection_t pd_sel, bool *detected);

/**
 * @brief Get maximum current for a specific voltage
 *
 * @param[in] handle Device handle
 * @param[in] pd_sel Voltage to query
 * @param[out] current Maximum current available
 * @return ESP_OK on success
 */
esp_err_t husb238_get_current_for_voltage(husb238_handle_t *handle, husb238_pd_selection_t pd_sel, husb238_current_t *current);

/**
 * @brief Get currently selected PD voltage
 *
 * @param[in] handle Device handle
 * @param[out] selection Current selection
 * @return ESP_OK on success
 */
esp_err_t husb238_get_selected_pd(husb238_handle_t *handle, husb238_pd_selection_t *selection);

/**
 * @brief Select desired PD voltage
 *
 * @param[in] handle Device handle
 * @param[in] pd_sel Desired voltage
 * @return ESP_OK on success
 */
esp_err_t husb238_select_pd(husb238_handle_t *handle, husb238_pd_selection_t pd_sel);

/**
 * @brief Request PD negotiation (send GO command)
 *
 * Call this after husb238_select_pd() to initiate voltage change.
 *
 * @param[in] handle Device handle
 * @return ESP_OK on success
 */
esp_err_t husb238_request_pd(husb238_handle_t *handle);

/**
 * @brief Request source capabilities query
 *
 * @param[in] handle Device handle
 * @return ESP_OK on success
 */
esp_err_t husb238_get_source_capabilities(husb238_handle_t *handle);

/**
 * @brief Hard reset the HUSB238
 *
 * @param[in] handle Device handle
 * @return ESP_OK on success
 */
esp_err_t husb238_reset(husb238_handle_t *handle);

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * @brief Convert current enum to milliamps
 *
 * @param[in] current Current enum value
 * @return Current in milliamps
 */
uint16_t husb238_current_to_ma(husb238_current_t current);

/**
 * @brief Convert voltage enum to millivolts
 *
 * @param[in] voltage Voltage enum value
 * @return Voltage in millivolts
 */
uint16_t husb238_voltage_to_mv(husb238_voltage_t voltage);

/**
 * @brief Convert 5V current enum to milliamps
 *
 * @param[in] current 5V current enum value
 * @return Current in milliamps
 */
uint16_t husb238_5v_current_to_ma(husb238_5v_current_t current);

/* ============================================================================
 * High-Level Controller API
 * ========================================================================== */

/**
 * @brief Available voltage information
 */
typedef struct {
    husb238_pd_selection_t selection;   /**< PD selection enum */
    uint16_t voltage_mv;                /**< Voltage in millivolts */
    uint16_t max_current_ma;            /**< Maximum current in milliamps */
    bool available;                      /**< true if available from source */
} husb238_voltage_info_t;

/**
 * @brief Controller state machine states
 */
typedef enum {
    HUSB238_STATE_NOT_PRESENT,      /**< Device not detected on I2C */
    HUSB238_STATE_INITIALIZING,     /**< Device detected, initializing */
    HUSB238_STATE_WAITING_PD,       /**< Waiting for PD source connection */
    HUSB238_STATE_CONNECTED,        /**< PD source connected and ready */
    HUSB238_STATE_ERROR             /**< Communication error */
} husb238_state_t;

/**
 * @brief Voltage change callback function type
 *
 * Called when the output voltage changes.
 *
 * @param[in] voltage_mv New voltage in millivolts
 * @param[in] current_ma New current in milliamps
 * @param[in] user_data User-provided context
 */
typedef void (*husb238_voltage_change_cb_t)(uint16_t voltage_mv, uint16_t current_ma, void *user_data);

/**
 * @brief State change callback function type
 *
 * Called when the controller state changes.
 *
 * @param[in] state New state
 * @param[in] user_data User-provided context
 */
typedef void (*husb238_state_change_cb_t)(husb238_state_t state, void *user_data);

/**
 * @brief Controller configuration
 */
typedef struct {
    /* I2C - Option A: Controller creates I2C bus */
    int sda_gpio;                           /**< SDA GPIO (ignored if i2c_bus provided) */
    int scl_gpio;                           /**< SCL GPIO (ignored if i2c_bus provided) */
    uint32_t i2c_freq_hz;                   /**< I2C frequency, 0 = 100kHz default */

    /* I2C - Option B: User provides existing bus */
    i2c_master_bus_handle_t i2c_bus;        /**< User's I2C bus, NULL = create internally */
    uint8_t i2c_addr;                       /**< I2C address, 0 = default (0x08) */

    /* Behavior */
    bool force_5v_on_connect;               /**< Force safe 5V on connect/reconnect */

    /* Callbacks */
    husb238_voltage_change_cb_t on_voltage_change;  /**< Voltage change callback */
    husb238_state_change_cb_t on_state_change;      /**< State change callback */
    void *user_data;                                /**< User context for callbacks */
} husb238_controller_config_t;

/**
 * @brief Controller handle (opaque pointer)
 */
typedef struct husb238_controller* husb238_controller_handle_t;

/**
 * @brief Initialize the HUSB238 controller
 *
 * Creates a background FreeRTOS task that handles:
 * - Device detection and initialization
 * - Hot-plug detection and reconnection
 * - Button-based voltage cycling
 * - State management and callbacks
 *
 * @param[in] config Controller configuration
 * @param[out] handle_out Controller handle
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid parameters
 *     - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t husb238_controller_init(const husb238_controller_config_t *config,
                                   husb238_controller_handle_t *handle_out);

/**
 * @brief Deinitialize the controller
 *
 * Stops the background task and frees resources.
 *
 * @param[in] handle Controller handle
 * @return ESP_OK on success
 */
esp_err_t husb238_controller_deinit(husb238_controller_handle_t handle);

/**
 * @brief Get current controller state
 *
 * @param[in] handle Controller handle
 * @return Current state
 */
husb238_state_t husb238_controller_get_state(husb238_controller_handle_t handle);

/**
 * @brief Get number of available voltages
 *
 * @param[in] handle Controller handle
 * @return Number of available voltages (0-6)
 */
int husb238_controller_get_voltage_count(husb238_controller_handle_t handle);

/**
 * @brief Get voltage info by index
 *
 * @param[in] handle Controller handle
 * @param[in] index Voltage index (0 to count-1)
 * @param[out] info Voltage information
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid index
 */
esp_err_t husb238_controller_get_voltage_info(husb238_controller_handle_t handle,
                                               int index,
                                               husb238_voltage_info_t *info);

/**
 * @brief Get current voltage index
 *
 * @param[in] handle Controller handle
 * @return Current index (0 to count-1), or -1 if none selected
 */
int husb238_controller_get_current_index(husb238_controller_handle_t handle);

/**
 * @brief Select voltage by index
 *
 * @param[in] handle Controller handle
 * @param[in] index Voltage index (0 to count-1)
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_STATE: Not connected
 *     - ESP_ERR_INVALID_ARG: Invalid index
 */
esp_err_t husb238_controller_select_voltage(husb238_controller_handle_t handle, int index);

/**
 * @brief Cycle to next available voltage
 *
 * Wraps around to first voltage after last.
 *
 * @param[in] handle Controller handle
 * @return ESP_OK on success
 */
esp_err_t husb238_controller_next_voltage(husb238_controller_handle_t handle);

/**
 * @brief Force output to safe 5V
 *
 * Use this for safe shutdown or error recovery.
 *
 * @param[in] handle Controller handle
 * @return ESP_OK on success
 */
esp_err_t husb238_controller_force_5v(husb238_controller_handle_t handle);

/**
 * @brief Request a specific voltage
 *
 * Searches available voltages and selects if found.
 *
 * @param[in] handle Controller handle
 * @param[in] voltage_mv Desired voltage in millivolts (5000, 9000, 12000, 15000, 18000, 20000)
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_STATE: Not connected
 *     - ESP_ERR_NOT_FOUND: Voltage not available from power supply
 */
esp_err_t husb238_controller_request_voltage(husb238_controller_handle_t handle, uint16_t voltage_mv);

/**
 * @brief Get current actual voltage
 *
 * @param[in] handle Controller handle
 * @return Voltage in millivolts (0 if not connected)
 */
uint16_t husb238_controller_get_voltage_mv(husb238_controller_handle_t handle);

/**
 * @brief Get current actual current capability
 *
 * @param[in] handle Controller handle
 * @return Current in milliamps (0 if not connected)
 */
uint16_t husb238_controller_get_current_ma(husb238_controller_handle_t handle);

/**
 * @brief Set log verbosity for HUSB238 component
 *
 * Controls the amount of logging output from the controller.
 *
 * @param[in] level ESP-IDF log level (ESP_LOG_NONE, ESP_LOG_ERROR, ESP_LOG_WARN,
 *                  ESP_LOG_INFO, ESP_LOG_DEBUG, ESP_LOG_VERBOSE)
 */
void husb238_set_log_level(esp_log_level_t level);

#ifdef __cplusplus
}
#endif
