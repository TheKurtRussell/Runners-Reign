/*
 * Runner's Reign — scene painters (LVGL canvas layer drawing).
 */
#include "rr_scenes.h"
#include "rr_content.h"

/* palette (kept in sync with rr_game.c) */
#define C_NEON   0xFF49D1
#define C_CYAN   0x56E0FF
#define C_MINT   0x7CF5C2
#define C_AMBER  0xFFC24F

/* ---------------------------------------------------------- primitives ----- */
static void rect(lv_layer_t *L, int x, int y, int w, int h, uint32_t col,
                 lv_opa_t opa, int radius)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(col);
    d.bg_opa   = opa;
    d.radius   = radius;
    lv_area_t a = { x, y, x + w - 1, y + h - 1 };
    lv_draw_rect(L, &d, &a);
}

static void vgrad(lv_layer_t *L, int x, int y, int w, int h, uint32_t top, uint32_t bot)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_COVER;
    d.bg_grad.dir = LV_GRAD_DIR_VER;
    d.bg_grad.stops_count = 2;
    d.bg_grad.stops[0].color = lv_color_hex(top);
    d.bg_grad.stops[0].opa   = LV_OPA_COVER;
    d.bg_grad.stops[0].frac  = 0;
    d.bg_grad.stops[1].color = lv_color_hex(bot);
    d.bg_grad.stops[1].opa   = LV_OPA_COVER;
    d.bg_grad.stops[1].frac  = 255;
    lv_area_t a = { x, y, x + w - 1, y + h - 1 };
    lv_draw_rect(L, &d, &a);
}

static void line(lv_layer_t *L, int x1, int y1, int x2, int y2, uint32_t col,
                 int wdt, lv_opa_t opa)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = lv_color_hex(col);
    d.width = wdt;
    d.opa   = opa;
    d.round_start = d.round_end = true;
    d.p1.x = x1; d.p1.y = y1;
    d.p2.x = x2; d.p2.y = y2;
    lv_draw_line(L, &d);
}

static void disc(lv_layer_t *L, int cx, int cy, int r, uint32_t col, lv_opa_t opa)
{
    rect(L, cx - r, cy - r, 2 * r, 2 * r, col, opa, LV_RADIUS_CIRCLE);
}

/* ------------------------------------------------------------- office ------ */
static void paint_office(lv_layer_t *L)
{
    vgrad(L, 0, 0, RR_ART_W, 400, 0x1A1540, 0x0E0A22);     /* back wall */
    vgrad(L, 0, 380, RR_ART_W, 340, 0x171133, 0x090616);   /* floor */

    /* perspective floor lines from a high vanishing point */
    for (int i = -7; i <= 7; i++)
        line(L, 640, 384, 640 + i * 150, 720, C_NEON, 1, 36);
    for (int j = 1; j <= 4; j++) {
        int y = 400 + j * j * 20;
        line(L, 0, y, RR_ART_W, y, C_CYAN, 1, 26);
    }
    line(L, 0, 384, RR_ART_W, 384, C_CYAN, 3, 150);        /* wall/floor seam */

    /* ceiling light strips */
    for (int i = 0; i < 4; i++)
        rect(L, 120 + i * 300, 40, 180, 8, C_CYAN, 70, 4);

    /* --- server rack (hotspot ~470,250,260,230) --- */
    rect(L, 470, 250, 260, 236, 0x0C0820, LV_OPA_COVER, 10);
    rect(L, 470, 250, 260, 236, C_CYAN, 40, 10);           /* rim glow */
    for (int r = 0; r < 6; r++) {
        rect(L, 486, 268 + r * 34, 228, 24, 0x141033, LV_OPA_COVER, 4);
        disc(L, 500, 280 + r * 34, 4, (r % 2) ? C_MINT : C_CYAN, LV_OPA_COVER);
        disc(L, 516, 280 + r * 34, 4, (r % 3) ? C_NEON : 0x44306A, LV_OPA_COVER);
    }

    /* --- drone's desk (hotspot ~120,300,230,150) --- */
    rect(L, 120, 330, 230, 120, 0x141028, LV_OPA_COVER, 8);
    rect(L, 150, 300, 150, 90, 0x0A0720, LV_OPA_COVER, 6); /* monitor */
    rect(L, 160, 310, 130, 64, C_CYAN, 55, 3);             /* dead-ish screen glow */

    /* --- maglock door (hotspot ~860,240,210,260) --- */
    rect(L, 860, 240, 210, 262, 0x0C0820, LV_OPA_COVER, 6);
    rect(L, 860, 240, 210, 262, C_NEON, 90, 6);            /* neon frame */
    line(L, 965, 250, 965, 492, 0x05030E, 4, LV_OPA_COVER);/* door seam */
    disc(L, 884, 370, 6, C_AMBER, LV_OPA_COVER);           /* lock light */
}

