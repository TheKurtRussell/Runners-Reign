#!/usr/bin/env python3
"""
Runner's Reign — isometric tileset generator.

Authors a cyberpunk isometric tileset (diamond floors, shaded wall cubes with
emissive windows, props and character sprites) entirely offline with Pillow, and
bakes it to a binary atlas the firmware embeds in flash. No runtime image decoder
needed: each sprite is RGB565 colour + 8-bit alpha, blitted directly.

Outputs (committed to the repo):
  components/rr_game/assets/rr_tiles.bin      concatenated RGB565 + A8 per sprite
  components/rr_game/rr_tiles_index.h         enum + metadata table + extern blob
  components/rr_game/rr_rooms_gen.h           room floor grids + object placements
  tools/preview_office.png / preview_street.png   visual check (not shipped)

The engine and this script share one projection, so the preview matches hardware.
To swap in a real CC0 tileset (e.g. Kenney), replace the builder bodies with a
sheet-slicer; the atlas format, index and rooms stay identical.
"""

import struct
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

# ----------------------------------------------------------------------------
# Projection (MUST match rr_iso.c)
# ----------------------------------------------------------------------------
TILE_W, TILE_H = 128, 64
HALF_W, HALF_H = TILE_W // 2, TILE_H // 2

# ----------------------------------------------------------------------------
# Palette (cyberpunk noir)
# ----------------------------------------------------------------------------
NEON_MAG = (255, 73, 209)
NEON_CYN = (86, 224, 255)
MINT     = (124, 245, 194)
AMBER    = (255, 176, 74)
REDLOCK  = (255, 80, 96)
GREENLK  = (96, 255, 150)

def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))

def shade(c, f):
    return tuple(max(0, min(255, int(c[i] * f))) for i in range(3))

# ----------------------------------------------------------------------------
# Low-level helpers
# ----------------------------------------------------------------------------
def new_img(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))

def add_noise(img, amt=10, seed=0):
    """Subtle luminance speckle on opaque pixels for texture."""
    rng = np.random.default_rng(seed)
    a = np.array(img).astype(np.int16)
    mask = a[:, :, 3] > 0
    n = rng.integers(-amt, amt + 1, size=a.shape[:2])
    for c in range(3):
        a[:, :, c] = np.where(mask, np.clip(a[:, :, c] + n, 0, 255), a[:, :, c])
    return Image.fromarray(a.astype(np.uint8), "RGBA")

def face_pt(corners, u, v):
    """Point inside a parallelogram face. corners = (TL, TR, BL)."""
    TL, TR, BL = corners
    x = TL[0] + u * (TR[0] - TL[0]) + v * (BL[0] - TL[0])
    y = TL[1] + u * (TR[1] - TL[1]) + v * (BL[1] - TL[1])
    return (x, y)

def face_quad(corners, u0, u1, v0, v1):
    return [face_pt(corners, u0, v0), face_pt(corners, u1, v0),
            face_pt(corners, u1, v1), face_pt(corners, u0, v1)]

# ----------------------------------------------------------------------------
# Floor tiles (128x64 diamond). anchor = diamond centre (64,32)
# ----------------------------------------------------------------------------
DIAMOND = [(64, 0), (128, 32), (64, 64), (0, 32)]

