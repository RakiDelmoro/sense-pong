idf_component_register(
    SRCS "main.c" "lv_port.c" "pong.c" "joystick.c" "cobs.c" "rp2040_boot.c"
    INCLUDE_DIRS "."
    REQUIRES bsp lvgl nvs_flash driver)
