/**
 * sense-pong: force the RP2040 into its USB UF2 bootloader from the ESP32-S3.
 *
 * Replaces the broken/missing BOOTSEL button. Repeatable, software-driven.
 *
 * ONE WIRE REQUIRED (one-time solder):
 *   RP2040 BOOTSEL pad  ->  ESP32-S3 GPIO (RP2040_BOOTSEL_GPIO below)
 *
 *   The RP2040's BOOTSEL is its pin 1 ("nBOOT" / "GPIO0-BOOT"). On the
 *   SenseCAP Indicator PCB it is exposed as a small test pad near the RP2040
 *   (often labeled BOOT, or the via next to the BOOTSEL tactile switch you
 *   broke). Solder a thin wire from that pad to the ESP32 GPIO chosen below.
 *
 *   RESET (RUN) needs NO wire — it's already on the IO expander (pin 8),
 *   which bsp_board_init() set up for us.
 *
 * Mechanism:
 *   1. Drive the BOOTSEL GPIO LOW (simulates holding the BOOTSEL button).
 *   2. Pull RUN (IO expander pin 8) LOW  -> RP2040 held in reset.
 *   3. Wait ~100 ms.
 *   4. Release RUN (HIGH) while BOOTSEL is still LOW -> RP2040 comes out of
 *      reset, sees BOOTSEL=low, jumps to the ROM USB bootloader.
 *   5. Wait ~50 ms, then release BOOTSEL GPIO (high-impedance). The
 *      bootloader has already latched the decision, so this is safe.
 *
 * Result: a "RPI-RP2" USB drive appears on the host PC within ~1 s.
 */

#include "rp2040_boot.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "bsp_board.h"

static const char *TAG = "rp2040_boot";

/* IO-expander pin that drives the RP2040 RUN/RESET line (already wired). */
#define EXPANDER_IO_RP2040_RESET   8

/* Free ESP32-S3 GPIO that YOU wire to the RP2040 BOOTSEL pad.
 * GPIO 42 is free on this board (not on the LCD/I2C/SPI/audio buses).
 * If you soldered to a different pin, change this. */
#define RP2040_BOOTSEL_GPIO        42

bool rp2040_force_bootloader(void)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if (!brd || !brd->io_expander_ops || !brd->FUNC_IO_EXPANDER_EN) {
        ESP_LOGE(TAG, "IO expander not available — can't reset RP2040");
        return false;
    }

    /* Configure the BOOTSEL GPIO as output, start LOW (button "pressed"). */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << RP2040_BOOTSEL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG, "failed to config BOOTSEL GPIO %d", RP2040_BOOTSEL_GPIO);
        return false;
    }
    gpio_set_level(RP2040_BOOTSEL_GPIO, 0);   /* hold BOOTSEL low */
    ESP_LOGI(TAG, "BOOTSEL held LOW on GPIO %d", RP2040_BOOTSEL_GPIO);

    /* Hold RP2040 in reset (RUN low) via the IO expander. */
    brd->io_expander_ops->set_level(EXPANDER_IO_RP2040_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Release reset (RUN high) while BOOTSEL is still low -> bootloader. */
    brd->io_expander_ops->set_level(EXPANDER_IO_RP2040_RESET, 1);
    ESP_LOGI(TAG, "RP2040 RUN released — bootloader should start");

    vTaskDelay(pdMS_TO_TICKS(50));

    /* Release BOOTSEL (high-Z). Bootloader decision already latched. */
    gpio_set_level(RP2040_BOOTSEL_GPIO, 1);
    gpio_config_t io_in = {
        .pin_bit_mask = (1ULL << RP2040_BOOTSEL_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_in);

    ESP_LOGI(TAG, "Done. Look for a new 'RPI-RP2' USB drive on the PC, "
                  "then drag sense_pong_rp2040.uf2 onto it.");
    return true;
}