def floor_tile(base, variant=None, seed=1):
    img = new_img(TILE_W, TILE_H)
    d = ImageDraw.Draw(img)
    d.polygon(DIAMOND, fill=base + (255,))
    # vertical darkening toward the bottom point for a touch of depth
    for y in range(TILE_H):
        t = y / TILE_H
        col = shade(base, 1.0 - 0.18 * t)
        d.line([(0, y), (TILE_W, y)], fill=col + (0,))  # noop guard
    # redraw diamond with gradient via clip
    grad = new_img(TILE_W, TILE_H)
    gd = ImageDraw.Draw(grad)
    for y in range(TILE_H):
        t = y / TILE_H
        gd.line([(0, y), (TILE_W, y)], fill=shade(base, 1.08 - 0.30 * t) + (255,))
    mask = Image.new("L", (TILE_W, TILE_H), 0)
    ImageDraw.Draw(mask).polygon(DIAMOND, fill=255)
    img = Image.composite(grad, new_img(TILE_W, TILE_H), mask)

    img = add_noise(img, 8, seed)
    d = ImageDraw.Draw(img)
    # top edges highlight, bottom edges shadow
    hi = shade(base, 1.6)
    lo = shade(base, 0.55)
    d.line([(0, 32), (64, 0)], fill=hi + (255,), width=1)
    d.line([(64, 0), (128, 32)], fill=hi + (255,), width=1)
    d.line([(0, 32), (64, 64)], fill=lo + (255,), width=1)
    d.line([(64, 64), (128, 32)], fill=lo + (255,), width=1)

    if variant == "grate":
        for k in range(-2, 3):
            off = k * 10
            d.line([(32 + off, 16 + abs(k) * 0), (96 + off, 48)], fill=shade(base, 0.5) + (255,))
        for gx in range(16, 112, 12):
            d.line([(gx, 32), (gx + 16, 24)], fill=shade(base, 1.3) + (120,))
    elif variant == "neon":
        col = NEON_CYN
        d.line([(0, 32), (64, 64)], fill=col + (255,), width=2)
        d.line([(64, 64), (128, 32)], fill=col + (255,), width=2)
        glow = img.filter(ImageFilter.GaussianBlur(2))
        img = Image.alpha_composite(glow, img)
    elif variant == "puddle":
        # reflective cyan pool in the middle of the diamond
        pud = new_img(TILE_W, TILE_H)
        pd = ImageDraw.Draw(pud)
        small = [(64, 14), (108, 32), (64, 50), (20, 32)]
        for yy in range(14, 51):
            t = (yy - 14) / 36
            pd.line([(0, yy), (128, yy)], fill=lerp((18, 30, 44), NEON_CYN, 0.15 + 0.5 * t) + (255,))
        pm = Image.new("L", (TILE_W, TILE_H), 0)
        ImageDraw.Draw(pm).polygon(small, fill=255)
        img = Image.composite(pud, img, pm)
        d = ImageDraw.Draw(img)
        d.line([(30, 40), (60, 44)], fill=NEON_MAG + (160,), width=2)
        d.line([(70, 30), (96, 34)], fill=NEON_CYN + (180,), width=2)
    return img, 64, 32

# ----------------------------------------------------------------------------
# Wall cube / props with faces. anchor = base floor centre (64, 32+height)
# ----------------------------------------------------------------------------
def cube(height, base, top_f=1.25, left_f=0.62, right_f=0.92,
         windows=None, win_color=NEON_CYN, door=False, seed=2, width_scale=1.0):
    W = TILE_W
    H = TILE_H + height
    img = new_img(W, H)
    d = ImageDraw.Draw(img)

    top = [(64, 0), (128, 32), (64, 64), (0, 32)]
    left = [(0, 32), (64, 64), (64, 64 + height), (0, 32 + height)]
    right = [(64, 64), (128, 32), (128, 32 + height), (64, 64 + height)]

    d.polygon(left, fill=shade(base, left_f) + (255,))
    d.polygon(right, fill=shade(base, right_f) + (255,))
    d.polygon(top, fill=shade(base, top_f) + (255,))

    img = add_noise(img, 7, seed)
    d = ImageDraw.Draw(img)

    # crisp edges
    edge_hi = shade(base, 1.7)
    edge_lo = shade(base, 0.4)
    d.line([(0, 32), (64, 0)], fill=edge_hi + (255,))
    d.line([(64, 0), (128, 32)], fill=edge_hi + (255,))
    d.line([(64, 64), (64, 64 + height)], fill=edge_lo + (255,))  # inner vertical
    d.line([(0, 32), (0, 32 + height)], fill=edge_lo + (255,))
    d.line([(128, 32), (128, 32 + height)], fill=shade(base, 1.1) + (255,))

    L = ((0, 32), (64, 64), (0, 32 + height))   # TL,TR,BL of left face
    R = ((64, 64), (128, 32), (64, 64 + height)) # TL,TR,BL of right face

    if windows:
        cols, rows = windows
        for face, lit_bias in ((L, 0.0), (R, 0.15)):
            for r in range(rows):
                for c in range(cols):
                    u0 = (c + 0.22) / cols
                    u1 = (c + 0.78) / cols
                    v0 = (r + 0.18) / rows
                    v1 = (r + 0.74) / rows
                    lit = ((c * 7 + r * 3 + seed) % 10) / 10.0 > (0.45 - lit_bias)
                    if lit:
                        col = win_color
                        glow_q = face_quad(face, u0 - 0.04, u1 + 0.04, v0 - 0.05, v1 + 0.05)
                        g = new_img(W, H)
                        ImageDraw.Draw(g).polygon(glow_q, fill=col + (90,))
                        g = g.filter(ImageFilter.GaussianBlur(3))
                        img = Image.alpha_composite(img, g)
                        d = ImageDraw.Draw(img)
                        d.polygon(face_quad(face, u0, u1, v0, v1), fill=col + (255,))
                    else:
                        d.polygon(face_quad(face, u0, u1, v0, v1),
                                  fill=shade(base, 0.35) + (255,))

    if door:
        # bright maglock doorway on the right face
        frame = face_quad(R, 0.30, 0.70, 0.12, 0.98)
        d.polygon(frame, fill=(14, 16, 26, 255))
        inner = face_quad(R, 0.36, 0.64, 0.18, 0.95)
        g = new_img(W, H)
        ImageDraw.Draw(g).polygon(face_quad(R, 0.30, 0.70, 0.10, 1.0),
                                  fill=NEON_CYN + (80,))
        g = g.filter(ImageFilter.GaussianBlur(4))
        img = Image.alpha_composite(img, g)
        d = ImageDraw.Draw(img)
        d.polygon(inner, fill=shade(NEON_CYN, 0.5) + (255,))
        seam = [face_pt(R, 0.50, 0.18), face_pt(R, 0.50, 0.95)]
        d.line(seam, fill=NEON_CYN + (255,), width=2)
        lock = face_pt(R, 0.60, 0.30)
        d.ellipse([lock[0] - 3, lock[1] - 3, lock[0] + 3, lock[1] + 3],
                  fill=GREENLK + (255,))
    return img, 64, 32 + height

