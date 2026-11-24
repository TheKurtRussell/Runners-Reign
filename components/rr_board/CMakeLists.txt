idf_component_register(
    SRCS "rr_board.c"
    INCLUDE_DIRS "."
    REQUIRES m5stack_tab5
)
# m5stack_tab5 (via main/idf_component.yml) provides bsp/esp-bsp.h and pulls in
# esp_lvgl_port + lvgl + the panel/touch drivers.
