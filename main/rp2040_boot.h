#ifndef RP2040_BOOT_H
#define RP2040_BOOT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Force the RP2040 into its USB UF2 bootloader (the "RPI-RP2" mass-storage
 * drive), entirely under software control from the ESP32-S3 — no BOOTSEL
 * button needed.
 *
 * How it works:
 *   The RP2040 enters its ROM USB bootloader ONLY if the BOOTSEL pin is held
 *   LOW at the moment its RUN (reset) line rises from low to high.
 *
 *   - RUN/RESET is already wired to the on-board IO expander (TCA9535 pin 8),
 *     so the ESP32 toggles it with no extra wiring.
 *   - BOOTSEL is NOT routed to anything the ESP32 can drive in the public
 *     SenseCAP Indicator design, so this function also drives a free ESP32
 *     GPIO (RP2040_BOOTSEL_GPIO below) that you must wire to the RP2040's
 *     BOOTSEL test pad. See rp2040_boot.c / project docs for the one wire.
 *
 * After calling this, the RP2040 reboots into the bootloader and a new
 * USB mass-storage drive "RPI-RP2" appears on the PC. Drag your .uf2 onto it.
 *
 * @return true if the reset sequence was issued (the drive should appear
 *         within ~1 second on the host PC).
 *         false if the board / IO expander isn't available.
 *
 * @note This NEVER returns control to the caller if it succeeds in the sense
 *       that the RP2040 is now in bootloader — but the ESP32 keeps running
 *       normally. You can call this repeatedly.
 */
bool rp2040_force_bootloader(void);

#ifdef __cplusplus
}
#endif

#endif /* RP2040_BOOT_H */
