/*
 * Runner's Reign — scene painters.
 *
 * Each room is drawn as an illustrated scene into an LVGL canvas layer (gradients,
 * perspective, neon, silhouettes) rather than boxes. Hotspot touch zones are laid
 * over the art by the engine.
 */
#ifndef RR_SCENES_H
#define RR_SCENES_H

#include "lvgl.h"

/* Design resolution the painters assume (landscape). */
#define RR_ART_W 1280
#define RR_ART_H 720

/* Paint `room` into the canvas layer. */
void rr_paint_room(lv_layer_t *layer, int room);

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
}
#endif
#endif /* RR_SCENES_H */
