# HUSB238 Arduino-as-Component Example

Using the HUSB238 component with Arduino-ESP32 as an ESP-IDF component.

## Features

- Arduino setup()/loop() programming model
- Serial output for status and voltage info
- Periodic display of available voltages
- Compatible with arduino-esp32 as component

## Prerequisites

1. Arduino-ESP32 installed as ESP-IDF component
2. See: https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html

## Setup

1. Install arduino-esp32 as a component in your ESP-IDF project
2. Update `CMakeLists.txt` with the path to arduino-esp32:

```cmake
set(ARDUINO_PATH "/path/to/arduino-esp32")
list(APPEND EXTRA_COMPONENT_DIRS ${ARDUINO_PATH}/components)
```

## Hardware Required

- ESP32 or compatible board
- HUSB238 breakout board
- USB-C PD power supply

## Wiring

| HUSB238 | ESP32 |
|---------|-------|
| SDA | GPIO21 |
| SCL | GPIO22 |
| GND | GND |

## Build and Flash

```bash
idf.py build flash monitor
```

## Expected Output

```
HUSB238 Arduino Example
I2C initialized on SDA=21, SCL=22
Controller started!
State: CONNECTED
Voltage: 5.00V @ 3.00A
Available: 4 voltages | Current: 5000mV (index 0)
Voltages:
  [0] 5V @ 3000mA *
  [1] 9V @ 3000mA
  [2] 12V @ 3000mA
  [3] 20V @ 2250mA
```

## Notes

- Arduino Wire library and ESP-IDF I2C use different APIs
- This example creates a separate I2C bus for HUSB238
- For shared I2C bus scenarios, use the low-level HUSB238 API
