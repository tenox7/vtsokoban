/* mktiles - package the Sokoban tile art for VT Sokoban's DRCS soft font.
 *
 * Sokoban has only a handful of tile types, so unlike VT City's streaming
 * font this set is tiny and lives permanently in the terminal's 94 DRCS
 * slots.  The art is hand-drawn 1-bit 16x16 pixel tiles (below), modelled on
 * the classic Thinking-Rabbit skin: cobblestone walls, wooden X-crates, a dot
 * for the storage goal and a little worker.
 *
 * Emits soko_tiles.h: raw 1-bit 16x16 bitmaps, a half-tile alias table
 * (identical 8x16 halves share one slot) and per-tile ASCII preview pairs for
 * headless -shot dumps -- the same layout vt_term.c consumes.
 *
 *   mktiles [-o soko_tiles.h] [-d demoprefix] [-p preview.pgm]
 *     -d  write demoprefix{420,340,320}.vt raw-escape demo boards
 *     -p  write a preview.pgm mosaic of every tile (host-side eyeballing)
 *
 * Host-side dev tool; output header is committed to git.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILEW 16
#define TILEH 16
#define NTILE 7
#define NHALF (NTILE * 2)
#define NSLOT 94

/* tile indices -- must match the TILE_* enum used by the game */
enum { T_FLOOR, T_WALL, T_GOAL, T_BOX, T_BOXGOAL, T_PLAYER, T_PLAYERGOAL };

/* 16x16 pixel art: any char other than '.' is an "on" (lit) pixel.  Rows are
 * exactly 16 columns; the loader checks. */
static const char *art[NTILE][TILEH] = {
    {                       /* plain floor: blank (goals are dots) */
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    },
    {                        /* cobblestone: offset brick courses */
        "#######.########",
        "#######.########",
        "#######.########",
        "................",
        "###.#######.####",
        "###.#######.####",
        "###.#######.####",
        "................",
        "#######.########",
        "#######.########",
        "#######.########",
        "................",
        "###.#######.####",
        "###.#######.####",
        "###.#######.####",
        "................",
    },
    {                        /* storage marker: a ring / target */
        "................",
        "................",
        "......####......",
        "....########....",
        "...##########...",
        "...###....###...",
        "..###......###..",
        "..###......###..",
        "..###......###..",
        "..###......###..",
        "...###....###...",
        "...##########...",
        "....########....",
        "......####......",
        "................",
        "................",
    },
    {                        /* wooden crate: inset frame + X braces */
        "................",
        ".##############.",
        ".##############.",
        ".####......####.",
        ".##.##....##.##.",
        ".##..##..##..##.",
        ".##...####...##.",
        ".##....##....##.",
        ".##....##....##.",
        ".##...####...##.",
        ".##..##..##..##.",
        ".##.##....##.##.",
        ".####......####.",
        ".##############.",
        ".##############.",
        "................",
    },
    {                        /* crate seated on goal: solid, carved X */
        "................",
        ".##############.",
        ".##############.",
        ".##..######..##.",
        ".###..####..###.",
        ".####..##..####.",
        ".#####....#####.",
        ".######..######.",
        ".######..######.",
        ".#####....#####.",
        ".####..##..####.",
        ".###..####..###.",
        ".##..######..##.",
        ".##############.",
        ".##############.",
        "................",
    },
    {                      /* worker: head, shoulders, split legs */
        "................",
        "......####......",
        ".....######.....",
        ".....######.....",
        "......####......",
        ".......##.......",
        "..############..",
        "..#####..#####..",
        "......####......",
        "......####......",
        "......####......",
        "......####......",
        "......#..#......",
        "......#..#......",
        ".....##..##.....",
        "................",
    },
    {                  /* worker framed: standing on the goal */
        "################",
        "#.....####.....#",
        "#....######....#",
        "#....######....#",
        "#.....####.....#",
        "#......##......#",
        "#.############.#",
        "#.#####..#####.#",
        "#.....####.....#",
        "#.....####.....#",
        "#.....####.....#",
        "#.....####.....#",
        "#.....#..#.....#",
        "#.....#..#.....#",
        "#....##..##....#",
        "################",
    },
};

static const char *prev[NTILE] = {      /* -shot ASCII, one pair per tile */
    "  ", "##", "()", "[]", "**", "@@", "++"
};

static unsigned char tiles[NTILE][TILEH][TILEW];   /* 1 = lit */

static void die(const char *m) { fprintf(stderr, "mktiles: %s\n", m); exit(1); }

static void load_art(void)
{
    int t, y, x;
    for (t = 0; t < NTILE; t++)
        for (y = 0; y < TILEH; y++) {
            const char *r = art[t][y];
            if (!r || (int)strlen(r) != TILEW) die("art row not 16 wide");
            for (x = 0; x < TILEW; x++)
                tiles[t][y][x] = (r[x] != '.');
        }
}

