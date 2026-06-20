#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ESP32-S3 UART link to the RP2040.
 *
 * Uses the SenseCAP Indicator's on-board ESP32<->RP2040 UART:
 *   UART2, TX=GPIO19, RX=GPIO20, 115200 baud, 8N1, COBS-framed.
 *
 * Starts a background FreeRTOS task that decodes incoming COBS packets and
 * caches the latest joystick Y. No wiring needed — this link is on the PCB.
 *
 * The RP2040 must be running the sense-pong RP2040 firmware (see rp2040/)
 * which streams PKT_TYPE_JOYSTICK_Y packets at ~100 Hz.
 */
void joystick_init(void);

/**
 * @brief Latest normalized joystick Y, -1.0 (down) .. +1.0 (up), 0.0 = center.
 *
 * Returns 0.0 if no packet has been received yet (e.g. RP2040 firmware not
 * flashed) so the paddle simply stays still — safe fallback.
 */
float joystick_get_y(void);

/**
 * @brief Whether at least one valid joystick packet has been received.
 *
 * Useful to know if the RP2040 firmware is actually running.
 */
bool joystick_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* JOYSTICK_H */