# ----------------------------------------------------------------------------
# Props
# ----------------------------------------------------------------------------
def server_rack(seed=3):
    h = 96
    img, ax, ay = cube(h, (30, 32, 46), windows=None, seed=seed)
    d = ImageDraw.Draw(img)
    R = ((64, 64), (128, 32), (64, 64 + h))
    # vent slats + blinking LED column on right face
    for r in range(8):
        v0 = 0.10 + r * 0.10
        d.polygon(face_quad(R, 0.18, 0.82, v0, v0 + 0.05),
                  fill=(18, 20, 30, 255))
    leds = [NEON_CYN, GREENLK, AMBER, NEON_MAG, GREENLK, NEON_CYN, AMBER, GREENLK]
    for r in range(8):
        p = face_pt(R, 0.30, 0.14 + r * 0.10)
        c = leds[r] if (r + seed) % 3 else shade(leds[r], 0.3)
        d.ellipse([p[0] - 2, p[1] - 2, p[0] + 2, p[1] + 2], fill=c + (255,))
    return img, ax, ay

def desk_terminal(seed=4):
    h = 34
    img, ax, ay = cube(h, (46, 40, 60), left_f=0.6, right_f=0.9, seed=seed)
    # monitor rising from the top, glowing screen
    W, H = img.size
    mon = new_img(W, H)
    md = ImageDraw.Draw(mon)
    top_c = (48, 60)  # near top-centre of the cube top
    scr = [(44, 2), (92, 14), (92, 40), (44, 28)]
    g = new_img(W, H)
    ImageDraw.Draw(g).polygon([(40, -2), (96, 12), (96, 44), (40, 32)], fill=NEON_CYN + (70,))
    g = g.filter(ImageFilter.GaussianBlur(4))
    img = Image.alpha_composite(img, g)
    d = ImageDraw.Draw(img)
    d.polygon(scr, fill=shade(NEON_CYN, 0.55) + (255,))
    for k in range(4):
        y = 8 + k * 6
        d.line([(48, y + 2), (88, y + 14)], fill=NEON_CYN + (220,))
    return img, ax, ay

def crate(seed=5):
    h = 40
    img, ax, ay = cube(h, (70, 58, 40), left_f=0.6, right_f=0.9, seed=seed)
    d = ImageDraw.Draw(img)
    L = ((0, 32), (64, 64), (0, 32 + h))
    R = ((64, 64), (128, 32), (64, 64 + h))
    for face in (L, R):
        d.line([face_pt(face, 0.1, 0.1), face_pt(face, 0.9, 0.9)], fill=(40, 32, 22, 255), width=2)
        d.line([face_pt(face, 0.9, 0.1), face_pt(face, 0.1, 0.9)], fill=(40, 32, 22, 255), width=2)
    d.polygon(face_quad(R, 0.3, 0.7, 0.35, 0.6), fill=AMBER + (120,))
    return img, ax, ay

