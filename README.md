# Runner's Reign

A cyberpunk **point-and-click adventure / RPG** for the **M5Stack Tab5**
(ESP32-P4), in the spirit of the Sega Genesis cyberpunk RPGs: explore locations
by touch, bank **keywords** from conversations, and grow your runner with
**karma**, skills and nuyen. Original world and writing.

Single-chip: the game runs entirely on the P4 (display + touch), so flashing is
just the one-click **web flasher** over USB-C — no SD card, no second chip.

## Play (the first slice)

A cold open, caught mid-run:
1. **The corp floor** — grab the **paydata** off the server (a Computer skill
   check), pocket a keycard, and spoof the **maglock** once you've got the chip.
2. **The street** — meet your fixer **Vex**; ask her about *the job* to learn you
   were **set up** (your first banked keyword), then slip into the sprawl.

Tap hotspots to interact; **CHAR** and **ITEMS** (bottom-right) open your
character sheet and inventory. Skill checks roll 2d6 + the relevant skill.

## Engine (data-driven)

Rooms, hotspots, NPCs and keyword dialogue are plain tables in
`components/rr_game/rr_content.c` — building the game is authoring content, not
rewriting the engine (`rr_game.c`). Art is styled LVGL for now, with a clean path
to swap in real backgrounds later.

## Status

| Piece | Status |
|---|---|
| Title screen + room exploration | **In** |
| Tappable hotspots, item pickup | **In** |
| Skill checks (2d6 + skill) | **In** |
| Keyword dialogue | **In** |
| Character sheet + inventory | **In** |
| Verb bar (look / use / talk) | next — single-action hotspots for now |
| Walk-to pathfinding, real art | later |
| Combat, decking mini-game, runs | later |
| Save/load to the FAT partition | later |

## Build / flash

GitHub Actions builds `merged-p4.bin` and publishes the web flasher to Pages.
Turn on **Settings → Pages → Source = GitHub Actions**, then open the Pages URL
in Chrome/Edge, plug the Tab5 in over USB-C, and click **Install**.

## Layout

- `main/` — entry point (display bring-up → game).
- `components/rr_board/` — Tab5 BSP display/touch bring-up.
- `components/rr_game/` — engine (`rr_game.c`) + content (`rr_content.c`).
- `webflasher/` — the Pages flasher.
