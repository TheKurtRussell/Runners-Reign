/*
 * Runner's Reign — Tab5 board support
 *
 * The Tab5 panel is a 1280x720 MIPI-DSI panel (portrait-native 720x1280) with a
 * GT911 touch controller. The m5stack_tab5 BSP brings up the panel, touch, the
 * display LDO rail and the LVGL port, returning an lv_display_t*.
 */
#include "rr_board.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

#define TAG "rr_board"

static lv_display_t *s_lv_disp;

esp_err_t rr_board_init(void)
{
    s_lv_disp = bsp_display_start();
    if (!s_lv_disp) {
        ESP_LOGE(TAG, "bsp_display_start() failed");
        return ESP_FAIL;
    }
    bsp_display_backlight_on();

    /* Landscape: panel is portrait-native, rotate 270 to sit upright held over
     * the battery. Flip to _90 if it comes up inverted. */
    if (bsp_display_lock(1000)) {
        lv_display_set_rotation(s_lv_disp, LV_DISPLAY_ROTATION_270);
        bsp_display_unlock();
    }
    ESP_LOGI(TAG, "display up (landscape 1280x720)");
    return ESP_OK;
}

void *rr_board_lvgl_display(void) { return s_lv_disp; }
