/*
 * Runner's Reign — engine (LVGL) for the Tab5.
 *
 * Title screen -> room exploration with tappable hotspots -> skill checks,
 * item pickups, keyword dialogue, and character/inventory screens, all driven by
 * the data tables in rr_content.c. Cyberpunk-kawaii palette.
 *
 * Threading: rr_game_init() runs outside the LVGL task, so it locks the display.
 * Everything else runs from LVGL event / async / are already on the LVGL task.
 */
#include "rr_game.h"
#include "rr_content.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "rr_iso.h"

#define TAG "rr_game"

/* ---- palette ---- */
#define C_BG     0x0C0A17
#define C_PANEL  0x1A1333
#define C_NODE   0x160F2E
#define C_NEON   0xFF49D1   /* magenta */
#define C_CYAN   0x56E0FF
#define C_MINT   0x7CF5C2
#define C_TXT    0xEDE6FF
#define C_DIM    0x9186C2

/* ---- player state ---- */
typedef struct {
    int  attr[5];                 /* body, str, cha, int, wil */
    int  skill[RR_N_SKILLS];
    int  karma, nuyen, hp, hp_max;
    bool item[RR_N_ITEMS];
    bool flag[RR_N_FLAGS];
    int  room;
} rr_state_t;

static const char *k_attr_names[5] = { "Body", "Strength", "Charisma",
                                       "Intelligence", "Willpower" };

static rr_state_t g;

/* persistent UI */
static lv_obj_t *s_canvas;       /* painted scene (full-screen pixel buffer) */
static void     *s_canvas_buf;   /* PSRAM backing store for the canvas */
static lv_obj_t *s_zones;        /* transparent touch zones over the art */
static lv_obj_t *s_hud;          /* top bar + message panel + char/inv buttons */
static lv_obj_t *s_loc;
static lv_obj_t *s_stat;
static lv_obj_t *s_msg;
static lv_obj_t *s_overlay;      /* dialogue / character / inventory; one at a time */

/* dialogue state */
static lv_obj_t *s_dlg_resp;
static lv_obj_t *s_dlg_btns;
static int       s_dlg_npc;

/* ---------------------------------------------------------------- helpers -- */
static void set_msg(const char *t) { if (s_msg) lv_label_set_text(s_msg, t); }

static void update_hud(void)
{
    if (s_loc)  lv_label_set_text(s_loc, rr_rooms[g.room].name);
    if (s_stat) {
        static char b[72];
        snprintf(b, sizeof(b), "NUYEN %d    HP %d/%d    KARMA %d",
                 g.nuyen, g.hp, g.hp_max, g.karma);
        lv_label_set_text(s_stat, b);
    }
}

static lv_obj_t *mk_label(lv_obj_t *par, const char *txt, uint32_t col,
                          const lv_font_t *font, int w)
{
    lv_obj_t *l = lv_label_create(par);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    if (w > 0) { lv_obj_set_width(l, w); lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP); }
    return l;
}

static lv_obj_t *mk_btn(lv_obj_t *par, const char *txt, int x, int y, int w, int h,
                        uint32_t border, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = lv_button_create(par);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_NODE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(C_TXT), 0);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

/* ---------------------------------------------------------- overlays ------- */
static void close_overlay(void)
{
    if (s_overlay) { lv_obj_delete(s_overlay); s_overlay = NULL; }
    s_dlg_resp = s_dlg_btns = NULL;
    update_hud();
}

static lv_obj_t *make_panel(const char *title, uint32_t accent)
{
    s_overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = mk_label(s_overlay, title, accent, &lv_font_montserrat_28, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 40, 28);
    return s_overlay;
}

/* --- character sheet --- */
static void char_close_cb(lv_event_t *e) { (void)e; close_overlay(); }

static void open_char(void)
{
    lv_obj_t *ov = make_panel("Character", C_MINT);
    char b[64];
    int y = 96;
    for (int i = 0; i < 5; i++) {
        snprintf(b, sizeof(b), "%-14s %d", k_attr_names[i], g.attr[i]);
        lv_obj_t *l = mk_label(ov, b, C_TXT, &lv_font_montserrat_22, 0);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 40, y); y += 40;
    }
    y = 96;
    for (int i = 0; i < RR_N_SKILLS; i++) {
        snprintf(b, sizeof(b), "%-14s %d", rr_skill_names[i], g.skill[i]);
        lv_obj_t *l = mk_label(ov, b, C_CYAN, &lv_font_montserrat_22, 0);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 420, y); y += 40;
    }
    snprintf(b, sizeof(b), "Karma %d     Nuyen %d     HP %d/%d",
             g.karma, g.nuyen, g.hp, g.hp_max);
    lv_obj_t *s = mk_label(ov, b, C_NEON, &lv_font_montserrat_22, 0);
    lv_obj_align(s, LV_ALIGN_TOP_LEFT, 40, 320);

    mk_btn(ov, LV_SYMBOL_LEFT "  back", 40, 600, 160, 56, C_NEON, char_close_cb, NULL);
}

