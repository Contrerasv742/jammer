cmake_minimum_required(VERSION 3.16)

# Arduino as ESP-IDF component
# Uncomment and set path to your arduino-esp32 installation:
# set(ARDUINO_PATH "/path/to/arduino-esp32")
# list(APPEND EXTRA_COMPONENT_DIRS ${ARDUINO_PATH}/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(husb238_arduino)