def streetlamp(seed=6):
    h = 150
    W, Hh = 128, TILE_H + h
    img = new_img(W, Hh)
    d = ImageDraw.Draw(img)
    base_x = 64
    foot_y = 32 + h
    # pole
    d.line([(base_x, foot_y), (base_x, 40)], fill=(40, 44, 58, 255), width=5)
    d.line([(base_x - 1, foot_y), (base_x - 1, 40)], fill=(70, 76, 94, 255), width=1)
    # neon box light at top
    box = [(base_x - 22, 24), (base_x + 22, 24), (base_x + 22, 44), (base_x - 22, 44)]
    g = new_img(W, Hh)
    ImageDraw.Draw(g).polygon([(base_x - 30, 16), (base_x + 30, 16), (base_x + 30, 52), (base_x - 30, 52)],
                              fill=NEON_MAG + (90,))
    g = g.filter(ImageFilter.GaussianBlur(6))
    img = Image.alpha_composite(img, g)
    d = ImageDraw.Draw(img)
    d.polygon(box, fill=shade(NEON_MAG, 1.0) + (255,))
    d.polygon(box, outline=(255, 255, 255, 255))
    return img, 64, 32 + h

def neon_sign(seed=7):
    h = 110
    W, Hh = 128, TILE_H + h
    img = new_img(W, Hh)
    # floating sign panel high on a pole
    d = ImageDraw.Draw(img)
    d.line([(64, 32 + h), (64, 70)], fill=(40, 44, 58, 255), width=4)
    panel = [(20, 6), (108, 6), (108, 66), (20, 66)]
    g = new_img(W, Hh)
    ImageDraw.Draw(g).rectangle([12, -2, 116, 74], fill=NEON_CYN + (70,))
    g = g.filter(ImageFilter.GaussianBlur(7))
    img = Image.alpha_composite(img, g)
    d = ImageDraw.Draw(img)
    d.rectangle([20, 6, 108, 66], fill=(10, 12, 22, 255), outline=NEON_CYN + (255,), width=2)
    # abstract glyph blocks (not real text/IP)
    glyphs = [NEON_MAG, NEON_CYN, MINT, AMBER]
    gx = 30
    for k in range(4):
        c = glyphs[k]
        d.rectangle([gx, 18, gx + 6, 54], fill=c + (255,))
        d.rectangle([gx + 10, 18, gx + 16, 30], fill=c + (255,))
        gx += 22
    return img, 64, 32 + h

# ----------------------------------------------------------------------------
# Characters. anchor = feet centre
# ----------------------------------------------------------------------------
def runner(coat, trim, skin=(214, 170, 140), hair=(28, 28, 38), visor=None, seed=8):
    W, Hh = 56, 104
    img = new_img(W, Hh)
    d = ImageDraw.Draw(img)
    cx = 28
    feet_y = 98
    # ground shadow
    sh = new_img(W, Hh)
    ImageDraw.Draw(sh).ellipse([cx - 16, feet_y - 6, cx + 16, feet_y + 6], fill=(0, 0, 0, 120))
    sh = sh.filter(ImageFilter.GaussianBlur(2))
    img = Image.alpha_composite(sh, img)
    d = ImageDraw.Draw(img)
    # legs
    d.polygon([(cx - 9, 66), (cx - 2, 66), (cx - 3, 96), (cx - 10, 96)], fill=shade(coat, 0.45) + (255,))
    d.polygon([(cx + 2, 66), (cx + 9, 66), (cx + 10, 96), (cx + 3, 96)], fill=shade(coat, 0.55) + (255,))
    d.line([(cx + 9, 66), (cx + 10, 96)], fill=trim + (160,))
    # coat (trapezoid)
    coat_poly = [(cx - 13, 34), (cx + 13, 34), (cx + 17, 70), (cx - 17, 70)]
    d.polygon(coat_poly, fill=coat + (255,))
    # shaded (left) half
    d.polygon([(cx - 13, 34), (cx, 34), (cx, 70), (cx - 17, 70)], fill=shade(coat, 0.72) + (255,))
    # lit rim on right
    d.line([(cx + 13, 34), (cx + 17, 70)], fill=shade(coat, 1.6) + (255,), width=2)
    # collar + centre trim
    d.polygon([(cx - 8, 32), (cx + 8, 32), (cx + 5, 42), (cx - 5, 42)], fill=shade(coat, 1.2) + (255,))
    d.line([(cx, 40), (cx, 70)], fill=trim + (255,), width=2)
    # shoulders / arms
    d.ellipse([cx - 17, 34, cx - 8, 56], fill=shade(coat, 0.8) + (255,))
    d.ellipse([cx + 8, 34, cx + 17, 56], fill=shade(coat, 1.05) + (255,))
    # head
    d.ellipse([cx - 8, 12, cx + 8, 30], fill=skin + (255,))
    d.pieslice([cx - 9, 8, cx + 9, 28], 180, 360, fill=hair + (255,))
    d.line([(cx + 6, 14), (cx + 7, 26)], fill=lerp(skin, (255, 255, 255), 0.4) + (255,))  # rim
    if visor:
        d.line([(cx - 6, 20), (cx + 6, 20)], fill=visor + (255,), width=2)
    return img, cx, feet_y

