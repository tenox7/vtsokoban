/* mktiles - package VT Sokoban's art for the DRCS soft font.
 *
 * Everything the game draws is built from one currency: an 8x16 source cell
 * that becomes exactly one DRCS glyph.  A map tile is 2 cells (16x16), a logo
 * letter is 2x2 cells (16x32), a counter digit or a key cap is 2x1 (16x16).
 * Art is painted into a scratch bitmap, sliced into cells, and identical cells
 * are interned so they share a slot -- the terminal has 94 and the whole game
 * fits with room to spare, so the font is downloaded once and stays resident.
 *
 * Emits soko_tiles.h: the cell bitmaps plus index tables saying which cells
 * make up each tile / logo row / digit / key cap -- the layout vt_term.c
 * consumes.  The 8x16 -> cell resampler here is a copy of vt_term.c's; the two
 * must stay identical or the -shot preview and the terminal disagree.
 *
 *   mktiles [-o soko_tiles.h] [-d demoprefix] [-p preview.pgm]
 *     -d  write demoprefix{420,340,320}.vt raw-escape demo screens
 *     -p  write a preview.pgm sheet of every glyph (host-side eyeballing)
 *
 * Host-side dev tool; `make` runs it and the header it writes is not in git.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CW 8                    /* source cell: 8 px wide ... */
#define CH 16                   /* ... 16 px tall = one DRCS glyph */
#define NSLOT 94                /* DRCS positions in a 94-char set */
#define MAXCELL 128

#define NTILE 7

/* tile indices -- must match the TILE_* enum used by the game */
enum { T_FLOOR, T_WALL, T_GOAL, T_BOX, T_BOXGOAL, T_PLAYER, T_PLAYERGOAL };

/* ---- tile art: 16x16, modelled on the classic Thinking-Rabbit skin --------
 * Any char other than '.' is an "on" (lit) pixel; rows are exactly 16 wide. */