/* --- inventory --- */
static void inv_close_cb(lv_event_t *e) { (void)e; close_overlay(); }

static void open_inv(void)
{
    lv_obj_t *ov = make_panel("Inventory", C_MINT);
    int y = 110, shown = 0;
    for (int i = 0; i < RR_N_ITEMS; i++) {
        if (!g.item[i]) continue;
        lv_obj_t *l = mk_label(ov, rr_item_names[i], C_TXT, &lv_font_montserrat_22, 0);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 48, y); y += 44; shown++;
    }
    if (!shown) {
        lv_obj_t *l = mk_label(ov, "(empty — you're running light)", C_DIM,
                               &lv_font_montserrat_22, 0);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 48, 110);
    }
    mk_btn(ov, LV_SYMBOL_LEFT "  back", 40, 600, 160, 56, C_NEON, inv_close_cb, NULL);
}

/* --- keyword dialogue --- */
static void dlg_leave_cb(lv_event_t *e) { (void)e; close_overlay(); }
static void dlg_build_topics(int npc);

static void dlg_refresh_async(void *p) { (void)p; if (s_overlay) dlg_build_topics(s_dlg_npc); }

static void dlg_topic_cb(lv_event_t *e)
{
    int ti = (int)(intptr_t)lv_event_get_user_data(e);
    const rr_npc_t *n = &rr_npcs[s_dlg_npc];
    if (ti < 0 || ti >= n->n_topics) return;
    const rr_topic_t *t = &n->topics[ti];
    if (s_dlg_resp) lv_label_set_text(s_dlg_resp, t->response);
    if (t->teach_flag >= 0 && !g.flag[t->teach_flag]) {
        g.flag[t->teach_flag] = true;
        lv_async_call(dlg_refresh_async, NULL);   /* reveal newly-unlocked topics */
    }
}

static void dlg_build_topics(int npc)
{
    if (!s_dlg_btns) return;
    lv_obj_clean(s_dlg_btns);
    const rr_npc_t *n = &rr_npcs[npc];
    int y = 0;
    for (int i = 0; i < n->n_topics; i++) {
        const rr_topic_t *t = &n->topics[i];
        if (t->need_flag >= 0 && !g.flag[t->need_flag]) continue;
        mk_btn(s_dlg_btns, t->topic, 0, y, 300, 56, C_CYAN,
               dlg_topic_cb, (void *)(intptr_t)i);
        y += 68;
    }
    mk_btn(s_dlg_btns, LV_SYMBOL_LEFT "  leave", 0, y + 10, 300, 56, C_NEON,
           dlg_leave_cb, NULL);
}

static void open_dialogue(int npc)
{
    s_dlg_npc = npc;
    const rr_npc_t *n = &rr_npcs[npc];
    lv_obj_t *ov = make_panel(n->name, C_NEON);

    s_dlg_resp = mk_label(ov, n->greeting, C_TXT, &lv_font_montserrat_22, 560);
    lv_obj_align(s_dlg_resp, LV_ALIGN_TOP_LEFT, 40, 110);

    s_dlg_btns = lv_obj_create(ov);
    lv_obj_remove_style_all(s_dlg_btns);
    lv_obj_set_size(s_dlg_btns, 320, 560);
    lv_obj_align(s_dlg_btns, LV_ALIGN_TOP_RIGHT, -40, 100);
    dlg_build_topics(npc);
}

/* ---------------------------------------------------------- room render ---- */
static void render_room(void);
static void render_async(void *p) { (void)p; render_room(); }