# ----------------------------------------------------------------------------
# Sprite registry (order defines the enum / atlas order)
# ----------------------------------------------------------------------------
SPRITES = []   # (NAME, img, ax, ay)

def reg(name, triple):
    img, ax, ay = triple
    SPRITES.append((name, img, ax, ay))

reg("FLOOR_CONCRETE", floor_tile((42, 44, 60), None, 11))
reg("FLOOR_GRATE",    floor_tile((50, 54, 70), "grate", 12))
reg("FLOOR_NEON",     floor_tile((40, 42, 58), "neon", 13))
reg("FLOOR_ASPHALT",  floor_tile((30, 31, 40), None, 14))
reg("FLOOR_SIDEWALK", floor_tile((52, 52, 60), None, 15))
reg("FLOOR_PUDDLE",   floor_tile((26, 28, 40), "puddle", 16))

reg("WALL",        cube(120, (36, 38, 56), seed=21))
reg("WALL_WINDOW", cube(120, (34, 36, 54), windows=(3, 4), win_color=NEON_CYN, seed=22))
reg("WALL_WINDOW_M", cube(120, (40, 34, 52), windows=(3, 4), win_color=NEON_MAG, seed=23))
reg("WALL_DOOR",   cube(120, (38, 40, 58), door=True, seed=24))
reg("BUILDING",    cube(170, (28, 30, 48), windows=(3, 6), win_color=NEON_CYN, seed=25))
reg("BUILDING_M",  cube(170, (34, 28, 46), windows=(3, 6), win_color=NEON_MAG, seed=26))

reg("SERVER_RACK", server_rack(31))
reg("DESK",        desk_terminal(32))
reg("CRATE",       crate(33))
reg("STREETLAMP",  streetlamp(34))
reg("NEON_SIGN",   neon_sign(35))

reg("PLAYER", runner((36, 60, 96), NEON_CYN, visor=NEON_CYN, hair=(24, 26, 36), seed=41))
reg("VEX",    runner((70, 24, 56), NEON_MAG, hair=(60, 20, 48), skin=(206, 158, 150), seed=42))

NAMES = [s[0] for s in SPRITES]
IDX = {n: i + 1 for i, n in enumerate(NAMES)}   # 0 = NONE
IDX["NONE"] = 0

# ----------------------------------------------------------------------------
# Rooms (single source of truth for preview + firmware)
# ----------------------------------------------------------------------------
# object: (i, j, SPRITE, hotspot_index or -1)
OFFICE = {
    "ox": 630, "oy": 250, "gw": 7, "gh": 6,
    "floor_default": "FLOOR_CONCRETE",
    "floor_over": {  # (i,j): tile
        (3, 1): "FLOOR_GRATE", (4, 1): "FLOOR_GRATE",
        (0, 5): "FLOOR_NEON", (6, 5): "FLOOR_NEON",
    },
    "objs": [
        # back wall ring (i=-1 column, j=-1 row)
        *[(-1, j, "WALL_WINDOW", -1) for j in range(0, 6)],
        *[(i, -1, "WALL_WINDOW", -1) for i in range(0, 7) if i != 3],
        (3, -1, "WALL_DOOR", 2),          # maglock door -> hotspot 2
        (-1, -1, "WALL", -1),
        # props
        (1, 4, "DESK", 0),                # drone's desk -> hotspot 0
        (5, 1, "SERVER_RACK", 1),         # server rack -> hotspot 1
        (0, 2, "CRATE", -1),
        (5, 4, "CRATE", -1),
        (3, 3, "PLAYER", -1),             # the runner, mid-floor
    ],
}

