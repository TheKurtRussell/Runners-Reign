/*
 * Runner's Reign — first playable slice (intro #3: caught mid-run)
 *
 * Cold open inside a corp floor: grab the paydata, spoof the maglock, bail to the
 * street, meet your fixer and learn you were set up. Original world/writing in the
 * style of the Genesis cyberpunk RPGs.
 */
#include "rr_content.h"

const char *rr_item_names[RR_N_ITEMS] = {
    "Maglock keycard",
    "Paydata chip",
};

const char *rr_skill_names[RR_N_SKILLS] = {
    "Firearms", "Computer", "Magic", "Negotiation",
};

/* ---- Room 1: the corp floor --------------------------------------------- */
static const rr_hotspot_t office_spots[] = {
    {
        .name = "drone's desk", .x = 120, .y = 300, .w = 230, .h = 150,
        .act = ACT_TAKE, .arg = 0, .difficulty = 0,
        .give_item = ITEM_KEYCARD, .set_flag = FLAG_KEYCARD_TAKEN,
        .fail_flag = -1, .need_flag = -1, .hide_flag = FLAG_KEYCARD_TAKEN,
        .text = "A wage-drone's desk, still warm. A maglock keycard sits "
                "forgotten by the dead terminal. You pocket it.",
        .fail = NULL,
    },
    {
        .name = "server rack", .x = 470, .y = 250, .w = 260, .h = 230,
        .act = ACT_CHECK, .arg = SK_COMPUTER, .difficulty = 7,
        .give_item = ITEM_PAYDATA, .set_flag = FLAG_PAYDATA,
        .fail_flag = FLAG_ALARM, .need_flag = -1, .hide_flag = FLAG_PAYDATA,
        .text = "You jack in and slide past the ICE like smoke. The paydata "
                "peels loose — the chip's yours. [+1 karma]",
        .fail = "Black ICE bites back and the floor lights blood-red. "
                "Somewhere a siren winds up. Try again — fast.",
    },
    {
        .name = "maglock door", .x = 860, .y = 240, .w = 210, .h = 260,
        .act = ACT_GOTO, .arg = ROOM_STREET, .difficulty = 0,
        .give_item = -1, .set_flag = -1, .fail_flag = -1,
        .need_flag = FLAG_PAYDATA, .hide_flag = -1,
        .text = "The maglock blinks green. Alarms howling behind you, you kick "
                "through onto the fire stair and down into the rain.",
        .fail = "The maglock's sealed. You didn't come forty floors up to leave "
                "empty-handed — get the paydata first.",
    },
};

/* ---- Room 2: the street -------------------------------------------------- */
static const rr_hotspot_t street_spots[] = {
    {
        .name = "Vex (fixer)", .x = 150, .y = 240, .w = 240, .h = 260,
        .act = ACT_TALK, .arg = 0, .difficulty = 0,
        .give_item = -1, .set_flag = -1, .fail_flag = -1,
        .need_flag = -1, .hide_flag = -1,
        .text = "Vex waits in the steam of a noodle stand, eyes everywhere.",
        .fail = NULL,
    },
    {
        .name = "service door", .x = 470, .y = 250, .w = 200, .h = 250,
        .act = ACT_LOOK, .arg = 0, .difficulty = 0,
        .give_item = -1, .set_flag = -1, .fail_flag = -1,
        .need_flag = -1, .hide_flag = -1,
        .text = "The service door sealed behind you. No going back up that way.",
        .fail = NULL,
    },
    {
        .name = "into the sprawl", .x = 850, .y = 260, .w = 240, .h = 240,
        .act = ACT_END, .arg = 0, .difficulty = 0,
        .give_item = -1, .set_flag = -1, .fail_flag = -1,
        .need_flag = -1, .hide_flag = -1,
        .text = "You pull your collar up and melt into the neon rain, the chip a "
                "cold weight against your chest.\n\n"
                "[ End of the first slice — more runs to come. ]",
        .fail = NULL,
    },
};

const rr_room_t rr_rooms[RR_N_ROOMS] = {
    [ROOM_OFFICE] = {
        .name = "Nakatomi Dynamics — 44th floor",
        .desc = "Cubicle rows in blackout. Your run went loud the moment you "
                "stepped off the lift. Grab what you came for and get gone.",
        .bg = 0x141024, .accent = 0xFF49D1,
        .spots = office_spots, .n_spots = sizeof(office_spots) / sizeof(office_spots[0]),
    },
    [ROOM_STREET] = {
        .name = "Rain-slick alley, Jackson St.",
        .desc = "Forty floors down, the sprawl swallows you. Steam, neon, and a "
                "fixer who looks like she already knows how the night went.",
        .bg = 0x0E1526, .accent = 0x56E0FF,
        .spots = street_spots, .n_spots = sizeof(street_spots) / sizeof(street_spots[0]),
    },
};

/* ---- NPC: Vex, your fixer ----------------------------------------------- */
static const rr_topic_t vex_topics[] = {
    { .topic = "the paydata",
      .response = "\"Good. That chip's worth more than your life — which is "
                  "exactly the problem now.\"",
      .need_flag = -1, .teach_flag = -1 },
    { .topic = "the job",
      .response = "\"The brief said cold floor, dead ICE. You walked into live "
                  "ICE and a hot alarm. Somebody wanted you taken in there.\" "
                  "Her jaw tightens. \"You got burned. A setup.\"",
      .need_flag = -1, .teach_flag = FLAG_KW_SETUP },
    { .topic = "setup",
      .response = "\"The run came to me through a cutout. I don't like being "
                  "used any more than you do.\" She flicks you a credstick. "
                  "\"Lie low. I'll dig up the name.\"",
      .need_flag = FLAG_KW_SETUP, .teach_flag = -1 },
    { .topic = "who are you",
      .response = "\"Name's Vex. I broker shadows in this sector. You're my "
                  "runner now — try not to flatline on me twice.\"",
      .need_flag = -1, .teach_flag = FLAG_KW_VEX },
};

const rr_npc_t rr_npcs[] = {
    { .name = "Vex",
      .greeting = "Vex leans out of the noodle steam. \"You're breathing. "
                  "Didn't take that bet. Tell me you pulled the paydata.\"",
      .topics = vex_topics, .n_topics = sizeof(vex_topics) / sizeof(vex_topics[0]) },
};
