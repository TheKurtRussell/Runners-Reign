/*
 * Runner's Reign — isometric sprite renderer.
 *
 * Composites authored RGB565+A8 sprite tiles (baked into flash by
 * tools/gen_tiles.py) directly into a full-screen RGB565 framebuffer, with
 * diamond projection and back-to-front depth sorting. No runtime image decoder.
 */
#ifndef RR_ISO_H
#define RR_ISO_H

#include <stdint.h>
#include <stdbool.h>

/* Design resolution the renderer targets (landscape). */
#define RR_ART_W 1280
#define RR_ART_H 720

/* Fill `buf` (W x H, RGB565) with the background gradient, then composite the
 * floor and objects for `room`. */
void rr_iso_render(uint16_t *buf, int W, int H, int room);

/* Screen rect of the sprite bound to hotspot index `hs` in `room`.
 * Returns true and fills x/y/w/h when an iso object links to that hotspot. */
bool rr_iso_hotspot_rect(int room, int hs, int *x, int *y, int *w, int *h);

#endif /* RR_ISO_H */
