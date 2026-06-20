# sense-pong

> The SenseCAP Indicator **is** the Atari. A self-running Pong that boots
> straight onto the device's 4-inch round (480×480) touchscreen.

This is a stripped-down fork of the
[SenseCap-Indicator](https://github.com/RakiDelmoro/SenseCap-Indicator)
ESP-IDF project. All the Home-Assistant / MQTT / WiFi / sensor monitoring code
was removed. What's left is the minimum needed to drive the hardware:

- `components/bsp` — Board Support Package (LCD, touch, I²C, etc.)
- `components/lvgl` — LVGL 8.3 graphics library
- `components/bus`, `components/i2c_devices`, `components/iot_button` — drivers
- `main/lv_port.c` — LVGL ↔ LCD/touch glue (untouched from the source)
- `main/pong.c` / `pong.h` — the game
- `main/main.c` — boots the board and launches the game

## What it does right now (Phase 1)

Power the device on → it boots into Pong:

- Black playfield with a dashed center net
- Two **stationary** paddles (left & right), vertically centered
- A white ball that bounces off the top/bottom walls and the fixed paddles
- A score that increments when the ball escapes a side, then the ball resets
  to center

No player input yet — the paddles don't move. That's the next phase.

## Hardware

- ESP32-S3 (main CPU, drives the display)
- RP2040 (sensor helper — unused for now)
- 4-inch round RGB LCD, 480×480
- USB-C for power

## Develop / build / flash

**Editing** happens in this dev container (no ESP-IDF needed here — just a
text editor environment). **Building and flashing** happen on the Windows
host that has ESP-IDF v5.1.6 installed.

This project now has **two firmwares** (two chips on the device):

- `main/` — ESP32-S3 firmware (the Pong game + screen). Built with ESP-IDF.
- `rp2040/` — RP2040 firmware (reads the Grove joystick, streams it to the
  ESP32 over the on-board UART). Built with the pico-sdk. See `rp2040/README.md`.

The ESP32 firmware runs on its own (CPU-controlled right paddle) even before
the RP2040 firmware is flashed — the joystick just takes over automatically
once `joystick_is_connected()` goes true.

### On the Windows host (PowerShell)

```powershell
# 1. Activate the IDF environment
& 'C:\Espressif\tools\Microsoft.v5.1.6.PowerShell_profile.ps1'

# 2. cd into the project (sync this repo from the container first)
cd C:\Users\Raki\sense-pong

# 3. Build
idf.py build

# 4. Flash + monitor (adjust the port to your device)
idf.py -p COM3 flash monitor
```

If the COM port is unknown, run `idf.py -p (Get-CimInstance Win32_SerialPort).DeviceID flash`
or check Device Manager for the USB-C serial device.

### Set the target (only needed once / after a clean clone)

```powershell
idf.py set-target esp32s3
```

`sdkconfig.defaults` already pins the board config (ESP32-S3, PSRAM, 8 MB
flash, 480×480 LCD, LVGL direct-mode). `idf.py build` will pick it up.

## Project layout

```
sense-pong/
├── CMakeLists.txt          # project() = sense_pong
├── partitions.csv          # 4 MB factory app
├── sdkconfig               # board + LCD + LVGL config (from source)
├── sdkconfig.defaults      # same, in defaults form
├── components/             # hardware drivers (from source, unchanged)
│   ├── bsp/
│   ├── lvgl/
│   ├── bus/
│   ├── i2c_devices/
│   └── iot_button/
└── main/
    ├── CMakeLists.txt
    ├── main.c              # app_main: board init → LVGL → pong_start()
    ├── lv_port.c / .h      # LVGL display+touch porting (from source)
    └── pong.c / .h         # the game
```

## Next steps

- [x] Move the right paddle with the **KY-023 joystick** via the RP2040 Grove ADC port
- [ ] Wire the joystick button (SW) to START/PAUSE
- [ ] Add the RP2040 analog input as an alternative paddle control
- [ ] Sound via the on-board codec
- [ ] Start menu / difficulty