/* ------------------------------------------------------------- street ------ */
static const int bld[][3] = { /* x, top-y, width */
    {0, 120, 150}, {160, 70, 120}, {300, 150, 110}, {430, 40, 140},
    {590, 110, 120}, {730, 60, 130}, {880, 130, 120}, {1020, 80, 140}, {1180, 140, 100},
};

static void paint_street(lv_layer_t *L)
{
    vgrad(L, 0, 0, RR_ART_W, 400, 0x0B1026, 0x1A1B3E);     /* sky haze */
    /* building silhouettes + lit windows */
    for (int b = 0; b < (int)(sizeof(bld) / sizeof(bld[0])); b++) {
        int x = bld[b][0], ty = bld[b][1], w = bld[b][2];
        rect(L, x, ty, w, 400 - ty, 0x070912, LV_OPA_COVER, 0);
        for (int wy = ty + 16; wy < 380; wy += 28)
            for (int wx = x + 10; wx < x + w - 10; wx += 26)
                if (((wx * 7 + wy * 13) % 5) < 2)
                    rect(L, wx, wy, 10, 14, (wx % 3) ? C_CYAN : C_AMBER, 60, 2);
    }

    vgrad(L, 0, 380, RR_ART_W, 340, 0x121A30, 0x05070F);   /* wet street */
    line(L, 0, 384, RR_ART_W, 384, C_NEON, 2, 120);
    /* neon reflections streaking down the wet asphalt */
    for (int i = 0; i < 9; i++) {
        uint32_t c = (i % 2) ? C_NEON : C_CYAN;
        line(L, 120 + i * 130, 390, 120 + i * 130, 700, c, 6, 22);
    }
    /* rain */
    for (int i = 0; i < 46; i++) {
        int x = (i * 137) % RR_ART_W;
        int y = (i * 83) % 460;
        line(L, x, y, x - 10, y + 26, C_CYAN, 1, 40);
    }

    /* --- noodle stand + Vex (hotspot ~150,240,240,260) --- */
    rect(L, 150, 360, 250, 150, 0x0E0B22, LV_OPA_COVER, 8);  /* stall */
    rect(L, 140, 338, 270, 26, C_NEON, LV_OPA_COVER, 6);     /* awning */
    rect(L, 176, 300, 150, 40, 0x0A0720, LV_OPA_COVER, 4);   /* sign box */
    rect(L, 184, 308, 134, 24, C_AMBER, 80, 3);              /* sign glow */
    disc(L, 300, 420, 26, C_MINT, 30);                       /* steam pot glow */
    /* Vex, a rim-lit silhouette */
    disc(L, 250, 300, 24, 0x05030E, LV_OPA_COVER);           /* head */
    disc(L, 250, 300, 24, C_CYAN, 40);
    rect(L, 224, 322, 52, 150, 0x05030E, LV_OPA_COVER, 16);  /* torso/coat */
    line(L, 224, 322, 224, 470, C_CYAN, 2, 150);             /* rim light */
    line(L, 276, 322, 276, 470, C_NEON, 2, 110);

    /* --- service door (hotspot ~470,250,200,250) --- */
    rect(L, 470, 300, 200, 210, 0x0A0818, LV_OPA_COVER, 4);
    rect(L, 470, 300, 200, 210, 0x33264F, LV_OPA_COVER, 4);
    rect(L, 486, 316, 168, 178, 0x0A0818, LV_OPA_COVER, 2);

    /* --- alley exit (hotspot ~850,260,240,240): a glowing way out --- */
    vgrad(L, 860, 280, 220, 230, 0x241038, 0x05030E);
    rect(L, 860, 280, 220, 230, C_NEON, 60, 10);
    disc(L, 970, 470, 60, C_NEON, 24);                      /* far glow */
}

void rr_paint_room(lv_layer_t *layer, int room)
{
    switch (room) {
    case ROOM_OFFICE: paint_office(layer); break;
    case ROOM_STREET: paint_street(layer); break;
    default:          paint_office(layer); break;
    }
}
