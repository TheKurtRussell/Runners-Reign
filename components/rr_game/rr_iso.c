/*
 * Runner's Reign — isometric sprite renderer (implementation).
 */
#include "rr_iso.h"
#include "rr_tiles_index.h"
#include "rr_rooms_gen.h"

#include <stdlib.h>
#include <string.h>

#define HALF_W 64
#define HALF_H 32

/* Background gradient endpoints (match tools/gen_tiles.py preview). */
#define BG_TOP_R 16
#define BG_TOP_G 12
#define BG_TOP_B 30
#define BG_BOT_R 6
#define BG_BOT_G 5
#define BG_BOT_B 14

static inline uint16_t rgb565(int r, int g, int b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* Alpha-composite one RGB565 source over an RGB565 destination. */
static inline uint16_t blend565(uint16_t dst, uint16_t src, uint8_t a)
{
    if (a == 0)   return dst;
    if (a == 255) return src;
    uint32_t ia = 255u - a;
    uint32_t sr = (src >> 11) & 0x1F, sg = (src >> 5) & 0x3F, sb = src & 0x1F;
    uint32_t dr = (dst >> 11) & 0x1F, dg = (dst >> 5) & 0x3F, db = dst & 0x1F;
    uint32_t r = (sr * a + dr * ia) / 255;
    uint32_t g = (sg * a + dg * ia) / 255;
    uint32_t b = (sb * a + db * ia) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* Blit sprite `tile` (TILE_*) so its anchor lands at screen (bx,by). */
static void blit(uint16_t *buf, int W, int H, uint16_t tile, int bx, int by)
{
    if (tile == TILE_NONE || tile >= TILE_COUNT) return;
    const rr_sprite_t *s = &rr_sprites[tile - 1];
    int sw = s->w, sh = s->h;
    const uint8_t *rgb = rr_tiles_bin_start + s->off;        /* sw*sh * 2 bytes */
    const uint8_t *alpha = rgb + (size_t)sw * sh * 2;        /* sw*sh bytes */

    int ox = bx - s->ax;   /* top-left of sprite on screen */
    int oy = by - s->ay;

    for (int y = 0; y < sh; y++) {
        int dy = oy + y;
        if (dy < 0 || dy >= H) continue;
        const uint8_t *arow = alpha + (size_t)y * sw;
        const uint8_t *crow = rgb + (size_t)y * sw * 2;
        uint16_t *drow = buf + (size_t)dy * W;
        for (int x = 0; x < sw; x++) {
            uint8_t a = arow[x];
            if (a == 0) continue;
            int dx = ox + x;
            if (dx < 0 || dx >= W) continue;
            /* byte-wise read avoids unaligned 16-bit loads into flash */
            uint16_t src = (uint16_t)(crow[x * 2] | (crow[x * 2 + 1] << 8));
            drow[dx] = blend565(drow[dx], src, a);
        }
    }
}

static inline void project(const rr_iso_room_t *r, int i, int j, int *sx, int *sy)
{
    *sx = r->ox + (i - j) * HALF_W;
    *sy = r->oy + (i + j) * HALF_H;
}

/* Depth key: draw smaller (i+j) first (further back); tie-break by i. */
typedef struct { int key; int idx; } depth_t;
static int depth_cmp(const void *a, const void *b)
{
    const depth_t *x = a, *y = b;
    return x->key - y->key;
}

void rr_iso_render(uint16_t *buf, int W, int H, int room)
{
    if (room < 0 || (size_t)room >= sizeof(rr_iso_rooms) / sizeof(rr_iso_rooms[0]))
        return;
    const rr_iso_room_t *r = &rr_iso_rooms[room];

    /* background gradient */
    for (int y = 0; y < H; y++) {
        int t256 = (y * 256) / H;
        int rr = BG_TOP_R + ((BG_BOT_R - BG_TOP_R) * t256) / 256;
        int gg = BG_TOP_G + ((BG_BOT_G - BG_TOP_G) * t256) / 256;
        int bb = BG_TOP_B + ((BG_BOT_B - BG_TOP_B) * t256) / 256;
        uint16_t c = rgb565(rr, gg, bb);
        uint16_t *row = buf + (size_t)y * W;
        for (int x = 0; x < W; x++) row[x] = c;
    }

    /* floor, back-to-front */
    for (int sum = 0; sum <= (r->gw - 1) + (r->gh - 1); sum++) {
        for (int j = 0; j < r->gh; j++) {
            int i = sum - j;
            if (i < 0 || i >= r->gw) continue;
            uint16_t tile = r->floor[j * r->gw + i];
            if (tile == TILE_NONE) continue;
            int sx, sy;
            project(r, i, j, &sx, &sy);
            blit(buf, W, H, tile, sx, sy);
        }
    }

    /* objects (walls/props/characters), sorted by depth */
    #define RR_MAX_OBJS 128
    int n = r->n_objs;
    depth_t order[RR_MAX_OBJS];
    if (n > RR_MAX_OBJS) n = RR_MAX_OBJS;
    for (int k = 0; k < n; k++) {
        const rr_place_t *p = &r->objs[k];
        order[k].key = (p->i + p->j) * 64 + p->i;
        order[k].idx = k;
    }
    qsort(order, n, sizeof(depth_t), depth_cmp);
    for (int k = 0; k < n; k++) {
        const rr_place_t *p = &r->objs[order[k].idx];
        int sx, sy;
        project(r, p->i, p->j, &sx, &sy);
        blit(buf, W, H, p->tile, sx, sy);
    }
}

bool rr_iso_hotspot_rect(int room, int hs, int *x, int *y, int *w, int *h)
{
    if (room < 0 || (size_t)room >= sizeof(rr_iso_rooms) / sizeof(rr_iso_rooms[0]))
        return false;
    const rr_iso_room_t *r = &rr_iso_rooms[room];
    for (int k = 0; k < r->n_objs; k++) {
        const rr_place_t *p = &r->objs[k];
        if (p->hs != hs) continue;
        if (p->tile == TILE_NONE || p->tile >= TILE_COUNT) return false;
        const rr_sprite_t *s = &rr_sprites[p->tile - 1];
        int sx, sy;
        project(r, p->i, p->j, &sx, &sy);
        *x = sx - s->ax;
        *y = sy - s->ay;
        *w = s->w;
        *h = s->h;
        return true;
    }
    return false;
}