static const char *tile_art[NTILE][16] = {
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

static const char *tile_prev[NTILE] = {     /* -shot ASCII, one pair per tile */
    "  ", "##", "()", "[]", "**", "@@", "++"
};

/* ---- logo art: one 16x32 block per letter (2x2 cells) ---------------------
 * A bold 4px-stroke pixel face -- deliberately blockier than the terminal's
 * own font so the title reads as artwork rather than as text.  Letters are 14
 * px wide with a 2 px right bearing. */

#define LOGOW 16
#define LOGOH 32

#define LOGO_STR "VT SOKOBAN"
static const char LogoSet[] = "VTSOKBAN";   /* unique letters, art order */

static const char *logo_art[8][LOGOH] = {
    {                                       /* V */
        "................",
        "................",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        ".####....####...",
        ".####....####...",
        ".####....####...",
        ".####....####...",
        ".####....####...",
        ".####....####...",
        "..####..####....",
        "..####..####....",
        "..####..####....",
        "..####..####....",
        "..####..####....",
        "...########.....",
        "...########.....",
        "...########.....",
        "....######......",
        "....######......",
        "....######......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        "................",
        "................",
    },
    {                                       /* T */
        "................",
        "................",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        ".....####.......",
        "................",
        "................",
    },
    {                                       /* S */
        "................",
        "................",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        "####............",
        "####............",
        "####............",
        "####............",
        "####............",
        "####............",
        "####............",
        "####............",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        "..........####..",
        "..........####..",
        "..........####..",
        "..........####..",
        "..........####..",
        "..........####..",
        "..........####..",
        "..........####..",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        "................",
        "................",
    },
    {                                       /* O */
        "................",
        "................",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        "................",
        "................",
    },
    {                                       /* K */
        "................",
        "................",
        "####......####..",
        "####......####..",
        "####.....####...",
        "####.....####...",
        "####....####....",
        "####....####....",
        "####...####.....",
        "####...####.....",
        "####..####......",
        "####..####......",
        "####.####.......",
        "####.####.......",
        "########........",
        "########........",
        "########........",
        "########........",
        "####.####.......",
        "####.####.......",
        "####..####......",
        "####..####......",
        "####...####.....",
        "####...####.....",
        "####....####....",
        "####....####....",
        "####.....####...",
        "####.....####...",
        "####......####..",
        "####......####..",
        "................",
        "................",
    },
    {                                       /* B */
        "................",
        "................",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        "................",
        "................",
    },
    {                                       /* A */
        "................",
        "................",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "##############..",
        "##############..",
        "##############..",
        "##############..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "................",
        "................",
    },
    {                                       /* N */
        "................",
        "................",
        "######....####..",
        "######....####..",
        "######....####..",
        "#######...####..",
        "#######...####..",
        "#######...####..",
        "########..####..",
        "########..####..",
        "########..####..",
        "####.####.####..",
        "####.####.####..",
        "####.####.####..",
        "####..########..",
        "####..########..",
        "####..########..",
        "####...#######..",
        "####...#######..",
        "####...#######..",
        "####....######..",
        "####....######..",
        "####....######..",
        "####.....#####..",
        "####.....#####..",
        "####.....#####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "####......####..",
        "................",
        "................",
    },
};

/* ---- counters: seven-segment digits, 2x1 cells (16x16) --------------------
 * A LED readout reads instantly as a score and, unlike a hand-drawn face,
 * every digit is the same weight.  Segment rectangles inside the 16x16 block:
 *
 *      A      cols 2..13 wide, rows 0..14 tall, 3 px thick
 *    F   B
 *      G
 *    E   C
 *      D
 */
#define NUM_STR "0123456789/"
#define NNUM 11

/* bit 0=A 1=B 2=C 3=D 4=E 5=F 6=G -- the classic seven-segment table */
static const unsigned char seg_of[10] = {
    0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
};

/* x, y, w, h of each segment, indexed as the bits above */
static const unsigned char seg_box[7][4] = {
    {  2,  0, 12,  3 },                     /* A  top */
    { 11,  0,  3,  9 },                     /* B  upper right */
    { 11,  6,  3,  9 },                     /* C  lower right */
    {  2, 12, 12,  3 },                     /* D  bottom */
    {  2,  6,  3,  9 },                     /* E  lower left */
    {  2,  0,  3,  9 },                     /* F  upper left */
    {  2,  6, 12,  3 },                     /* G  middle */
};

static const char *slash_art[16] = {        /* the '/' of "01/24" */
    "...........###..",
    "...........###..",
    "..........###...",
    "..........###...",
    ".........###....",
    ".........###....",
    "........###.....",
    "........###.....",
    ".......###......",
    ".......###......",
    "......###.......",
    "......###.......",
    ".....###........",
    ".....###........",
    "....###.........",
    "....###.........",
};

/* ---- key caps: 2x1 cells (16x16) -----------------------------------------
 * A rounded cap with the letter set into it, so the help line reads as keys
 * you press rather than as bracketed punctuation. */

#define KEY_STR "RNPCQ"
#define NKEY 5

static const char *cap_art[16] = {          /* the empty cap all keys share */
    "..############..",
    ".##############.",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    ".##############.",
    "..############..",
};

/* 10x10 letter faces, stamped into the cap at (3,3) */
static const char *key_face[NKEY][10] = {
    {                                       /* R */
        "########..",
        "########..",
        "##....##..",
        "##....##..",
        "########..",
        "########..",
        "##..##....",
        "##..##....",
        "##...##...",
        "##....##..",
    },
    {                                       /* N */
        "##....##..",
        "###...##..",
        "###...##..",
        "####..##..",
        "##.##.##..",
        "##.##.##..",
        "##..####..",
        "##...###..",
        "##...###..",
        "##....##..",
    },
    {                                       /* P */
        "########..",
        "########..",
        "##....##..",
        "##....##..",
        "##....##..",
        "########..",
        "########..",
        "##........",
        "##........",
        "##........",
    },
    {                                       /* C */
        "..######..",
        ".########.",
        "###....##.",
        "##........",
        "##........",
        "##........",
        "##........",
        "###....##.",
        ".########.",
        "..######..",
    },
    {                                       /* Q */
        "..######..",
        ".########.",
        "##......##",
        "##......##",
        "##......##",
        "##......##",
        "##......##",
        ".##....##.",
        "..######..",
        ".....#####",
    },
};

/* ---- cell pool ------------------------------------------------------------ */

static unsigned char Cell[MAXCELL][CH][CW];     /* 1 = lit */
static int NCell = 0;

static void die(const char *m) { fprintf(stderr, "mktiles: %s\n", m); exit(1); }

/* scratch bitmap: art is painted here, then sliced into cells */
#define BMW 192
#define BMH 32
static unsigned char Bm[BMH][BMW];

static void bm_clear(void) { memset(Bm, 0, sizeof Bm); }

/* paste h rows of w chars ('.' = off) at (x0,y0) */
static void bm_art(const char **rows, int w, int h, int x0, int y0)
{
    int x, y;
    for (y = 0; y < h; y++) {
        if (!rows[y] || (int)strlen(rows[y]) != w) die("art row width mismatch");
        for (x = 0; x < w; x++)
            if (rows[y][x] != '.') Bm[y0 + y][x0 + x] = 1;
    }
}

static void bm_rect(int x0, int y0, int w, int h)
{
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) Bm[y0 + y][x0 + x] = 1;
}