static void do_hotspot(int idx)
{
    const rr_room_t *r = &rr_rooms[g.room];
    if (idx < 0 || idx >= r->n_spots) return;
    const rr_hotspot_t *h = &r->spots[idx];

    switch (h->act) {
    case ACT_LOOK:
        set_msg(h->text);
        break;

    case ACT_TAKE:
        if (h->give_item >= 0) g.item[h->give_item] = true;
        if (h->set_flag  >= 0) g.flag[h->set_flag] = true;
        set_msg(h->text);
        lv_async_call(render_async, NULL);
        break;

    case ACT_CHECK: {
        int roll = (esp_random() % 6) + 1 + (esp_random() % 6) + 1 + g.skill[h->arg];
        if (roll >= h->difficulty) {
            if (h->give_item >= 0) g.item[h->give_item] = true;
            if (h->set_flag  >= 0) g.flag[h->set_flag] = true;
            g.karma += 1;
            set_msg(h->text);
        } else {
            if (h->fail_flag >= 0) g.flag[h->fail_flag] = true;
            set_msg(h->fail);
        }
        update_hud();
        lv_async_call(render_async, NULL);
        break;
    }

    case ACT_GOTO:
        if (h->need_flag < 0 || g.flag[h->need_flag]) {
            set_msg(h->text);
            g.room = h->arg;
            lv_async_call(render_async, NULL);
        } else {
            set_msg(h->fail);
        }
        break;

    case ACT_TALK:
        if (!s_overlay) open_dialogue(h->arg);
        break;

    case ACT_END:
        if (!s_overlay) {
            make_panel("Runner's Reign", C_NEON);
            lv_obj_t *l = mk_label(s_overlay, h->text, C_TXT, &lv_font_montserrat_22, 900);
            lv_obj_align(l, LV_ALIGN_CENTER, 0, -20);
            mk_btn(s_overlay, "back to the street", 40, 600, 260, 56, C_NEON,
                   (lv_event_cb_t)char_close_cb, NULL);
        }
        break;
    }
}

static void hotspot_cb(lv_event_t *e)
{
    if (s_overlay) return;
    do_hotspot((int)(intptr_t)lv_event_get_user_data(e));
}

static void hud_char_cb(lv_event_t *e) { (void)e; if (!s_overlay) open_char(); }
static void hud_inv_cb(lv_event_t *e)  { (void)e; if (!s_overlay) open_inv(); }

static void repaint_scene(void)
{
    if (!s_canvas || !s_canvas_buf) return;
    rr_iso_render((uint16_t *)s_canvas_buf, RR_ART_W, RR_ART_H, g.room);
    lv_obj_invalidate(s_canvas);
}

static void render_room(void)
{
    const rr_room_t *r = &rr_rooms[g.room];

    repaint_scene();

    /* rebuild the transparent touch zones laid over the painted scene */
    if (s_zones) lv_obj_delete(s_zones);
    s_zones = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_zones);
    lv_obj_set_size(s_zones, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_zones, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(s_zones, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_zones, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < r->n_spots; i++) {
        const rr_hotspot_t *h = &r->spots[i];
        if (h->hide_flag >= 0 && g.flag[h->hide_flag]) continue;

        /* place the touch zone over the sprite the hotspot maps to; fall back
         * to the hotspot's own coords for ambient spots with no iso object */
        int zx, zy, zw, zh;
        if (!rr_iso_hotspot_rect(g.room, i, &zx, &zy, &zw, &zh)) {
            zx = h->x; zy = h->y; zw = h->w; zh = h->h;
        }
        /* pad small sprites so characters/doors are comfortable to tap */
        if (zw < 120) { zx -= (120 - zw) / 2; zw = 120; }
        if (zh < 120) { zy -= (120 - zh) / 2; zh = 120; }

        lv_obj_t *z = lv_obj_create(s_zones);
        lv_obj_remove_style_all(z);
        lv_obj_set_pos(z, zx, zy);
        lv_obj_set_size(z, zw, zh);
        lv_obj_set_style_bg_opa(z, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(z, lv_color_hex(r->accent), 0);
        lv_obj_set_style_border_width(z, 2, 0);
        lv_obj_set_style_border_opa(z, 80, 0);          /* subtle "interactive" hint */
        lv_obj_set_style_radius(z, 10, 0);
        lv_obj_remove_flag(z, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(z, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(z, hotspot_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *cap = lv_label_create(s_zones);
        lv_label_set_text(cap, h->name);
        lv_obj_set_style_text_color(cap, lv_color_hex(r->accent), 0);
        lv_obj_set_style_text_font(cap, &lv_font_montserrat_16, 0);
        lv_obj_set_style_bg_color(cap, lv_color_hex(0x05030E), 0);
        lv_obj_set_style_bg_opa(cap, 160, 0);
        lv_obj_set_style_pad_hor(cap, 6, 0);
        lv_obj_set_style_pad_ver(cap, 2, 0);
        lv_obj_set_style_radius(cap, 4, 0);
        lv_obj_set_pos(cap, zx + 6, zy + zh - 26);
    }

    if (s_canvas) lv_obj_move_background(s_canvas);
    if (s_hud)    lv_obj_move_foreground(s_hud);

    update_hud();
    set_msg(r->desc);
}

/* ------------------------------------------------------------- start ------- */
static void build_hud(void)
{
    s_hud = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_hud);
    lv_obj_set_size(s_hud, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_hud, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(s_hud, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_hud, LV_OBJ_FLAG_CLICKABLE);

    /* top-bar readability strip behind the location + stats */
    lv_obj_t *bar = lv_obj_create(s_hud);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), 60);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x05030E), 0);
    lv_obj_set_style_bg_opa(bar, 150, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    s_loc = mk_label(s_hud, "", C_NEON, &lv_font_montserrat_22, 0);
    lv_obj_align(s_loc, LV_ALIGN_TOP_LEFT, 24, 16);

    s_stat = mk_label(s_hud, "", C_CYAN, &lv_font_montserrat_16, 0);
    lv_obj_align(s_stat, LV_ALIGN_TOP_RIGHT, -24, 20);

    /* message/narration panel across the bottom */
    lv_obj_t *panel = lv_obj_create(s_hud);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LV_PCT(70), 150);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_LEFT, 20, -16);
    lv_obj_set_style_bg_color(panel, lv_color_hex(C_PANEL), 0);
    lv_obj_set_style_bg_opa(panel, 220, 0);
    lv_obj_set_style_radius(panel, 14, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x2E2452), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_pad_all(panel, 16, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    s_msg = mk_label(panel, "", C_TXT, &lv_font_montserrat_16, 820);
    lv_obj_align(s_msg, LV_ALIGN_TOP_LEFT, 0, 0);

    mk_btn(s_hud, "CHAR",  980, 600, 130, 60, C_MINT, hud_char_cb, NULL);
    mk_btn(s_hud, "ITEMS", 1130, 600, 130, 60, C_MINT, hud_inv_cb, NULL);
}

