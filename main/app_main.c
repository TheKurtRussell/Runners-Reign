/*
 * Runner's Reign — P4 application entry point
 *
 * Bring up the Tab5 display, then start the game on the LVGL display.
 */
#include "esp_log.h"
#include "nvs_flash.h"

#include "rr_board.h"
#include "rr_game.h"

#define TAG "runnersreign"

void app_main(void)
{
    ESP_LOGI(TAG, "Runner's Reign 0.1.0");

    esp_err_t nv = nvs_flash_init();
    if (nv == ESP_ERR_NVS_NO_FREE_PAGES || nv == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(rr_board_init());
    rr_game_init(rr_board_lvgl_display());

    ESP_LOGI(TAG, "jacked in");
}
