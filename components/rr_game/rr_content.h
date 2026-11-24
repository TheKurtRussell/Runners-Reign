/*
 * Runner's Reign — content model (data-driven)
 *
 * Rooms, hotspots, NPCs and keyword dialogue are plain data. Building the game
 * means authoring these tables; the engine (rr_game.c) interprets them.
 */
#ifndef RR_CONTENT_H
#define RR_CONTENT_H

#include <stdint.h>
#include <stdbool.h>

/* Items the player can hold */
enum { ITEM_KEYCARD, ITEM_PAYDATA, RR_N_ITEMS };

/* Flags: keywords learned + world state */
enum {
    FLAG_KEYCARD_TAKEN,
    FLAG_PAYDATA,        /* pulled the paydata from the server */
    FLAG_ALARM,          /* spoof failed; the floor is hot */
    FLAG_KW_SETUP,       /* learned the run was a setup */
    FLAG_KW_VEX,         /* knows the fixer's name */
    RR_N_FLAGS
};

/* Skills (index into state.skill[]) */
enum { SK_FIREARMS, SK_COMPUTER, SK_MAGIC, SK_NEGOTIATION, RR_N_SKILLS };

/* Rooms */
enum { ROOM_OFFICE, ROOM_STREET, RR_N_ROOMS };

typedef enum {
    ACT_LOOK,    /* show text */
    ACT_TAKE,    /* grant give_item, set set_flag, then hide */
    ACT_CHECK,   /* roll skill[arg] + 2d6 vs difficulty */
    ACT_GOTO,    /* travel to room arg (needs need_flag) */
    ACT_TALK,    /* open dialogue with npc arg */
    ACT_END,     /* end-of-slice beat */
} rr_act_t;

typedef struct {
    const char *name;
    int         x, y, w, h;   /* tap area in 1280x720 space */
    rr_act_t    act;
    int         arg;          /* item / room / npc / skill index */
    int         difficulty;   /* CHECK target */
    int         give_item;    /* -1 none: granted on TAKE / CHECK success */
    int         set_flag;     /* -1 none: set on action / CHECK success */
    int         fail_flag;    /* -1 none: set on CHECK failure */
    int         need_flag;    /* -1 none: required, else `fail` text shown */
    int         hide_flag;    /* -1 none: hotspot hidden when this flag is set */
    const char *text;         /* look / success text */
    const char *fail;         /* CHECK-fail or locked text */
} rr_hotspot_t;

typedef struct {
    const char     *name;
    const char     *desc;
    uint32_t        bg;
    uint32_t        accent;
    const rr_hotspot_t *spots;
    int             n_spots;
} rr_room_t;

typedef struct {
    const char *topic;      /* keyword / button label */
    const char *response;
    int         need_flag;  /* -1 always available */
    int         teach_flag; /* -1 none: learned when asked */
} rr_topic_t;

typedef struct {
    const char       *name;
    const char       *greeting;
    const rr_topic_t *topics;
    int               n_topics;
} rr_npc_t;

extern const rr_room_t rr_rooms[RR_N_ROOMS];
extern const rr_npc_t  rr_npcs[];
extern const char     *rr_item_names[RR_N_ITEMS];
extern const char     *rr_skill_names[RR_N_SKILLS];

#endif /* RR_CONTENT_H */