/* intern the 8x16 cell at (x0,y0); identical art returns the existing id */
static int bm_cell(int x0, int y0)
{
    unsigned char c[CH][CW];
    int i, x, y;

    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++) c[y][x] = Bm[y0 + y][x0 + x];
    for (i = 0; i < NCell; i++)
        if (!memcmp(Cell[i], c, sizeof c)) return i;
    if (NCell >= MAXCELL) die("cell pool full");
    memcpy(Cell[NCell], c, sizeof c);
    return NCell++;
}

/* ---- art -> cells --------------------------------------------------------- */

static short TileCell[NTILE][2];
#define LOGOCELLS ((int)(sizeof LOGO_STR - 1) * 2)
static short LogoCell[2][LOGOCELLS];
static short NumCell[NNUM][2];
static short KeyCell[NKEY][2];

static void build_tiles(void)
{
    int t;
    for (t = 0; t < NTILE; t++) {
        bm_clear();
        bm_art(tile_art[t], 16, 16, 0, 0);
        TileCell[t][0] = (short)bm_cell(0, 0);
        TileCell[t][1] = (short)bm_cell(8, 0);
    }
}

static void build_logo(void)
{
    const char *s = LOGO_STR;
    int i, row, col;

    bm_clear();
    for (i = 0; s[i]; i++) {
        const char *p = strchr(LogoSet, s[i]);
        if (!p) continue;                   /* space: leave it blank */
        bm_art(logo_art[p - LogoSet], LOGOW, LOGOH, i * LOGOW, 0);
    }
    for (row = 0; row < 2; row++)
        for (col = 0; col < LOGOCELLS; col++)
            LogoCell[row][col] = (short)bm_cell(col * CW, row * CH);
}

static void build_nums(void)
{
    int i, b;

    for (i = 0; i < NNUM; i++) {
        bm_clear();
        if (NUM_STR[i] == '/')
            bm_art(slash_art, 16, 16, 0, 0);
        else
            for (b = 0; b < 7; b++)
                if (seg_of[NUM_STR[i] - '0'] & (1 << b))
                    bm_rect(seg_box[b][0], seg_box[b][1],
                            seg_box[b][2], seg_box[b][3]);
        NumCell[i][0] = (short)bm_cell(0, 0);
        NumCell[i][1] = (short)bm_cell(8, 0);
    }
}

static void build_keys(void)
{
    int i;
    for (i = 0; i < NKEY; i++) {
        bm_clear();
        bm_art(cap_art, 16, 16, 0, 0);
        bm_art(key_face[i], 10, 10, 3, 3);
        KeyCell[i][0] = (short)bm_cell(0, 0);
        KeyCell[i][1] = (short)bm_cell(8, 0);
    }
}

/* ---- geometry transform (kept identical to vt_term.c cell_glyph) ---------- */

