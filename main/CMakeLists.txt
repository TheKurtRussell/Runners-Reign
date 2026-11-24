idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES rr_board rr_game nvs_flash
)
