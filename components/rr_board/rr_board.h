/*
 * Runner's Reign — M5Stack Tab5 board support (P4)
 *
 * Brings up the MIPI-DSI panel, GT911 touch, backlight and LVGL via the
 * m5stack_tab5 BSP, and hands back the LVGL display (rotated to landscape).
 */
#ifndef RR_BOARD_H
#define RR_BOARD_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rr_board_init(void);
void     *rr_board_lvgl_display(void);   /* lv_display_t* as void*; NULL on failure */

#ifdef __cplusplus
}
#endif
#endif /* RR_BOARD_H */
