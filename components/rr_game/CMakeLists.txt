idf_component_register(
    SRCS "rr_game.c" "rr_content.c" "rr_scenes.c"
    INCLUDE_DIRS "."
    REQUIRES m5stack_tab5 esp_hw_support
)
# m5stack_tab5 pulls in lvgl + the BSP (bsp/esp-bsp.h); esp_hw_support provides
# esp_random.h for the skill-check dice.