STREET = {
    "ox": 630, "oy": 250, "gw": 8, "gh": 5,
    "floor_default": "FLOOR_ASPHALT",
    "floor_over": {
        (0, 0): "FLOOR_SIDEWALK", (1, 0): "FLOOR_SIDEWALK", (2, 0): "FLOOR_SIDEWALK",
        (0, 1): "FLOOR_SIDEWALK",
        (3, 3): "FLOOR_PUDDLE", (5, 2): "FLOOR_PUDDLE", (6, 4): "FLOOR_PUDDLE",
    },
    "objs": [
        # buildings across the back
        *[(i, -1, ("BUILDING_M" if i % 2 else "BUILDING"), -1) for i in range(0, 8)],
        *[(-1, j, "BUILDING", -1) for j in range(0, 5)],
        (-1, -1, "BUILDING", -1),
        (2, -1, "WALL_DOOR", 1),          # service door -> hotspot 1
        (7, 2, "NEON_SIGN", 2),           # into the sprawl -> hotspot 2 (alley sign)
        (6, 1, "STREETLAMP", -1),
        (1, 3, "STREETLAMP", -1),
        (0, 4, "CRATE", -1),
        (1, 1, "VEX", 0),                 # Vex -> hotspot 0
        (4, 3, "PLAYER", -1),
    ],
}

ROOMS = [("ROOM_OFFICE", OFFICE), ("ROOM_STREET", STREET)]

# ----------------------------------------------------------------------------
# Render a preview PNG (same projection as firmware)
# ----------------------------------------------------------------------------
def project(room, i, j):
    return (room["ox"] + (i - j) * HALF_W, room["oy"] + (i + j) * HALF_H)

def sprite_by_name(name):
    i = IDX[name]
    return SPRITES[i - 1]

def render_preview(room, bg_top=(16, 12, 30), bg_bot=(6, 5, 14)):
    W, H = 1280, 720
    img = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    for y in range(H):
        t = y / H
        ImageDraw.Draw(img).line([(0, y), (W, y)], fill=lerp(bg_top, bg_bot, t) + (255,))
    # floor
    cells = []
    for j in range(room["gh"]):
        for i in range(room["gw"]):
            name = room["floor_over"].get((i, j), room["floor_default"])
            cells.append((i + j, i, j, name))
    for _, i, j, name in sorted(cells):
        spr = sprite_by_name(name)
        sx, sy = project(room, i, j)
        img.alpha_composite(spr[1], (sx - spr[2], sy - spr[3]))
    # objects back-to-front
    objs = sorted(room["objs"], key=lambda o: (o[0] + o[1], o[1]))
    for (i, j, name, _hs) in objs:
        spr = sprite_by_name(name)
        sx, sy = project(room, i, j)
        img.alpha_composite(spr[1], (sx - spr[2], sy - spr[3]))
    return img

# ----------------------------------------------------------------------------
# Emit binary atlas + headers
# ----------------------------------------------------------------------------
def to_rgb565(img):
    a = np.array(img)
    r = (a[:, :, 0].astype(np.uint16) >> 3) << 11
    g = (a[:, :, 1].astype(np.uint16) >> 2) << 5
    b = (a[:, :, 2].astype(np.uint16) >> 3)
    rgb = (r | g | b).astype('<u2')
    alpha = a[:, :, 3].astype(np.uint8)
    return rgb.tobytes(), alpha.tobytes()

