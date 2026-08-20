// Components
#include "Stepper.h"
#include "husb238.h"

// ESP-IDF
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

[[maybe_unused]] static const char *TAG { "Main" };

// #define DISPLAY
#ifdef DISPLAY
// Components
#include "Display.h"
#include "hsm.hpp"
#include "config.h"

// ESP-IDF
#include "esp_timer.h"

const int colorR { 80 };
const int colorG { 10 };
const int colorB { 200 };

/* @brief Convert microseconds to milliseconds */
uint32_t millis() {
    return esp_timer_get_time() / 1000;  
}

Display display(RGB_ADDR, LCD_COLS, LCD_ROWS);

/* @brief initialize the LCD and master IIC */
void setup() {
    display.init();
    display.setRGB(colorR, colorG, colorB);

    ESP_LOGD(TAG, "hello, world!");

    vTaskDelay(pdMS_TO_TICKS(100));
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Initializing LCD...");
    display.init();
    display.setRGB(colorR, colorG, colorB);
    ESP_LOGI(TAG, "LCD initialized!");
 
    // Hand the display to the state machine and run it forever.
    hsm_init(display);
    while (true) {
        hsm_run();
    }
}
#endif

extern "C" void app_main(void) {
    stepper_init();
    stepper_start_sweep();

    while (true) {
        // your detection code goes here; tag hits with the current angle:
        // if (camera_detected()) mark(stepper_position());
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_LOGI(TAG, "pos=%ld", (long)stepper_position());
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
