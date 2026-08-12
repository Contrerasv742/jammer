# HUSB238 - USB Power Delivery Sink Controller Driver

[![Component Registry](https://components.espressif.com/components/drfhaust/husb238/badge.svg)](https://components.espressif.com/components/drfhaust/husb238)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A complete ESP-IDF driver for the **Hynetek HUSB238** USB Power Delivery sink controller. Enables dynamic voltage selection (5V-20V) from USB-C PD power supplies.

## Features

- **High-Level Controller API** - Automatic device detection, hot-plug handling, and state management
- **Low-Level Register API** - Direct hardware access for advanced use cases
- **Callbacks** - Event notifications for voltage and state changes
- **Hot-Plug Safe** - Graceful handling of power supply connect/disconnect
- **Thread-Safe** - Mutex-protected operations for multi-task environments
- **Flexible I2C** - Use controller-managed or user-provided I2C bus
- **Kconfig Integration** - Configure pins via `idf.py menuconfig`

## Supported Chips

- ESP32, ESP32-S2, ESP32-S3
- ESP32-C3, ESP32-C5, ESP32-C6
- ESP32-H2

## Hardware

### HUSB238 Breakout Boards

- [Adafruit HUSB238 USB Type C Power Delivery Breakout](https://www.adafruit.com/product/5807)
- Generic HUSB238 breakout boards

### Wiring

| HUSB238 | ESP32 | Description |
|---------|-------|-------------|
| SDA     | GPIO21 (configurable) | I2C Data |
| SCL     | GPIO22 (configurable) | I2C Clock |
| GND     | GND   | Ground |
| VIN     | -     | Powered by USB-C |

**Note:** External I2C pull-ups (2.2k-4.7k to 3.3V) are recommended. The HUSB238 requires USB-C power to operate.

## Installation

### ESP-IDF Component Registry

```bash
idf.py add-dependency "drfhaust/husb238"
```

### Manual Installation

Copy the `husb238` folder to your project's `components/` directory.

## Quick Start

### Simplest Usage

```c
#include "husb238.h"

void app_main(void)
{
    husb238_controller_config_t config = {
        .sda_gpio = 21,
        .scl_gpio = 22,
        .force_5v_on_connect = true,
    };

    husb238_controller_handle_t ctrl;
    husb238_controller_init(&config, &ctrl);

    // Controller runs in background, handles device management automatically
    // Use husb238_controller_next_voltage() or husb238_controller_select_voltage()
    // to change voltage programmatically
}
```

### With Callbacks

```c
void on_voltage_change(uint16_t voltage_mv, uint16_t current_ma, void *user_data)
{
    printf("New voltage: %dmV @ %dmA\n", voltage_mv, current_ma);
}

void on_state_change(husb238_state_t state, void *user_data)
{
    const char *states[] = {"NOT_PRESENT", "INITIALIZING", "WAITING_PD", "CONNECTED", "ERROR"};
    printf("State: %s\n", states[state]);
}

void app_main(void)
{
    husb238_controller_config_t config = {
        .sda_gpio = 21,
        .scl_gpio = 22,
        .force_5v_on_connect = true,
        .on_voltage_change = on_voltage_change,
        .on_state_change = on_state_change,
    };

    husb238_controller_handle_t ctrl;
    husb238_controller_init(&config, &ctrl);
}
```

### User-Managed I2C Bus

```c
// Create your own I2C bus (allows sharing with other devices)
i2c_master_bus_handle_t i2c_bus;
i2c_master_bus_config_t bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_NUM_0,
    .scl_io_num = 22,
    .sda_io_num = 21,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
i2c_new_master_bus(&bus_config, &i2c_bus);

// Pass to controller
husb238_controller_config_t config = {
    .i2c_bus = i2c_bus,     // Use existing bus
    .force_5v_on_connect = true,
};

husb238_controller_handle_t ctrl;
husb238_controller_init(&config, &ctrl);
```

## API Reference

### Controller API (High-Level)

#### Initialization

```c
// Initialize controller
esp_err_t husb238_controller_init(const husb238_controller_config_t *config,
                                   husb238_controller_handle_t *handle_out);

// Cleanup
esp_err_t husb238_controller_deinit(husb238_controller_handle_t handle);
```

#### State Management

```c
// Get current state
husb238_state_t husb238_controller_get_state(husb238_controller_handle_t handle);

// States:
// - HUSB238_STATE_NOT_PRESENT   : Device not on I2C bus
// - HUSB238_STATE_INITIALIZING  : Device found, initializing
// - HUSB238_STATE_WAITING_PD    : Waiting for PD source
// - HUSB238_STATE_CONNECTED     : Ready, voltage selectable
// - HUSB238_STATE_ERROR         : Communication error
```

#### Voltage Selection

```c
// Get number of available voltages (0-6)
int husb238_controller_get_voltage_count(husb238_controller_handle_t handle);

// Get info about a specific voltage
esp_err_t husb238_controller_get_voltage_info(husb238_controller_handle_t handle,
                                               int index,
                                               husb238_voltage_info_t *info);

// Select voltage by index
esp_err_t husb238_controller_select_voltage(husb238_controller_handle_t handle, int index);

// Request specific voltage (e.g., 9000 for 9V, 12000 for 12V, 20000 for 20V)
esp_err_t husb238_controller_request_voltage(husb238_controller_handle_t handle, uint16_t voltage_mv);

// Cycle to next voltage
esp_err_t husb238_controller_next_voltage(husb238_controller_handle_t handle);

// Force safe 5V output
esp_err_t husb238_controller_force_5v(husb238_controller_handle_t handle);

// Get current voltage/current
uint16_t husb238_controller_get_voltage_mv(husb238_controller_handle_t handle);
uint16_t husb238_controller_get_current_ma(husb238_controller_handle_t handle);
int husb238_controller_get_current_index(husb238_controller_handle_t handle);

// Set log verbosity (ESP_LOG_NONE to silence, ESP_LOG_INFO default)
void husb238_set_log_level(esp_log_level_t level);
```

#### Return Values

Voltage selection functions verify the actual voltage after requesting:

| Return | Meaning |
|--------|---------|
| `ESP_OK` | Voltage changed successfully |
| `ESP_ERR_INVALID_RESPONSE` | Request sent, but actual voltage differs from requested |
| `ESP_ERR_NOT_FOUND` | Requested voltage not available from power supply |
| `ESP_ERR_INVALID_STATE` | Not connected to PD source |

### Low-Level API

For direct register access:

```c
// Initialize device
husb238_config_t config = {
    .i2c_bus = i2c_bus,
    .i2c_addr = 0,          // 0 = default 0x08
    .scl_speed_hz = 100000,
};
husb238_handle_t handle;
husb238_init(&config, &handle);

// Check attachment
bool attached;
husb238_is_attached(&handle, &attached);

// Get current voltage
husb238_voltage_t voltage;
husb238_get_pd_src_voltage(&handle, &voltage);
uint16_t mv = husb238_voltage_to_mv(voltage);

// Check available voltages
bool has_12v;
husb238_is_voltage_detected(&handle, HUSB238_PD_SRC_12V, &has_12v);

// Select and request voltage
husb238_select_pd(&handle, HUSB238_PD_SRC_12V);
husb238_request_pd(&handle);

// Cleanup
husb238_deinit(&handle);
```

## Configuration

### Via Kconfig (`idf.py menuconfig`)

Navigate to: `Component config` → `HUSB238 Configuration`

| Option | Default | Description |
|--------|---------|-------------|
| `HUSB238_I2C_SDA_GPIO` | 21 | I2C SDA pin |
| `HUSB238_I2C_SCL_GPIO` | 22 | I2C SCL pin |
| `HUSB238_I2C_FREQ_HZ` | 100000 | I2C frequency |
| `HUSB238_FORCE_5V_ON_CONNECT` | yes | Force 5V on connect |
| `HUSB238_TASK_STACK_SIZE` | 4096 | Controller task stack |
| `HUSB238_TASK_PRIORITY` | 5 | Controller task priority |
| `HUSB238_LOG_LEVEL` | Info | Log verbosity (None/Error/Warn/Info/Debug/Verbose) |

### Silencing Logs at Runtime

```c
#include "husb238.h"

// Silence all HUSB238 logs
husb238_set_log_level(ESP_LOG_NONE);

// Show only errors
husb238_set_log_level(ESP_LOG_ERROR);

// Restore default (Info)
husb238_set_log_level(ESP_LOG_INFO);
```

## Examples

### `/examples/basic`
Minimal example with controller API and Kconfig.

### `/examples/button_select`
Demonstrates button-based voltage cycling with application-managed button handling.

### `/examples/arduino`
Using HUSB238 with Arduino-ESP32 as a component.

## Voltage/Current Tables

### Available Voltages

| Selection | Voltage |
|-----------|---------|
| `HUSB238_PD_SRC_5V` | 5V |
| `HUSB238_PD_SRC_9V` | 9V |
| `HUSB238_PD_SRC_12V` | 12V |
| `HUSB238_PD_SRC_15V` | 15V |
| `HUSB238_PD_SRC_18V` | 18V |
| `HUSB238_PD_SRC_20V` | 20V |

### Current Capabilities

| Enum | Current |
|------|---------|
| `HUSB238_CURRENT_0_5_A` | 0.5A |
| `HUSB238_CURRENT_1_0_A` | 1.0A |
| `HUSB238_CURRENT_1_5_A` | 1.5A |
| `HUSB238_CURRENT_2_0_A` | 2.0A |
| `HUSB238_CURRENT_2_5_A` | 2.5A |
| `HUSB238_CURRENT_3_0_A` | 3.0A |
| ... | ... |
| `HUSB238_CURRENT_5_0_A` | 5.0A |

## Troubleshooting

### Device Not Detected

1. Check I2C wiring (SDA, SCL, GND)
2. Verify pull-up resistors (2.2k-4.7k to 3.3V)
3. Ensure USB-C power is connected to HUSB238
4. Confirm I2C address is 0x08

### Voltage Not Changing

1. Wait for `HUSB238_STATE_CONNECTED` before selecting voltage
2. Check if desired voltage is available from your power supply
3. Not all chargers support all voltages

### I2C Timeouts

1. Reduce I2C frequency to 50000 Hz
2. Check for bus conflicts with other devices
3. Verify pull-up resistor values

## License

MIT License - see [LICENSE](LICENSE)

## Contributing

Contributions welcome! Please open an issue or pull request.

## Credits

- Original Arduino library: [Adafruit_HUSB238](https://github.com/adafruit/Adafruit_HUSB238)
- Hynetek HUSB238 datasheet
