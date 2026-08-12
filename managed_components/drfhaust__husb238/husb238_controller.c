/**
 * @file husb238_controller.c
 * @brief HUSB238 High-Level Controller Implementation
 *
 * Handles device detection, reconnection, and voltage selection
 */

#include "husb238.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2c_master.h"
#include <string.h>

static const char *TAG = "HUSB238_CTRL";

// =====================================================
// Controller Internal Structure
// =====================================================

struct husb238_controller {
    // Configuration
    husb238_controller_config_t config;

    // I2C
    i2c_master_bus_handle_t i2c_bus;
    bool i2c_bus_owned;  // true if we created it, false if user provided
    husb238_handle_t device;

    // State
    husb238_state_t state;
    bool device_initialized;

    // Voltage tracking
    husb238_voltage_info_t voltages[6];
    int voltage_count;
    int current_index;
    uint16_t actual_voltage_mv;
    uint16_t actual_current_ma;

    // Task
    TaskHandle_t task_handle;
    bool task_running;

    // Thread safety
    SemaphoreHandle_t mutex;
};

// =====================================================
// Internal Helpers
// =====================================================

static bool controller_lock(husb238_controller_handle_t handle) {
    return xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}

static void controller_unlock(husb238_controller_handle_t handle) {
    xSemaphoreGive(handle->mutex);
}

static void set_state(husb238_controller_handle_t handle, husb238_state_t new_state) {
    if (handle->state != new_state) {
        handle->state = new_state;
        if (handle->config.on_state_change) {
            handle->config.on_state_change(new_state, handle->config.user_data);
        }
    }
}

static inline uint8_t get_i2c_addr(husb238_controller_handle_t handle) {
    return handle->config.i2c_addr;  // Default set in init
}

static bool device_is_present(husb238_controller_handle_t handle) {
    // Temporarily suppress I2C error logs during probe
    esp_log_level_t prev_level = esp_log_level_get("i2c.master");
    esp_log_level_set("i2c.master", ESP_LOG_NONE);

    bool present = (i2c_master_probe(handle->i2c_bus, get_i2c_addr(handle), 50) == ESP_OK);

    esp_log_level_set("i2c.master", prev_level);
    return present;
}

static bool try_init_device(husb238_controller_handle_t handle) {
    husb238_config_t cfg = {
        .i2c_bus = handle->i2c_bus,
        .i2c_addr = get_i2c_addr(handle),
        .scl_speed_hz = handle->config.i2c_freq_hz,
    };

    esp_err_t ret = husb238_init(&cfg, &handle->device);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "HUSB238 initialized");
        handle->device_initialized = true;
        return true;
    }
    return false;
}

static void scan_available_voltages(husb238_controller_handle_t handle) {
    husb238_pd_selection_t pd_options[] = {
        HUSB238_PD_SRC_5V,
        HUSB238_PD_SRC_9V,
        HUSB238_PD_SRC_12V,
        HUSB238_PD_SRC_15V,
        HUSB238_PD_SRC_18V,
        HUSB238_PD_SRC_20V
    };
    uint16_t voltages_mv[] = {5000, 9000, 12000, 15000, 18000, 20000};

    handle->voltage_count = 0;
    memset(handle->voltages, 0, sizeof(handle->voltages));

    ESP_LOGI(TAG, "Scanning available voltages...");

    for (int i = 0; i < 6; i++) {
        bool detected = false;
        if (husb238_is_voltage_detected(&handle->device, pd_options[i], &detected) == ESP_OK && detected) {
            husb238_current_t max_current;
            uint16_t current_ma = 0;

            if (husb238_get_current_for_voltage(&handle->device, pd_options[i], &max_current) == ESP_OK) {
                current_ma = husb238_current_to_ma(max_current);
            }

            int idx = handle->voltage_count;
            handle->voltages[idx].selection = pd_options[i];
            handle->voltages[idx].voltage_mv = voltages_mv[i];
            handle->voltages[idx].max_current_ma = current_ma;
            handle->voltages[idx].available = true;

            ESP_LOGI(TAG, "  [%d] %dV @ %dmA", idx, voltages_mv[i] / 1000, current_ma);
            handle->voltage_count++;
        }
    }

    ESP_LOGI(TAG, "Found %d available voltage(s)", handle->voltage_count);
}

