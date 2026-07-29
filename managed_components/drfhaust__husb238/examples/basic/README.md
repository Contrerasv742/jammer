# HUSB238 Basic Example

Minimal example demonstrating the HUSB238 controller API with automatic device management.

## Features

- Controller manages I2C bus internally
- Callbacks for voltage and state changes
- Periodic display of available voltages
- Uses Kconfig for pin configuration

## Hardware Required

- ESP32 or compatible board
- HUSB238 breakout board
- USB-C PD power supply

## Wiring

| HUSB238 | ESP32 |
|---------|-------|
| SDA | GPIO21 (configurable via menuconfig) |
| SCL | GPIO22 (configurable via menuconfig) |
| GND | GND |

## Configuration

```bash
idf.py menuconfig
```

Navigate to: `Component config` → `HUSB238 Configuration`

## Build and Flash

```bash
idf.py build flash monitor
```

## Expected Output

```
I (xxx) husb238_basic: HUSB238 Basic Example
I (xxx) husb238_basic: Controller started
I (xxx) husb238_basic: State: CONNECTED
I (xxx) husb238_basic: Voltage changed: 5.00V @ 3.00A
I (xxx) husb238_basic: Available: 4 voltages, current index: 0
I (xxx) husb238_basic:   [0] 5V @ 3000mA <-- current
I (xxx) husb238_basic:   [1] 9V @ 3000mA
I (xxx) husb238_basic:   [2] 12V @ 3000mA
I (xxx) husb238_basic:   [3] 20V @ 2250mA
```