/* halves: identical 8x16 halves collapse to the lowest id (fewer DRCS slots) */
static short alias[NHALF];

static int half_eq(int a, int b)
{
    int ta = a / 2, sa = (a % 2) * 8, tb = b / 2, sb = (b % 2) * 8, x, y;
    for (y = 0; y < TILEH; y++)
        for (x = 0; x < 8; x++)
            if (tiles[ta][y][sa + x] != tiles[tb][y][sb + x]) return 0;
    return 1;
}

static void build_halves(void)
{
    int h, i;
    for (h = 0; h < NHALF; h++) {
        alias[h] = (short)h;
        for (i = 0; i < h; i++)
            if (alias[i] == i && half_eq(i, h)) { alias[h] = (short)i; break; }
    }
}

/* ---- geometry transform (kept identical to vt_term.c half_glyph) ---------- */

static void half_glyph(int h, int w, int gh, unsigned char out[32][16])
{
    unsigned char hb[16][16];
    int t = h / 2, sd = (h % 2) * 8, x, y, per;

    for (y = 0; y < TILEH; y++) {
        per = tiles[t][y][sd] != tiles[t][y][sd + 1];
        for (x = 0; x < 6 && per; x++)
            if (tiles[t][y][sd + x] != tiles[t][y][sd + x + 2]) per = 0;
        for (x = 0; x < w; x++)
            hb[y][x] = per ? (unsigned char)(tiles[t][y][sd] ^ (x & 1))
                           : tiles[t][y][sd + (2 * x + 1) * 8 / (2 * w)];
    }
    for (x = 0; x < w; x++) {
        per = hb[0][x] != hb[1][x];
        for (y = 0; y < 14 && per; y++)
            if (hb[y][x] != hb[y + 2][x]) per = 0;
        for (y = 0; y < gh; y++)
            out[y][x] = per ? (unsigned char)(hb[0][x] ^ (y & 1))
                            : hb[(2 * y + 1) * TILEH / (2 * gh)][x];
    }
}

static void emit_glyph(FILE *f, unsigned char g[32][16], int w, int h)
{
    int band, x, i;
    for (band = 0; band < h; band += 6) {
        if (band) fputc('/', f);
        for (x = 0; x < w; x++) {
            int v = 0;
            for (i = 0; i < 6 && band + i < h; i++)
                if (g[band + i][x]) v |= 1 << i;
            fputc(63 + v, f);
        }
    }
}

/* ---- demo board (hardware verification) ----------------------------------- */

/* a compact sample level that exercises every tile type */
static const char *demo_map[] = {
    "#######",
    "#. $ @#",
    "# $*# #",
    "#  .+ #",
    "#######",
};
#define DEMO_ROWS (int)(sizeof demo_map / sizeof demo_map[0])

static int tile_of(char c)
{
    switch (c) {
    case '#': return T_WALL;
    case '$': return T_BOX;
    case '*': return T_BOXGOAL;
    case '@': return T_PLAYER;
    case '+': return T_PLAYERGOAL;
    case '.': return T_GOAL;
    default:  return T_FLOOR;
    }
}

static void write_demo(const char *prefix, const char *tag, int w, int gh)
{
    char path[512];
    FILE *f;
    short slot_of[NHALF];
    int nslot = 0, h, y, x;

    snprintf(path, sizeof path, "%s%s.vt", prefix, tag);
    f = fopen(path, "w");
    if (!f) die("cannot write demo");
    for (h = 0; h < NHALF; h++) slot_of[h] = -1;

    /* Same terminal setup the game does in vt_open: without it a terminal
     * left in 132-column mode (or holding a stale soft font) shows the ASCII
     * fallback instead of the glyphs. */
    fputs("\033 F", f);                         /* S7C1T: 7-bit C1 replies */
    fputs("\033[?7l", f);                       /* autowrap off */
    fputs("\033[?3l", f);                       /* 80 columns (10px cell) */
    fputs("\033[m\033[2J\033[H", f);            /* reset attrs, clear */
    fputs("\033P0;0;2{ @\033\\", f);            /* erase any resident font */
    fputs("\033) @", f);                        /* G1 = DRCS " @" */
    fprintf(f, "VT Sokoban DRCS tiles (%dx%d glyphs)\r\n\r\n", w, gh);

    /* download one glyph per unique half into consecutive slots */
    for (h = 0; h < NHALF; h++) {
        unsigned char g[32][16];
        int a = alias[h];
        if (slot_of[a] >= 0) { slot_of[h] = slot_of[a]; continue; }
        if (nslot >= NSLOT) die("demo: out of DRCS slots");
        slot_of[a] = slot_of[h] = (short)nslot++;
        half_glyph(a, w, gh, g);
        fprintf(f, "\033P0;%d;1;%d;0;2;%d;0{ @", slot_of[h] + 1, w, gh);
        emit_glyph(f, g, w, gh);
        fputs("\033\\", f);
    }

    for (y = 0; y < DEMO_ROWS; y++) {
        fputs("    ", f);
        for (x = 0; demo_map[y][x]; x++) {
            int t = tile_of(demo_map[y][x]);
            int l = slot_of[alias[t * 2]], r = slot_of[alias[t * 2 + 1]];
            fprintf(f, "\016%c%c\017", 33 + l, 33 + r);   /* SO half half SI */
        }
        fputs("\r\n", f);
    }
    fprintf(f, "\r\n%d glyph slots used\r\n", nslot);
    fputs("\033[?7h", f);                       /* autowrap back on for the shell */
    fclose(f);
    printf("wrote %s\n", path);
}

