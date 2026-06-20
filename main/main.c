/**
 * sense-pong - main entry point
 *
 * Powers on the SenseCAP Indicator (ESP32-S3 + 480x480 LCD) and launches
 * the Pong UI. No networking, no sensors, no player input yet — the device
 * boots into a welcome screen with a START button.
 */

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp_board.h"
#include "lv_port.h"
#include "pong.h"
#include "joystick.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "sense-pong boot");

    /* NVS (board init may use it for calibration) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Board + LCD + LVGL */
    ESP_ERROR_CHECK(bsp_board_init());
    lv_port_init();

    /* Start the RP2040<->ESP32 UART listener for the joystick. */
    joystick_init();

    /* Show the welcome screen (inside the LVGL lock) */
    lv_port_sem_take();
    pong_start();
    lv_port_sem_give();

    ESP_LOGI(TAG, "Pong UI ready. Nothing else to do.");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
