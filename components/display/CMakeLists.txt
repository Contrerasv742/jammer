idf_component_register(
    SRCS "Display.cpp" "Wire.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c
)
