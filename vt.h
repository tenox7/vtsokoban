/* vt.h  --  shared declarations for the VT Sokoban DEC VT front end.
 *
 * Renders the board on DEC VT300-series (and later) terminals using a DRCS
 * soft font, drawn by writing raw escape sequences.  No curses anywhere; the
 * backend (vt_term.c) keeps a diffed cell buffer and sends only what changed.
 *
 * The soft font holds the whole game's art, not just the board: 8x16 cells
 * that make up map tiles (2 cells), the title logo (2x2 per letter) and the
 * counter digits and key caps (2x1).  It is small enough to download once at
 * startup and leave resident -- no glyph cache, no streaming.
 */

#ifndef VT_H
#define VT_H

/* --- cell character sets --------------------------------------------------- */
#define VT_CS_ASCII 0			/* G0: normal text */
#define VT_CS_DRCS  1			/* G1: soft-font tile halves */
#define VT_CS_GFX   2			/* G2: DEC Special Graphics (lines) */

/* --- cell attributes (mono terminal) --------------------------------------- */
#define VA_BOLD  1
#define VA_UNDER 2
#define VA_BLINK 4
#define VA_REV   8

/* DEC Special Graphics glyphs (use with VT_CS_GFX) */
#define VG_HLINE   'q'
#define VG_VLINE   'x'
#define VG_ULC     'l'
#define VG_URC     'k'
#define VG_LLC     'm'
#define VG_LRC     'j'

/* --- vt_term.c: terminal backend ------------------------------------------- */
extern int ScrH, ScrW;			/* screen size in character cells */
extern int VtHeadless;			/* -shot mode: no terminal I/O at all */

void vt_open(int fontsel, int gwidth, int noquery);
void vt_close(void);
void vt_clear(void);			/* blank the whole cell buffer */
void vt_put(int y, int x, int ch, int cs, int attr);
void vt_puts(int y, int x, const char *s, int attr);
void vt_fill(int y, int x, int w, int h, int ch, int cs, int attr);
void vt_frame(int y, int x, int w, int h, int attr);
void vt_present(void);			/* diff cur vs sent, emit, flush */
void vt_repaint(void);			/* force a full repaint next present */
void vt_shot(const char *path);		/* dump the cell buffer as ASCII */

/* --- soft-font art (see soko_tiles.h for the cell tables) ------------------ */
void vt_tile(int y, int x, int tile, int attr);	  /* map tile: 2 cells wide */
void vt_logo(int y, int x, int attr);		  /* title: VT_LOGO_W x 2 */
int  vt_num(int y, int x, const char *s, int attr);   /* "01/24": 2 cells/char */
int  vt_keycap(int y, int x, int key, int attr);  /* one key cap: 2 cells */

/* keys returned by vt_getkey (plus plain ASCII chars) */
#define VK_NONE  (-1)
#define VK_ESC   27
#define VK_UP    0x1000
#define VK_DOWN  0x1001
#define VK_LEFT  0x1002
#define VK_RIGHT 0x1003
int vt_getkey(int wait_ms);		/* decoded key or VK_NONE on timeout */

#endif /* VT_H */
