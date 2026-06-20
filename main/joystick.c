/**
 * sense-pong ESP32-S3 joystick driver
 *
 * Receives COBS-framed joystick packets from the RP2040 over the on-board
 * UART link and exposes the latest Y axis to the Pong game.
 *
 * Mirrors the protocol used by Seeed's stock SenseCAP Indicator firmware:
 *   - UART2, 115200 8N1
 *   - TX = GPIO19, RX = GPIO20  (on-board, pre-wired to RP2040 GP17/GP16)
 *   - COBS framing, 0x00 = packet delimiter
 *   - Payload: [1-byte type][4-byte float]
 *
 * We handle two packet types from the RP2040: PKT_TYPE_JOYSTICK_A0 (GP26)
 * and PKT_TYPE_JOYSTICK_A1 (GP27), so we can see which Grove pin the
 * joystick is actually wired to.
 */

#include "joystick.h"

#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "cobs.h"

static const char *TAG = "joystick";

#define JK_UART_PORT        UART_NUM_2
#define JK_UART_TX          19          /* ESP32 -> RP2040 */
#define JK_UART_RX          20          /* ESP32 <- RP2040 */
#define JK_UART_BAUD        115200
#define JK_BUF_SIZE         512
#define JK_TASK_STACK       (4096)

#define PKT_TYPE_JOYSTICK_A1 0xC0   /* GP27 (Grove pin 4 / white) */
#define PKT_TYPE_JOYSTICK_A0 0xC1   /* GP26 (Grove pin 3 / yellow) */

/* Latest values + flags. Reads are single 32-bit words on ESP32-S3, no lock. */
static volatile float  g_y_a1      = 0.0f;   /* GP27 */
static volatile float  g_y_a0      = 0.0f;   /* GP26 */
static volatile bool   g_connected = false;
static volatile uint32_t g_pkt_count = 0;
static volatile uint32_t g_raw_bytes  = 0;

/* Decode one COBS packet payload (no framing bytes). */
static void handle_payload(const uint8_t *data, size_t len)
{
    if (len < 1) {
        return;
    }
    uint8_t type = data[0];
    if ((type == PKT_TYPE_JOYSTICK_A1 || type == PKT_TYPE_JOYSTICK_A0)
        && len >= 1 + sizeof(float)) {
        float v;
        memcpy(&v, &data[1], sizeof(float));
        if (!isnan(v)) {
            if (v > 1.0f)  v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            if (type == PKT_TYPE_JOYSTICK_A1) g_y_a1 = v;
            else                              g_y_a0 = v;
            g_pkt_count++;
            if (!g_connected) {
                g_connected = true;
                ESP_LOGI(TAG, "first packet! type=0x%02X y=%.3f (RP2040 talking)",
                         type, v);
            }
        }
    }
}

static void rx_task(void *arg)
{
    (void)arg;

    uart_config_t cfg = {
        .baud_rate  = JK_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(JK_UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(JK_UART_PORT, JK_UART_TX, JK_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(JK_UART_PORT, JK_BUF_SIZE * 2, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "UART%d @ %d baud (TX=%d RX=%d) listening for RP2040 joystick",
             JK_UART_PORT, JK_UART_BAUD, JK_UART_TX, JK_UART_RX);

    uint8_t buf[JK_BUF_SIZE];
    uint8_t decoded[64];

    while (1) {
        int len = uart_read_bytes(JK_UART_PORT, buf, JK_BUF_SIZE - 1,
                                  pdMS_TO_TICKS(5));
        if (len <= 0) {
            continue;
        }
        g_raw_bytes += len;

        /* Walk through the buffer packet by packet. Packets are delimited by
         * 0x00 bytes (COBS framing). Each non-zero run is one COBS frame. */
        uint8_t *p = buf;
        uint8_t *end = buf + len;
        while (p < end) {
            uint8_t *frame_end = p;
            while (frame_end < end && *frame_end != 0x00) {
                frame_end++;
            }
            if (frame_end > p) {
                cobs_decode_result r = cobs_decode(decoded, sizeof(decoded), p,
                                                   frame_end - p);
                if (r.status == COBS_DECODE_OK && r.out_len > 0) {
                    handle_payload(decoded, r.out_len);
                } else if (frame_end > p) {
                    ESP_LOGW(TAG, "COBS decode fail: status=%d len=%d first=0x%02X",
                             r.status, (int)(frame_end - p), p[0]);
                }
            }
            p = (frame_end < end) ? frame_end + 1 : end;
        }

        /* Periodic status every 2 s: are we getting bytes but no packets? */
        static uint32_t last_report = 0;
        uint32_t now = xTaskGetTickCount();
        if (now - last_report > pdMS_TO_TICKS(2000)) {
            last_report = now;
            ESP_LOGI(TAG, "status: bytes=%lu pkts=%lu conn=%d | A0(GP26)=%.3f A1(GP27)=%.3f",
                     (unsigned long)g_raw_bytes, (unsigned long)g_pkt_count,
                     (int)g_connected, (double)g_y_a0, (double)g_y_a1);
        }
    }
}

void joystick_init(void)
{
    xTaskCreate(rx_task, "joystick_rx", JK_TASK_STACK, NULL, 2, NULL);
}

float joystick_get_y(void)
{
    /* The KY-023 joystick on this board reports on A1 (GP27, Grove pin 4 /
     * white wire). A0 (GP26) is floating, so we ignore it. */
    return g_y_a1;
}

bool joystick_is_connected(void)
{
    return g_connected;
}
