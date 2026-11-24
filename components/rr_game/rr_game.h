/*
 * Runner's Reign — game entry point.
 */
#ifndef RR_GAME_H
#define RR_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the game on the given LVGL display (lv_display_t* as void*). */
void rr_game_init(void *lv_display);

#ifdef __cplusplus
}
#endif
#endif /* RR_GAME_H */