/* ---- host-side preview (my own eyeballing, not shipped) -------------------- */

static void write_preview(const char *path)
{
    FILE *f = fopen(path, "w");
    int cell = TILEH + 2, W = NTILE * cell, t, y, x;

    if (!f) die("cannot write preview");
    fprintf(f, "P2\n%d %d\n1\n", W, cell);
    for (y = 0; y < cell; y++) {
        for (t = 0; t < NTILE; t++)
            for (x = 0; x < cell; x++) {
                int on = 0;
                if (y >= 1 && y <= TILEH && x >= 1 && x <= TILEW)
                    on = tiles[t][y - 1][x - 1];
                fprintf(f, "%d ", on ? 0 : 1);
            }
        fputc('\n', f);
    }
    fclose(f);
    printf("wrote %s\n", path);
}

/* ---- header emission ------------------------------------------------------ */

static void write_header(const char *out)
{
    FILE *f = fopen(out, "w");
    int t, y, h, n = 0;

    if (!f) die("cannot write header");
    fputs("/* generated by mktiles - do not edit */\n\n", f);
    fprintf(f, "#define VT_NALLTILE %d\n#define VT_NHALF %d\n\n", NTILE, NHALF);
    fputs("#define TILE_FLOOR       0\n"
          "#define TILE_WALL        1\n"
          "#define TILE_GOAL        2\n"
          "#define TILE_BOX         3\n"
          "#define TILE_BOX_ON_GOAL 4\n"
          "#define TILE_PLAYER      5\n"
          "#define TILE_PLAYER_ON_GOAL 6\n\n", f);
    fputs("extern const unsigned char VtTileBits[VT_NALLTILE][16][2];\n"
          "extern const short VtHalfAlias[VT_NHALF];\n"
          "extern const char VtTilePrev[VT_NALLTILE][2];\n\n"
          "#ifdef VT_TILE_DATA\n", f);

    fputs("const unsigned char VtTileBits[VT_NALLTILE][16][2] = {\n", f);
    for (t = 0; t < NTILE; t++) {
        fputs("{", f);
        for (y = 0; y < TILEH; y++) {
            int b0 = 0, b1 = 0, x;
            for (x = 0; x < 8; x++) {
                if (tiles[t][y][x])     b0 |= 1 << x;
                if (tiles[t][y][8 + x]) b1 |= 1 << x;
            }
            fprintf(f, "{%d,%d},", b0, b1);
        }
        fprintf(f, "}%s\n", t < NTILE - 1 ? "," : "");
    }
    fputs("};\n", f);

    fputs("const short VtHalfAlias[VT_NHALF] = {", f);
    for (h = 0; h < NHALF; h++) fprintf(f, "%s%d", h ? "," : "", alias[h]);
    fputs("};\n", f);

    fputs("const char VtTilePrev[VT_NALLTILE][2] = {", f);
    for (t = 0; t < NTILE; t++)
        fprintf(f, "%s{%d,%d}", t ? "," : "", prev[t][0], prev[t][1]);
    fputs("};\n#endif /* VT_TILE_DATA */\n", f);
    fclose(f);

    for (h = 0; h < NHALF; h++) if (alias[h] == h) n++;
    printf("wrote %s (%d tiles, %d unique halves)\n", out, NTILE, n);
}

int main(int argc, char **argv)
{
    const char *out = NULL, *demo = NULL, *preview = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) demo = argv[++i];
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) preview = argv[++i];
        else die("usage: mktiles [-o out.h] [-d demoprefix] [-p preview.pgm]");
    }

    load_art();
    build_halves();

    if (out) write_header(out);
    if (preview) write_preview(preview);
    if (demo) {
        write_demo(demo, "420", 10, 16);   /* VT420/510/520, PC emulators */
        write_demo(demo, "340", 10, 20);   /* VT330/340 */
        write_demo(demo, "320", 15, 12);   /* VT320 */
    }
    return 0;
}
