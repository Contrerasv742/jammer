idf_component_register (
    SRCS "Stepper.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio
    PRIV_REQUIRES esp_driver_gptimer
)