static esp_err_t select_voltage_internal(husb238_controller_handle_t handle, int index) {
    if (index < 0 || index >= handle->voltage_count) {
        return ESP_ERR_INVALID_ARG;
    }

    husb238_voltage_info_t *info = &handle->voltages[index];
    uint16_t requested_mv = info->voltage_mv;

    ESP_LOGI(TAG, "Requesting %dV @ %dmA", requested_mv / 1000, info->max_current_ma);

    esp_err_t ret = husb238_select_pd(&handle->device, info->selection);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to select voltage");
        return ret;
    }

    ret = husb238_request_pd(&handle->device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to request PD");
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(500));  // Wait for negotiation

    // Read back actual values from device
    husb238_voltage_t voltage;
    husb238_current_t current;
    if (husb238_get_pd_src_voltage(&handle->device, &voltage) == ESP_OK) {
        handle->actual_voltage_mv = husb238_voltage_to_mv(voltage);
    }
    if (husb238_get_pd_src_current(&handle->device, &current) == ESP_OK) {
        handle->actual_current_ma = husb238_current_to_ma(current);
    }

    // Verify voltage actually changed
    if (handle->actual_voltage_mv != requested_mv) {
        ESP_LOGW(TAG, "Voltage mismatch! Requested %dmV, got %dmV",
                 requested_mv, handle->actual_voltage_mv);
        // Still update index and notify - caller can check actual vs requested
    }

    handle->current_index = index;

    ESP_LOGI(TAG, "Now at %dmV @ %dmA", handle->actual_voltage_mv, handle->actual_current_ma);

    if (handle->config.on_voltage_change) {
        handle->config.on_voltage_change(handle->actual_voltage_mv, handle->actual_current_ma,
                                         handle->config.user_data);
    }

    // Return error if voltage didn't match
    if (handle->actual_voltage_mv != requested_mv) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

static int find_5v_index(husb238_controller_handle_t handle) {
    for (int i = 0; i < handle->voltage_count; i++) {
        if (handle->voltages[i].voltage_mv == 5000) {
            return i;
        }
    }
    return 0;  // Default to first available
}

// =====================================================
// Controller Task
// =====================================================