def emit():
    import os
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    comp = os.path.join(base, "components", "rr_game")
    assets = os.path.join(comp, "assets")
    os.makedirs(assets, exist_ok=True)

    blob = bytearray()
    meta = []  # (name, off, w, h, ax, ay)
    for name, img, ax, ay in SPRITES:
        w, h = img.size
        off = len(blob)
        rgb, alpha = to_rgb565(img)
        blob += rgb
        blob += alpha
        meta.append((name, off, w, h, ax, ay))

    with open(os.path.join(assets, "rr_tiles.bin"), "wb") as f:
        f.write(blob)

    # index header
    lines = ["/* GENERATED by tools/gen_tiles.py — do not edit by hand. */",
             "#ifndef RR_TILES_INDEX_H", "#define RR_TILES_INDEX_H",
             "#include <stdint.h>", "",
             "typedef struct { uint32_t off; uint16_t w, h; int16_t ax, ay; } rr_sprite_t;",
             "", "enum {", "    TILE_NONE = 0,"]
    for i, (name, *_1) in enumerate(meta):
        lines.append(f"    TILE_{name} = {i + 1},")
    lines.append(f"    TILE_COUNT = {len(meta) + 1}")
    lines.append("};")
    lines.append("")
    lines.append("/* indexed by TILE_* - 1 (TILE_NONE has no entry) */")
    lines.append(f"static const rr_sprite_t rr_sprites[{len(meta)}] = {{")
    for (name, off, w, h, ax, ay) in meta:
        lines.append(f"    {{ {off}, {w}, {h}, {ax}, {ay} }},  /* {name} */")
    lines.append("};")
    lines.append("")
    lines.append('extern const uint8_t rr_tiles_bin_start[] asm("_binary_rr_tiles_bin_start");')
    lines.append('extern const uint8_t rr_tiles_bin_end[]   asm("_binary_rr_tiles_bin_end");')
    lines.append("")
    lines.append("#endif")
    with open(os.path.join(comp, "rr_tiles_index.h"), "w") as f:
        f.write("\n".join(lines) + "\n")

    # rooms header
    rl = ["/* GENERATED by tools/gen_tiles.py — do not edit by hand. */",
          "#ifndef RR_ROOMS_GEN_H", "#define RR_ROOMS_GEN_H",
          "#include <stdint.h>", '#include "rr_tiles_index.h"', "",
          "typedef struct { int16_t i, j; uint16_t tile; int8_t hs; } rr_place_t;",
          "typedef struct {",
          "    int16_t ox, oy; uint8_t gw, gh;",
          "    const uint16_t *floor;        /* gw*gh, row-major j*gw+i */",
          "    const rr_place_t *objs; uint16_t n_objs;",
          "} rr_iso_room_t;", ""]
    for rname, room in ROOMS:
        key = rname.lower()
        # floor array
        fl = []
        for j in range(room["gh"]):
            for i in range(room["gw"]):
                nm = room["floor_over"].get((i, j), room["floor_default"])
                fl.append(f"TILE_{nm}")
        rl.append(f"static const uint16_t {key}_floor[{room['gw']*room['gh']}] = {{")
        # chunk per row
        for j in range(room["gh"]):
            row = ", ".join(fl[j*room['gw']:(j+1)*room['gw']])
            rl.append(f"    {row},")
        rl.append("};")
        # objects
        rl.append(f"static const rr_place_t {key}_objs[] = {{")
        for (i, j, name, hs) in room["objs"]:
            rl.append(f"    {{ {i}, {j}, TILE_{name}, {hs} }},")
        rl.append("};")
        rl.append("")
    rl.append("static const rr_iso_room_t rr_iso_rooms[] = {")
    for rname, room in ROOMS:
        key = rname.lower()
        rl.append(f"    {{ {room['ox']}, {room['oy']}, {room['gw']}, {room['gh']}, "
                  f"{key}_floor, {key}_objs, "
                  f"sizeof({key}_objs)/sizeof({key}_objs[0]) }},")
    rl.append("};")
    rl.append("")
    rl.append("#endif")
    with open(os.path.join(comp, "rr_rooms_gen.h"), "w") as f:
        f.write("\n".join(rl) + "\n")

    return len(blob), len(meta)


if __name__ == "__main__":
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    render_preview(OFFICE).convert("RGB").save(os.path.join(here, "preview_office.png"))
    render_preview(STREET).convert("RGB").save(os.path.join(here, "preview_street.png"))
    size, n = emit()
    print(f"sprites: {n}, atlas: {size} bytes ({size/1024:.1f} KiB)")
    print("previews: tools/preview_office.png, tools/preview_street.png")