static void start_game(void)
{
    lv_obj_clean(lv_screen_active());
    s_canvas = NULL; s_zones = NULL; s_hud = NULL; s_overlay = NULL;

    /* full-screen painted-scene canvas (buffer lives in PSRAM, allocated once) */
    if (!s_canvas_buf)
        s_canvas_buf = heap_caps_malloc(RR_ART_W * RR_ART_H * 2, MALLOC_CAP_SPIRAM);
    s_canvas = lv_canvas_create(lv_screen_active());
    lv_obj_set_pos(s_canvas, 0, 0);
    if (s_canvas_buf)
        lv_canvas_set_buffer(s_canvas, s_canvas_buf, RR_ART_W, RR_ART_H,
                             LV_COLOR_FORMAT_RGB565);

    /* fresh character */
    memset(&g, 0, sizeof(g));
    for (int i = 0; i < 5; i++) g.attr[i] = 3;
    g.skill[SK_FIREARMS] = 2; g.skill[SK_COMPUTER] = 4;
    g.skill[SK_MAGIC] = 0;    g.skill[SK_NEGOTIATION] = 3;
    g.karma = 0; g.nuyen = 200; g.hp = g.hp_max = 10;
    g.room = ROOM_OFFICE;

    build_hud();
    render_room();
}

static void start_async(void *p) { (void)p; start_game(); }

static void title_tap_cb(lv_event_t *e) { (void)e; lv_async_call(start_async, NULL); }

static void build_title(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clean(scr);

    lv_obj_t *t = mk_label(scr, "RUNNER'S REIGN", C_NEON, &lv_font_montserrat_28, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -80);

    lv_obj_t *sub = mk_label(scr, "a shadow's never clean", C_CYAN,
                             &lv_font_montserrat_22, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t *hint = mk_label(scr, LV_SYMBOL_PLAY "  tap to jack in", C_DIM,
                              &lv_font_montserrat_22, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 60);

    /* full-screen tap target */
    lv_obj_t *tap = lv_button_create(scr);
    lv_obj_remove_style_all(tap);
    lv_obj_set_size(tap, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(tap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tap, title_tap_cb, LV_EVENT_CLICKED, NULL);
}

void rr_game_init(void *lv_display)
{
    if (!lv_display) { ESP_LOGW(TAG, "no display; game is headless"); return; }
    if (!bsp_display_lock(1000)) { ESP_LOGE(TAG, "display lock failed"); return; }
    build_title();
    bsp_display_unlock();
    ESP_LOGI(TAG, "title up");
}
