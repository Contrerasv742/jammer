idf_component_register(
    SRCS "husb238.c" "husb238_controller.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
    REQUIRES esp_driver_i2c
)
