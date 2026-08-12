idf_component_register(
    SRCS "main.cpp" "hsm.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c
    REQUIRES esp_timer
    REQUIRES display
    REQUIRES motor
    REQUIRES husb238
    REQUIRES esp_driver_ledc
    REQUIRES freertos
)
