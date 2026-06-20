# Minimal CMake for the RP2040 sense-pong joystick firmware.
# Requires the Raspberry Pi Pico SDK:
#   cmake -DPICO_SDK_PATH=C:/pico-sdk ..

cmake_minimum_required(VERSION 3.13)

# Pull in the pico-sdk (must initialize its submodules first).
include(pico_sdk_import.cmake)

project(sense_pong_rp2040 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# We don't have a Windows host compiler available (only the arm-none-eabi
# cross-compiler and ESP-IDF's esp-clang, neither of which can build picotool).
# Skip picotool entirely; we still get .elf + .bin, and convert .bin -> .uf2
# with the included Python script (uf2conv.py).
set(PICO_NO_PICOTOOL 1 CACHE BOOL "Disable picotool" FORCE)

pico_sdk_init()

add_executable(sense_pong_rp2040
    main.c
    cobs.c
)

target_include_directories(sense_pong_rp2040 PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# We only need: UART (uart1), ADC, GPIO, stdlib. No USB stdio, no fancy libs.
target_link_libraries(sense_pong_rp2040
    pico_stdlib
    hardware_uart
    hardware_adc
)

# Produce a UF2 for drag-drop flashing via BOOTSEL.
pico_enable_stdio_usb(sense_pong_rp2040 0)
pico_enable_stdio_uart(sense_pong_rp2040 0)
pico_add_extra_outputs(sense_pong_rp2040)