static void cell_glyph(int c, int w, int gh, unsigned char out[32][16])
{
    unsigned char hb[16][16];
    int x, y, per;

    for (y = 0; y < CH; y++) {
        per = Cell[c][y][0] != Cell[c][y][1];
        for (x = 0; x < 6 && per; x++)
            if (Cell[c][y][x] != Cell[c][y][x + 2]) per = 0;
        for (x = 0; x < w; x++)
            hb[y][x] = per ? (unsigned char)(Cell[c][y][0] ^ (x & 1))
                           : Cell[c][y][(2 * x + 1) * CW / (2 * w)];
    }
    for (x = 0; x < w; x++) {
        per = hb[0][x] != hb[1][x];
        for (y = 0; y < 14 && per; y++)
            if (hb[y][x] != hb[y + 2][x]) per = 0;
        for (y = 0; y < gh; y++)
            out[y][x] = per ? (unsigned char)(hb[0][x] ^ (y & 1))
                            : hb[(2 * y + 1) * CH / (2 * gh)][x];
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

/* ---- demo screen (hardware verification) ---------------------------------- */

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

static void put_cells(FILE *f, const short *cells, int n)
{
    int i;
    fputc(0x0e, f);                             /* SO: GL = G1 (DRCS) */
    for (i = 0; i < n; i++) fputc(33 + cells[i], f);
    fputc(0x0f, f);                             /* SI: GL = G0 (ASCII) */
}

/* Everything the game can draw, on one screen: cat it to a real terminal and
 * every glyph either shows up or the terminal is out of DRCS memory. */
static void write_demo(const char *prefix, const char *tag, int w, int gh)
{
    char path[512];
    FILE *f;
    int i, y, x;

    snprintf(path, sizeof path, "%s%s.vt", prefix, tag);
    f = fopen(path, "w");
    if (!f) die("cannot write demo");

    /* Same terminal setup the game does in vt_open: without it a terminal
     * left in 132-column mode (or holding a stale soft font) shows nothing
     * but the ASCII fallback. */
    fputs("\033 F", f);                         /* S7C1T: 7-bit C1 replies */
    fputs("\033[?7l", f);                       /* autowrap off */
    fputs("\033[?3l", f);                       /* 80 columns (10px cell) */
    fputs("\033[m\033[2J\033[H", f);            /* reset attrs, clear */
    fputs("\033P0;0;2{ @\033\\", f);            /* erase any resident font */
    fputs("\033) @", f);                        /* G1 = DRCS " @" */

    for (i = 0; i < NCell; i++) {               /* download the whole font */
        unsigned char g[32][16];
        cell_glyph(i, w, gh, g);
        fprintf(f, "\033P0;%d;1;%d;0;2;%d;0{ @", i + 1, w, gh);
        emit_glyph(f, g, w, gh);
        fputs("\033\\", f);
    }

    fprintf(f, "VT Sokoban DRCS font -- %d glyphs at %dx%d\r\n\r\n", NCell, w, gh);
    for (y = 0; y < 2; y++) {
        fputs("  ", f);
        put_cells(f, LogoCell[y], LOGOCELLS);
        fputs("\r\n", f);
    }
    fputs("\r\n  LEVEL ", f);
    for (i = 0; i < NNUM; i++) put_cells(f, NumCell[i], 2);
    fputs("\r\n\r\n  ", f);
    for (i = 0; i < NKEY; i++) { put_cells(f, KeyCell[i], 2); fputc(' ', f); }
    fputs("\r\n\r\n", f);
    for (y = 0; y < DEMO_ROWS; y++) {
        fputs("  ", f);
        for (x = 0; demo_map[y][x]; x++)
            put_cells(f, TileCell[tile_of(demo_map[y][x])], 2);
        fputs("\r\n", f);
    }
    fputs("\r\nAll of the above should be graphics, not letters.\r\n", f);
    fputs("\033[?7h", f);                       /* autowrap back on for the shell */
    fclose(f);
    printf("wrote %s\n", path);
}

/* ---- host-side preview (my own eyeballing, not shipped) -------------------- */

/* Every cell as it will actually be rasterised, in DRCS slot order. */
/* A mock screen of every piece of art, assembled and rasterised exactly the way
 * the terminal will do it -- the only way to see the logo as a word rather than
 * as a pile of cells.  A VT pixel is about 1.4x taller than it is wide, so
 * stretch this vertically to judge proportions. */

#define PVW 10                          /* preview at VT420 cell geometry */
#define PVH 16
#define PVCOLS 24
#define PVROWS 9
static unsigned char Pv[PVROWS * PVH][PVCOLS * PVW];

static void pv_cells(int row, int col, const short *cells, int n)
{
    unsigned char g[32][16];
    int i, x, y;

    for (i = 0; i < n && col + i < PVCOLS; i++) {
        cell_glyph(cells[i], PVW, PVH, g);
        for (y = 0; y < PVH; y++)
            for (x = 0; x < PVW; x++)
                Pv[row * PVH + y][(col + i) * PVW + x] = g[y][x];
    }
}

static void write_preview(const char *path)
{
    FILE *f = fopen(path, "w");
    int W = PVCOLS * PVW, H = PVROWS * PVH, i, y, x;

    if (!f) die("cannot write preview");
    memset(Pv, 0, sizeof Pv);
    for (y = 0; y < 2; y++) pv_cells(y, 1, LogoCell[y], LOGOCELLS);
    for (i = 0; i < NNUM; i++) pv_cells(3, 1 + i * 2, NumCell[i], 2);
    for (i = 0; i < NKEY; i++) pv_cells(5, 1 + i * 3, KeyCell[i], 2);
    for (i = 0; i < NTILE; i++) pv_cells(7, 1 + i * 2, TileCell[i], 2);

    fprintf(f, "P2\n%d %d\n1\n", W, H);
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) fprintf(f, "%d ", Pv[y][x] ? 0 : 1);
        fputc('\n', f);
    }
    fclose(f);
    printf("wrote %s\n", path);
}

