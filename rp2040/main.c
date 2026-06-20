/**
 * sense-pong RP2040 firmware
 *
 * Reads the KY-023 joystick Y axis on the Grove ADC port (GP27 = ADC input 1)
 * and streams the normalized value to the ESP32-S3 over the on-board UART
 * link using the SenseCAP COBS protocol.
 *
 * Wiring summary (full details in README.md):
 *   - KY-023 VRy  -> Grove ADC pin A1 (GP27)
 *   - KY-023 VCC  -> Grove ADC VCC (3.3 V)
 *   - KY-023 GND  -> Grove ADC GND
 *   - RP2040 uart1 TX (GP16) -> ESP32 GPIO 20 (RX)   [on-board, pre-wired]
 *   - RP2040 uart1 RX (GP17) <- ESP32 GPIO 19 (TX)   [on-board, pre-wired]
 *
 * Protocol: COBS-framed 5-byte payloads [type][float32], 0x00 delimited, 115200 8N1.
 */

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/adc.h"
#include "cobs.h"

#define UART_ID         uart0       /* GP16/GP17 are UART0 pins on the RP2040 */
#define UART_BAUD       115200
#define UART_TX_PIN     16          /* RP2040 GP16 (UART0_TX) -> ESP32 GPIO20 RX */
#define UART_RX_PIN     17          /* RP2040 GP17 (UART0_RX) <- ESP32 GPIO19 TX */

/* ADC reference values (RP2040 ADC is 12-bit: 0..4095, center ~2048). */

/* Packet types: send BOTH Grove ADC pins so the ESP32 can see which one
 * is the actual joystick (the other will be floating/static). */
#define PKT_TYPE_JOYSTICK_A1  0xC0   /* GP27 (Grove pin 4 / white) */
#define PKT_TYPE_JOYSTICK_A0  0xC1   /* GP26 (Grove pin 3 / yellow) */

/* Deadband around electrical center so a still joystick reads 0.0.
 * RP2040 ADC is 12-bit (0..4095); ~2048 is center. Tune if needed. */
#define ADC_CENTER      2048
#define ADC_DEADBAND    60
#define ADC_MAX         4095.0f

static void send_packet(uint8_t type, float value)
{
    uint8_t raw[5];
    raw[0] = type;
    memcpy(&raw[1], &value, sizeof(float));

    uint8_t enc[16];
    cobs_encode_result r = cobs_encode(enc, sizeof(enc), raw, sizeof(raw));
    if (r.status != COBS_ENCODE_OK) {
        return;
    }

    uart_write_blocking(UART_ID, enc, r.out_len);
    uint8_t delim = 0x00;
    uart_write_blocking(UART_ID, &delim, 1);
}

static float normalize_adc(uint16_t raw)
{
    int32_t centered = (int32_t)raw - ADC_CENTER;
    if (centered > -ADC_DEADBAND && centered < ADC_DEADBAND) {
        centered = 0;
    }
    float y = (float)centered / (ADC_MAX / 2.0f);
    if (y > 1.0f)  y = 1.0f;
    if (y < -1.0f) y = -1.0f;
    return y;
}

int main(void)
{
    /* UART to the ESP32-S3 */
    uart_init(UART_ID, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    /* ADC for the joystick Y — init both Grove ADC pins (GP26 + GP27) */
    adc_init();
    adc_gpio_init(26);   /* GP26 = A0 (Grove pin 3 / yellow) */
    adc_gpio_init(27);   /* GP27 = A1 (Grove pin 4 / white) */

    while (true) {
        /* Read only the joystick channel (A0 / GP26 — the yellow Grove wire).
         * A1 (GP27) is unused now; sending it just doubled UART traffic and
         * added latency. */
        adc_select_input(0);             /* GP26 = A0 */
        uint16_t raw_a0 = adc_read();

        send_packet(PKT_TYPE_JOYSTICK_A0, normalize_adc(raw_a0));
        sleep_ms(10);          /* ~100 Hz */
    }

    return 0;
}
