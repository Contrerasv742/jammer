idf_component_register(
    SRCS "main.cpp" "hsm.cpp"
    INCLUDE_DIRS "."

    # ESP
    REQUIRES esp_driver_i2c
    REQUIRES esp_timer
    REQUIRES esp_driver_ledc
    REQUIRES freertos

    # Components
    REQUIRES display
    REQUIRES stepper
    REQUIRES husb238
)
