# HUSB238 Button Voltage Select Example

Demonstrates button-based voltage cycling with user-managed I2C bus.

## Features

- User-managed I2C bus (allows sharing with other devices)
- Application-level button handling with debouncing
- GPIO interrupt-driven button detection
- Press button to cycle through available PD voltages

## Hardware Required

- ESP32 or compatible board
- HUSB238 breakout board
- Push button (connected to GPIO0 by default)
- USB-C PD power supply

## Wiring

| HUSB238 | ESP32 |
|---------|-------|
| SDA | GPIO21 (configurable) |
| SCL | GPIO22 (configurable) |
| GND | GND |

| Button | ESP32 |
|--------|-------|
| One side | GPIO0 (configurable in code) |
| Other side | GND |

## Configuration

Edit `main.c` to change pin assignments:
```c
#define BUTTON_GPIO     0   /* Change to your button GPIO */
```

Or use menuconfig for I2C pins:
```bash
idf.py menuconfig
```

## Build and Flash

```bash
idf.py build flash monitor
```

## Usage

1. Connect USB-C PD power supply to HUSB238
2. Wait for "PD source connected!" message
3. Press the button to cycle through available voltages

## Expected Output

```
I (xxx) husb238_button: HUSB238 Button Voltage Select Example
I (xxx) husb238_button: I2C: SDA=21, SCL=22 | Button: GPIO 0
I (xxx) husb238_button: PD source connected! Press button to change voltage.
I (xxx) husb238_button: >>> Output: 5.0V @ 3.00A <<<
I (xxx) husb238_button: Button pressed - cycling voltage
I (xxx) husb238_button: >>> Output: 9.0V @ 3.00A <<<
```