/* ---- header emission ------------------------------------------------------ */

static void emit_pairs(FILE *f, const char *name, short (*p)[2], int n)
{
    int i;
    fprintf(f, "const short %s[%d][2] = {", name, n);
    for (i = 0; i < n; i++) fprintf(f, "%s{%d,%d}", i ? "," : "", p[i][0], p[i][1]);
    fputs("};\n", f);
}

static void write_header(const char *out)
{
    FILE *f = fopen(out, "w");
    int i, y, x;

    if (!f) die("cannot write header");
    fputs("/* generated by mktiles - do not edit */\n\n", f);
    fprintf(f, "#define VT_NCELL %d\n", NCell);
    fprintf(f, "#define VT_NALLTILE %d\n\n", NTILE);
    fputs("#define TILE_FLOOR       0\n"
          "#define TILE_WALL        1\n"
          "#define TILE_GOAL        2\n"
          "#define TILE_BOX         3\n"
          "#define TILE_BOX_ON_GOAL 4\n"
          "#define TILE_PLAYER      5\n"
          "#define TILE_PLAYER_ON_GOAL 6\n\n", f);
    fprintf(f, "#define VT_LOGO_STR \"%s\"\n", LOGO_STR);
    fprintf(f, "#define VT_LOGO_W %d\n#define VT_LOGO_H 2\n", LOGOCELLS);
    fprintf(f, "#define VT_NUM_STR \"%s\"\n#define VT_NNUM %d\n", NUM_STR, NNUM);
    fprintf(f, "#define VT_KEY_STR \"%s\"\n#define VT_NKEY %d\n\n", KEY_STR, NKEY);

    fputs("extern const unsigned char VtCellBits[VT_NCELL][16];\n"
          "extern const short VtTileCell[VT_NALLTILE][2];\n"
          "extern const char VtTilePrev[VT_NALLTILE][2];\n"
          "extern const short VtLogoCell[VT_LOGO_H][VT_LOGO_W];\n"
          "extern const short VtNumCell[VT_NNUM][2];\n"
          "extern const short VtKeyCell[VT_NKEY][2];\n\n"
          "#ifdef VT_TILE_DATA\n", f);

    fputs("const unsigned char VtCellBits[VT_NCELL][16] = {\n", f);
    for (i = 0; i < NCell; i++) {
        fputc('{', f);
        for (y = 0; y < CH; y++) {
            int b = 0;
            for (x = 0; x < CW; x++) if (Cell[i][y][x]) b |= 1 << x;
            fprintf(f, "%d,", b);
        }
        fprintf(f, "}%s\n", i < NCell - 1 ? "," : "");
    }
    fputs("};\n", f);

    emit_pairs(f, "VtTileCell", TileCell, NTILE);
    fputs("const char VtTilePrev[VT_NALLTILE][2] = {", f);
    for (i = 0; i < NTILE; i++)
        fprintf(f, "%s{%d,%d}", i ? "," : "", tile_prev[i][0], tile_prev[i][1]);
    fputs("};\n", f);

    fputs("const short VtLogoCell[VT_LOGO_H][VT_LOGO_W] = {\n", f);
    for (y = 0; y < 2; y++) {
        fputc('{', f);
        for (x = 0; x < LOGOCELLS; x++)
            fprintf(f, "%s%d", x ? "," : "", LogoCell[y][x]);
        fprintf(f, "}%s\n", y ? "" : ",");
    }
    fputs("};\n", f);

    emit_pairs(f, "VtNumCell", NumCell, NNUM);
    emit_pairs(f, "VtKeyCell", KeyCell, NKEY);
    fputs("#endif /* VT_TILE_DATA */\n", f);
    fclose(f);
    printf("wrote %s (%d of %d DRCS slots)\n", out, NCell, NSLOT);
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

    /* Tiles first: they are the art the game cannot do without, so they get
     * the low slots and stay put as the UI art around them changes. */
    build_tiles();
    build_logo();
    build_nums();
    build_keys();
    if (NCell > NSLOT) die("art needs more than 94 DRCS slots");

    if (out) write_header(out);
    if (preview) write_preview(preview);
    if (demo) {
        write_demo(demo, "420", 10, 16);   /* VT420/510/520, PC emulators */
        write_demo(demo, "340", 10, 20);   /* VT330/340 */
        write_demo(demo, "320", 15, 12);   /* VT320 */
    }
    return 0;
}