static void controller_task(void *arg) {
    husb238_controller_handle_t handle = (husb238_controller_handle_t)arg;
    int error_count = 0;
    const int MAX_ERRORS = 3;
    bool was_attached = false;

    ESP_LOGI(TAG, "Controller task started");
    set_state(handle, HUSB238_STATE_NOT_PRESENT);

    while (handle->task_running) {
        // Device not present - poll for it
        if (handle->state == HUSB238_STATE_NOT_PRESENT ||
            handle->state == HUSB238_STATE_ERROR) {

            if (device_is_present(handle)) {
                ESP_LOGI(TAG, "HUSB238 detected");
                set_state(handle, HUSB238_STATE_INITIALIZING);
                vTaskDelay(pdMS_TO_TICKS(100));

                if (try_init_device(handle)) {
                    error_count = 0;
                    was_attached = false;
                    handle->voltage_count = 0;
                    handle->current_index = -1;
                    set_state(handle, HUSB238_STATE_WAITING_PD);
                } else {
                    set_state(handle, HUSB238_STATE_NOT_PRESENT);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Device initialized - monitor status (suppress I2C logs during check)
        esp_log_level_t prev_level = esp_log_level_get("i2c.master");
        esp_log_level_set("i2c.master", ESP_LOG_NONE);

        bool attached = false;
        esp_err_t ret = husb238_is_attached(&handle->device, &attached);

        esp_log_level_set("i2c.master", prev_level);

        if (ret != ESP_OK) {
            error_count++;
            if (error_count >= MAX_ERRORS) {
                ESP_LOGW(TAG, "Communication lost");
                handle->device_initialized = false;
                was_attached = false;
                handle->voltage_count = 0;
                handle->current_index = -1;
                handle->actual_voltage_mv = 0;
                handle->actual_current_ma = 0;
                set_state(handle, HUSB238_STATE_NOT_PRESENT);
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        error_count = 0;

        // Handle attachment changes
        if (attached && !was_attached) {
            ESP_LOGI(TAG, "PD source connected");
            vTaskDelay(pdMS_TO_TICKS(500));

            husb238_response_t response;
            if (husb238_get_pd_response(&handle->device, &response) == ESP_OK &&
                response == HUSB238_RESPONSE_SUCCESS) {

                scan_available_voltages(handle);

                if (handle->voltage_count > 0) {
                    set_state(handle, HUSB238_STATE_CONNECTED);

                    // Force 5V on connect if configured
                    if (handle->config.force_5v_on_connect) {
                        int idx = find_5v_index(handle);
                        select_voltage_internal(handle, idx);
                    } else {
                        handle->current_index = 0;
                        // Read current voltage
                        husb238_voltage_t voltage;
                        husb238_current_t current;
                        if (husb238_get_pd_src_voltage(&handle->device, &voltage) == ESP_OK) {
                            handle->actual_voltage_mv = husb238_voltage_to_mv(voltage);
                        }
                        if (husb238_get_pd_src_current(&handle->device, &current) == ESP_OK) {
                            handle->actual_current_ma = husb238_current_to_ma(current);
                        }
                    }
                }
            }
        } else if (!attached && was_attached) {
            ESP_LOGI(TAG, "PD source disconnected");
            handle->voltage_count = 0;
            handle->current_index = -1;
            handle->actual_voltage_mv = 0;
            handle->actual_current_ma = 0;
            set_state(handle, HUSB238_STATE_WAITING_PD);
        }
        was_attached = attached;

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Controller task stopped");
    vTaskDelete(NULL);
}

// =====================================================
// Public API Implementation
// =====================================================

esp_err_t husb238_controller_init(const husb238_controller_config_t *config,
                                   husb238_controller_handle_t *handle_out) {
    if (!config || !handle_out) {
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate controller
    struct husb238_controller *ctrl = calloc(1, sizeof(struct husb238_controller));
    if (!ctrl) {
        return ESP_ERR_NO_MEM;
    }

    // Copy config
    memcpy(&ctrl->config, config, sizeof(husb238_controller_config_t));

    // Set defaults
    if (ctrl->config.i2c_addr == 0) {
        ctrl->config.i2c_addr = HUSB238_I2CADDR_DEFAULT;
    }
    if (ctrl->config.i2c_freq_hz == 0) {
        ctrl->config.i2c_freq_hz = 100000;
    }

    esp_err_t ret;

    // Create mutex
    ctrl->mutex = xSemaphoreCreateMutex();
    if (!ctrl->mutex) {
        free(ctrl);
        return ESP_ERR_NO_MEM;
    }

    // Initialize I2C bus - either use provided or create new
    if (config->i2c_bus != NULL) {
        // User provided I2C bus
        ctrl->i2c_bus = config->i2c_bus;
        ctrl->i2c_bus_owned = false;
        ESP_LOGI(TAG, "Using user-provided I2C bus");
    } else {
        // Create I2C bus internally
        i2c_master_bus_config_t bus_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = I2C_NUM_0,
            .scl_io_num = config->scl_gpio,
            .sda_io_num = config->sda_gpio,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };

        ret = i2c_new_master_bus(&bus_config, &ctrl->i2c_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
            vSemaphoreDelete(ctrl->mutex);
            free(ctrl);
            return ret;
        }
        ctrl->i2c_bus_owned = true;
        ESP_LOGI(TAG, "I2C bus initialized (SDA=%d, SCL=%d)", config->sda_gpio, config->scl_gpio);
    }

    // Initialize state
    ctrl->state = HUSB238_STATE_NOT_PRESENT;
    ctrl->current_index = -1;

    // Start task
    ctrl->task_running = true;
    BaseType_t task_ret = xTaskCreate(controller_task, "husb238_ctrl", 4096, ctrl, 5, &ctrl->task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        i2c_del_master_bus(ctrl->i2c_bus);
        vSemaphoreDelete(ctrl->mutex);
        free(ctrl);
        return ESP_FAIL;
    }

    *handle_out = ctrl;
    ESP_LOGI(TAG, "Controller initialized");
    return ESP_OK;
}

esp_err_t husb238_controller_deinit(husb238_controller_handle_t handle) {
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->task_running = false;
    vTaskDelay(pdMS_TO_TICKS(200));  // Wait for task to stop

    if (handle->device_initialized) {
        husb238_deinit(&handle->device);
    }

    // Only delete I2C bus if we created it
    if (handle->i2c_bus && handle->i2c_bus_owned) {
        i2c_del_master_bus(handle->i2c_bus);
    }

    vSemaphoreDelete(handle->mutex);
    free(handle);

    return ESP_OK;
}

husb238_state_t husb238_controller_get_state(husb238_controller_handle_t handle) {
    return handle ? handle->state : HUSB238_STATE_NOT_PRESENT;
}

int husb238_controller_get_voltage_count(husb238_controller_handle_t handle) {
    return handle ? handle->voltage_count : 0;
}

esp_err_t husb238_controller_get_voltage_info(husb238_controller_handle_t handle,
                                               int index,
                                               husb238_voltage_info_t *info) {
    if (!handle || !info || index < 0 || index >= handle->voltage_count) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(info, &handle->voltages[index], sizeof(husb238_voltage_info_t));
    return ESP_OK;
}

int husb238_controller_get_current_index(husb238_controller_handle_t handle) {
    return handle ? handle->current_index : -1;
}

esp_err_t husb238_controller_select_voltage(husb238_controller_handle_t handle, int index) {
    if (!handle || handle->state != HUSB238_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!controller_lock(handle)) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = select_voltage_internal(handle, index);
    controller_unlock(handle);
    return ret;
}

esp_err_t husb238_controller_next_voltage(husb238_controller_handle_t handle) {
    if (!handle || handle->state != HUSB238_STATE_CONNECTED || handle->voltage_count == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    int next_idx = (handle->current_index + 1) % handle->voltage_count;
    return husb238_controller_select_voltage(handle, next_idx);
}

esp_err_t husb238_controller_force_5v(husb238_controller_handle_t handle) {
    if (!handle || handle->state != HUSB238_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    int idx = find_5v_index(handle);
    return husb238_controller_select_voltage(handle, idx);
}

esp_err_t husb238_controller_request_voltage(husb238_controller_handle_t handle, uint16_t voltage_mv) {
    if (!handle || handle->state != HUSB238_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    // Find matching voltage in available list
    for (int i = 0; i < handle->voltage_count; i++) {
        if (handle->voltages[i].voltage_mv == voltage_mv) {
            return husb238_controller_select_voltage(handle, i);
        }
    }

    ESP_LOGW(TAG, "Requested voltage %dmV not available", voltage_mv);
    return ESP_ERR_NOT_FOUND;
}

uint16_t husb238_controller_get_voltage_mv(husb238_controller_handle_t handle) {
    return handle ? handle->actual_voltage_mv : 0;
}

uint16_t husb238_controller_get_current_ma(husb238_controller_handle_t handle) {
    return handle ? handle->actual_current_ma : 0;
}

void husb238_set_log_level(esp_log_level_t level) {
    esp_log_level_set("HUSB238", level);
    esp_log_level_set("HUSB238_CTRL", level);
}
