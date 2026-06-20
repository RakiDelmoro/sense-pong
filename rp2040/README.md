# sense-pong RP2040 firmware

Firmware for the RP2040 on the SenseCAP Indicator. Reads the Grove ADC
joystick (KY-023 on Grove ADC port pin A1 = GP27) and sends the normalized
Y axis to the ESP32-S3 over UART using the SenseCAP COBS protocol.

Built with the Raspberry Pi Pico SDK in plain C — same `cobs.c` the ESP32
side uses, no Arduino / no PacketSerial dependency.

## Wiring (already done on your side)

```
KY-023 joystick        Grove ADC port (RP2040)        ESP32-S3 (via on-board UART)
  GND    ────── Pin 1 (black)  GND              ─── common ───   GND
  +5V    ────── Pin 2 (red)    VCC 3.3V
  VRy    ────── Pin 4 (yellow) GP27 (ADC A1)
                       (Grove port has 4 pins; only 3 used: GND/VCC/VRy)
```

The RP2040 → ESP32-S3 UART is an internal on-board link, **not** the Grove
port. The Grove ADC port only carries the joystick signal *to the RP2040*:

```
RP2040  Serial1  TX (GP16) ──> ESP32 GPIO 20 (RX)   [on-board, no wiring needed]
RP2040  Serial1  RX (GP17) <- ESP32 GPIO 19 (TX)   [on-board, no wiring needed]
                          115200 baud, 8N1, COBS-framed
```

You do NOT wire anything between the RP2040 and ESP32 — that link is on the
PCB already. You only wire the joystick to the Grove ADC port (done).

## Protocol (shared with ESP32 side)

COBS-framed packets over UART at 115200 8N1. Packet delimiter = 0x00 byte.

Payload (5 bytes):
    [0]    type   = 0xC0 (PKT_TYPE_JOYSTICK_Y)
    [1..4] float  = normalized Y, -1.0 (full down) .. +1.0 (full up), 0.0 = center

Sent at ~100 Hz (every 10 ms).

## Build (one-time toolchain setup on Windows host)

Install the ARM toolchain + pico-sdk. See Seeed's wiki:
  https://wiki.seeedstudio.com/SenseCAP_Indicator_How_To_Flash_The_Default_Firmware/#rp2040-development-tool

Quick version:

```powershell
# ARM toolchain (arm-gnu for Windows, from developer.arm.com)
# pico-sdk
git clone https://github.com/raspberrypi/pico-sdk C:\pico-sdk
# tinyusb etc. are submodules — init them:
cd C:\pico-sdk
git submodule update --init --recursive
```

## Build + flash

We skip picotool (it needs a Windows host compiler we don't have) — the
build produces `.elf` + `.bin`, and a tiny Python script converts `.bin` to
`.uf2`.

```powershell
# Use the ESP-IDF PowerShell so cmake/ninja/python are on PATH, then add the
# ARM compiler to that session:
& 'C:\Espressif\tools\Microsoft.v5.1.6.PowerShell_profile.ps1'
$env:Path += ";C:\Program Files\Arm\GNU Toolchain mingw-w64-x86_64-arm-none-eabi\bin"

cd C:\Users\Raki\sense-pong\rp2040
# start fresh if you configured before the PICO_NO_PICOTOOL change:
Remove-Item -Recurse -Force build   # only needed once
mkdir build
cd build
cmake -G Ninja -DPICO_SDK_PATH=C:\pico-sdk ..
ninja
# -> produces sense_pong_rp2040.elf and sense_pong_rp2040.bin

# convert bin -> uf2 (pure Python, no host compiler needed):
python ..\uf2conv.py sense_pong_rp2040.bin
# -> produces sense_pong_rp2040.uf2
```

Flash: hold BOOTSEL (or trigger RP2040 reset via the on-board path — see wiki),
plug USB-C so the `RPI-RP2` drive appears, then copy `sense_pong_rp2040.uf2`
onto that drive. It reboots and runs.

The RP2040 ADC reads GP27 (your VRy) and streams the value to the ESP32-S3.
The ESP32-S3 (our `main/joystick.c`) parses it and moves the left Pong paddle.

## Flashing — two methods

### Method 1 (one-time, no wire): short the BOOTSEL pads

The RP2040's BOOTSEL button is an internal tactile switch reached through a
pinhole. If that's broken, open the case and **short the two pads of the
BOOTSEL button** (or the RP2040's BOOT test pad to GND) with tweezers while
plugging in USB-C. `RPI-RP2` appears → drag the .uf2 onto it.

This is the easiest first flash — pure hardware, no ESP32 involvement, order
vs. the ESP32 firmware doesn't matter.

### Method 2 (Option C, repeatable, no button): ESP32 forces bootloader

After soldering **one wire** from the RP2040 BOOTSEL pad to ESP32 GPIO 42
(see `main/rp2040_boot.c`), you can force the RP2040 into bootloader mode
from the Pong UI — no button, repeatable forever:

1. Flash + run the ESP32-S3 firmware (Pong).
2. On the welcome screen, tap **FLASH RP2040**.
3. The ESP32 holds BOOTSEL low + pulses the RP2040 reset (via the on-board
   IO expander). Within ~1 s a `RPI-RP2` drive appears on the PC.
4. Drag `sense_pong_rp2040.uf2` onto it.

Requires the ESP32 firmware to be running first (it does the forcing).
