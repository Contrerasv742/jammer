idf_component_register (
    SRCS "Motor.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_ledc
    PRIV_REQUIRES esp_timer
)
