/* vt_term.c  --  raw-escape terminal backend for VT Sokoban.
 *
 * Talks straight to a DEC VT300-series (or later) terminal: locking-shift
 * charset switching (G0 ASCII / G1 DRCS / G2 DEC Special Graphics) and a
 * diffed cell buffer so each frame sends only what changed -- playable over a
 * slow serial line.
 *
 * The soft font is tiny and static: at startup every unique 8x16 tile half is
 * downloaded once with DECDLD into a fixed DRCS slot and stays resident for
 * the whole session (7 tiles = 13 unique halves, far under the 94 slots).  So
 * there is no glyph cache and no streaming -- vt_tile resolves a tile straight
 * to its slot characters.
 *
 * Glyph geometry: one 16x16 tile is two 8x16 half-tile characters, resampled
 * to the terminal's cell (VT420 10x16, VT340 10x20, VT320 15x12).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>

#include "vt.h"
#define VT_TILE_DATA
#include "soko_tiles.h"

#define VT_MAXH 64
#define VT_MAXW 160
#define NSLOT 94

int ScrH = 24, ScrW = 80;
int VtHeadless = 0;

typedef struct {
  unsigned short ch;			/* ASCII/GFX char, or DRCS slot char */
  unsigned char cs, attr;
  char prev;				/* ASCII approximation for vt_shot */
} VCell;

static VCell Cur[VT_MAXH][VT_MAXW];	/* what we want on screen */
static VCell Sent[VT_MAXH][VT_MAXW];	/* what the terminal currently shows */
static int NeedFull = 1;
static struct termios SavedTio;
static int TioSaved = 0;

/* glyph geometry, chosen from the terminal model */
static int GlyphW = 10, GlyphH = 16;

/* half id -> DRCS slot, filled once by preload_font() */
static short HalfSlot[VT_NHALF];

/* ---- output buffering ------------------------------------------------------ */

static char Out[16384];
static int OutLen = 0;

static void
oflush(void)
{
  int off = 0, n;
  while (off < OutLen) {
    n = (int)write(STDOUT_FILENO, Out + off, (size_t)(OutLen - off));
    if (n < 0) {
      if (errno == EINTR || errno == EAGAIN) continue;
      break;
    }
    off += n;
  }
  OutLen = 0;
}

static void
outs(const char *s)
{
  int len = (int)strlen(s);
  if (OutLen + len > (int)sizeof(Out)) oflush();
  memcpy(Out + OutLen, s, (size_t)len);
  OutLen += len;
}

static void
outc(int c)
{
  if (OutLen + 1 > (int)sizeof(Out)) oflush();
  Out[OutLen++] = (char)c;
}

/* ---- input ------------------------------------------------------------------ */

static int
inbyte(int wait_ms)
{
  fd_set fds;
  struct timeval tv;
  unsigned char b;
  int r;

  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  tv.tv_sec = wait_ms / 1000;
  tv.tv_usec = (wait_ms % 1000) * 1000;
  r = select(STDIN_FILENO + 1, &fds, NULL, NULL, wait_ms < 0 ? NULL : &tv);
  if (r <= 0) return -1;
  if (read(STDIN_FILENO, &b, 1) != 1) return -1;
  return b;
}

/* Next byte of an escape sequence; XON/XOFF flow-control bytes are skipped. */
static int
seqbyte(void)
{
  int c;
  do {
    c = inbyte(50);
  } while (c == 0x11 || c == 0x13);
  return c;
}

/* Decode one key.  Handles CSI (7-bit ESC [ and 8-bit 0x9B) and SS3 arrow
 * reports; a lone Esc is returned as VK_ESC.  Unknown reports are consumed
 * through their final byte so the tail is never delivered as keystrokes. */
int
vt_getkey(int wait_ms)
{
  int c, fin, n;
  char num[8];

  do {
    c = inbyte(wait_ms);
  } while (c == 0x11 || c == 0x13);
  if (c < 0) return VK_NONE;

  if (c == 0x9b) goto csi;
  if (c != 27) return c;

  c = seqbyte();
  if (c < 0) return VK_ESC;
  if (c == 'O') {			/* SS3: application arrows */
    c = seqbyte();
    switch (c) {
    case 'A': return VK_UP;
    case 'B': return VK_DOWN;
    case 'C': return VK_RIGHT;
    case 'D': return VK_LEFT;
    }
    return VK_NONE;
  }
  if (c != '[') return VK_ESC;
csi:
  n = 0;
  num[0] = 0;
  for (;;) {
    fin = seqbyte();
    if (fin < 0) return VK_NONE;
    if (fin >= '0' && fin <= '9') {
      if (n < (int)sizeof(num) - 1) { num[n++] = (char)fin; num[n] = 0; }
      continue;
    }
    if (fin == ';') { n = 0; num[0] = 0; continue; }
    if (fin >= 0x20 && fin <= 0x3f) continue;
    if (fin >= 0x40 && fin <= 0x7e) break;
    return VK_NONE;
  }
  switch (fin) {
  case 'A': return VK_UP;
  case 'B': return VK_DOWN;
  case 'C': return VK_RIGHT;
  case 'D': return VK_LEFT;
  }
  return VK_NONE;
}

/* ---- startup probes --------------------------------------------------------- */

static int
read_report(char *buf, int buflen, int fin, int wait_ms)
{
  int c, n = 0;
  for (;;) {
    c = inbyte(wait_ms);
    if (c < 0) return 0;
    if (n < buflen - 1) buf[n++] = (char)c;
    if (c == fin) break;
  }
  buf[n] = 0;
  return 1;
}

/* DA1: ESC [ ? p1 ; ... c  -> 1 if parameter `feat` is present */
static int
da1_has_feature(const char *rep, int feat)
{
  const char *p = strchr(rep, '?');
  int v = 0;
  if (!p) return 0;
  for (p++; ; p++) {
    if (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); continue; }
    if (v == feat) return 1;
    v = 0;
    if (*p != ';') break;
  }
  return 0;
}

/* DA2: ESC [ > id ; fw ; kb c  -> model id, or -1 */
static int
da2_model(const char *rep)
{
  const char *p = strchr(rep, '>');
  if (!p) return -1;
  return atoi(p + 1);
}

/* Glyph geometry by terminal model; 0 = VT2xx (unsupported). */
static int
geometry_for_model(int id, int *w, int *h)
{
  switch (id) {
  case 1: case 2:			/* VT220/VT240: 8x10, too small */
    return 0;
  case 24: case 42:			/* VT320, VT1000: 15x12 cell */
    *w = 15; *h = 12; return 1;
  case 18: case 19:			/* VT330/VT340: 10x20 cell */
    *w = 10; *h = 20; return 1;
  case 32: case 44:			/* VT382: 12x30 cell */
    *w = 12; *h = 30; return 1;
  default:				/* VT420/510/520, PC emulators */
    *w = 10; *h = 16; return 1;
  }
}

static void
set_geometry_sel(int sel)
{
  if (sel == 320)      { GlyphW = 15; GlyphH = 12; }
  else if (sel == 330 || sel == 340) { GlyphW = 10; GlyphH = 20; }
  else                 { GlyphW = 10; GlyphH = 16; }
}

static void
screen_size(void)
{
  struct winsize ws;
  char rep[64];
  int r, c;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
    ScrW = ws.ws_col;
    ScrH = ws.ws_row;
  } else {
    outs("\0337\033[999;999H\033[6n");
    oflush();
    if (read_report(rep, sizeof rep, 'R', 2000) &&
	sscanf(strchr(rep, '[') ? strchr(rep, '[') + 1 : "", "%d;%d", &r, &c) == 2) {
      ScrH = r;
      ScrW = c;
    }
    outs("\0338");
  }
  if (ScrW > VT_MAXW) ScrW = VT_MAXW;
  if (ScrH > VT_MAXH) ScrH = VT_MAXH;
  if (ScrW < 40) ScrW = 40;
  if (ScrH < 10) ScrH = 10;
}

static void
on_fatal_signal(int sig)
{
  (void)sig;
  vt_close();
  _exit(1);
}

static volatile int Winched = 0;

static void
on_winch(int sig)
{
  (void)sig;
  Winched = 1;
}

/* ---- glyph download --------------------------------------------------------- */

/* One 8x16 half -> GlyphW x GlyphH pixels.  Period-2 dither rows/columns
 * continue their pattern at the target size; structured art resamples
 * centered-nearest.  At 8x16 it is an exact copy.  (Identical to mktiles.) */
static void
half_glyph(int half, unsigned char g[32][16])
{
  unsigned char s[16][8], hb[16][16];
  int t = half / 2, sd = half % 2, x, y, per;

  for (y = 0; y < 16; y++)
    for (x = 0; x < 8; x++)
      s[y][x] = (VtTileBits[t][y][sd] >> x) & 1;

  for (y = 0; y < 16; y++) {		/* horizontal: 8 -> GlyphW */
    per = s[y][0] != s[y][1];
    for (x = 0; x < 6 && per; x++)
      if (s[y][x] != s[y][x + 2]) per = 0;
    for (x = 0; x < GlyphW; x++)
      hb[y][x] = per ? (unsigned char)(s[y][0] ^ (x & 1))
		     : s[y][(2 * x + 1) * 8 / (2 * GlyphW)];
  }
  for (x = 0; x < GlyphW; x++) {	/* vertical: 16 -> GlyphH */
    per = hb[0][x] != hb[1][x];
    for (y = 0; y < 14 && per; y++)
      if (hb[y][x] != hb[y + 2][x]) per = 0;
    for (y = 0; y < GlyphH; y++)
      g[y][x] = per ? (unsigned char)(hb[0][x] ^ (y & 1))
		    : hb[(2 * y + 1) * 16 / (2 * GlyphH)][x];
  }
}

/* one-character DECDLD: define DRCS slot `slot` as tile half `half` */
static void
load_glyph(int slot, int half)
{
  unsigned char g[32][16];
  char hdr[48];
  int band, x, i;

  half_glyph(half, g);
  sprintf(hdr, "\033P0;%d;1;%d;0;2;%d;0{ @", slot + 1, GlyphW, GlyphH);
  outs(hdr);
  for (band = 0; band < GlyphH; band += 6) {
    if (band) outc('/');
    for (x = 0; x < GlyphW; x++) {
      int v = 0;
      for (i = 0; i < 6 && band + i < GlyphH; i++)
	if (g[band + i][x]) v |= 1 << i;
      outc(63 + v);
    }
  }
  outs("\033\\");
}

/* Download every unique tile half into a fixed slot, once, and remember the
 * slot for each half (aliases share their canonical half's slot). */
static void
preload_font(void)
{
  int h, nslot = 0;

  outs("\033P0;0;2{ @\033\\");		/* erase any resident soft font */
  outs("\033) @");			/* G1 = DRCS " @" */
  for (h = 0; h < VT_NHALF; h++) {
    int a = VtHalfAlias[h];
    if (a != h) { HalfSlot[h] = HalfSlot[a]; continue; }
    if (nslot >= NSLOT) continue;	/* cannot happen for this tile set */
    HalfSlot[h] = (short)nslot;
    load_glyph(nslot, h);
    nslot++;
  }
}

/* ---- terminal session ------------------------------------------------------- */

void
vt_open(int fontsel, int gwidth, int noquery)
{
  struct termios tio;
  char rep[256];

  if (VtHeadless) return;

  tcgetattr(STDIN_FILENO, &SavedTio);
  TioSaved = 1;
  tio = SavedTio;
  tio.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
  /* IXON stays ON: the terminal's XOFF must pause our output or a DECDLD
   * burst overruns the input silo on a real serial line. */
  tio.c_iflag &= ~(ICRNL | INLCR);
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio);

  signal(SIGTERM, on_fatal_signal);
  signal(SIGHUP, on_fatal_signal);
  signal(SIGQUIT, on_fatal_signal);
  signal(SIGINT, on_fatal_signal);
#ifdef SIGWINCH
  signal(SIGWINCH, on_winch);
#endif

  outs("\033 F");			/* S7C1T: 7-bit C1 replies */
  outs("\033[?25l");			/* hide cursor */
  outs("\033[?7l");			/* autowrap off */
  outs("\033[?3l");			/* 80 columns */
  outs("\033[m\033[2J\033[H");

  screen_size();

  if (fontsel) {
    set_geometry_sel(fontsel);
  } else if (!noquery) {
    outs("\033[c");			/* DA1 */
    oflush();
    if (read_report(rep, sizeof rep, 'c', 2000)) {
      if (!da1_has_feature(rep, 7)) {
	vt_close();
	fprintf(stderr,
	  "vtsokoban: this terminal does not report DRCS soft-font support\n"
	  "        (DA1 feature 7).  A DEC VT320/330/340/420/520 or an\n"
	  "        emulator with DECDLD support is required.\n"
	  "        Use -t 320|340|420 to force a font and try anyway.\n");
	exit(1);
      }
      outs("\033[>c");			/* DA2: model id */
      oflush();
      if (read_report(rep, sizeof rep, 'c', 1000)) {
	if (!geometry_for_model(da2_model(rep), &GlyphW, &GlyphH)) {
	  vt_close();
	  fprintf(stderr,
	    "vtsokoban: VT2xx terminals only load 8x10 soft fonts, which are\n"
	    "        too small for the tile set.  A VT320 or later is needed.\n");
	  exit(1);
	}
      }
    }
  }

  if (gwidth >= 5 && gwidth <= 15)
    GlyphW = gwidth;			/* -w: match an emulator's pixel grid */

  preload_font();
  outs("\033*0");			/* G2 = DEC Special Graphics */
  outs("\017");				/* GL = G0 (ASCII) */

  outs("\033[6n");			/* sync barrier */
  oflush();
  read_report(rep, sizeof rep, 'R', 4000);

  outs("\033[2J\033[H");
  oflush();
  vt_repaint();
}

void
vt_close(void)
{
  if (VtHeadless) return;
  outs("\017\033[m\033[2J\033[H");	/* SI, plain video, clear */
  outs("\033P0;0;2{ @\033\\");		/* erase the soft font */
  outs("\033)B");			/* G1 back to ASCII */
  outs("\033*<");			/* G2 back to user-preference suppl. */
  outs("\033[?7h\033[?25h");		/* autowrap + cursor back on */
  oflush();
  if (TioSaved) tcsetattr(STDIN_FILENO, TCSAFLUSH, &SavedTio);
}

/* ---- cell buffer ------------------------------------------------------------ */

void
vt_repaint(void)
{
  NeedFull = 1;
}

void
vt_clear(void)
{
  int y, x;
  for (y = 0; y < ScrH; y++)
    for (x = 0; x < ScrW; x++) {
      VCell *c = &Cur[y][x];
      c->ch = ' ';
      c->cs = VT_CS_ASCII;
      c->attr = 0;
      c->prev = ' ';
    }
}

void
vt_put(int y, int x, int ch, int cs, int attr)
{
  VCell *c;

  if (y < 0 || y >= ScrH || x < 0 || x >= ScrW) return;
  c = &Cur[y][x];
  c->ch = (unsigned short)ch;
  c->cs = (unsigned char)cs;
  c->attr = (unsigned char)attr;
  if (cs == VT_CS_ASCII) {
    c->prev = (char)((ch >= 32 && ch < 127) ? ch : '?');
  } else if (cs == VT_CS_GFX) {
    switch (ch) {
    case VG_HLINE: c->prev = '-'; break;
    case VG_VLINE: c->prev = '|'; break;
    default: c->prev = '+'; break;
    }
  } else {
    c->prev = '?';
  }
}

void
vt_puts(int y, int x, const char *s, int attr)
{
  for (; *s; s++, x++) vt_put(y, x, (unsigned char)*s, VT_CS_ASCII, attr);
}

void
vt_fill(int y, int x, int w, int h, int ch, int cs, int attr)
{
  int i, j;
  for (j = 0; j < h; j++)
    for (i = 0; i < w; i++)
      vt_put(y + j, x + i, ch, cs, attr);
}

/* dialog frame: border in DEC line graphics, interior cleared */
void
vt_frame(int y, int x, int w, int h, int attr)
{
  int i;

  if (w < 2 || h < 2) return;
  vt_fill(y + 1, x + 1, w - 2, h - 2, ' ', VT_CS_ASCII, attr);
  vt_put(y, x, VG_ULC, VT_CS_GFX, attr);
  vt_put(y, x + w - 1, VG_URC, VT_CS_GFX, attr);
  vt_put(y + h - 1, x, VG_LLC, VT_CS_GFX, attr);
  vt_put(y + h - 1, x + w - 1, VG_LRC, VT_CS_GFX, attr);
  for (i = 1; i < w - 1; i++) {
    vt_put(y, x + i, VG_HLINE, VT_CS_GFX, attr);
    vt_put(y + h - 1, x + i, VG_HLINE, VT_CS_GFX, attr);
  }
  for (i = 1; i < h - 1; i++) {
    vt_put(y + i, x, VG_VLINE, VT_CS_GFX, attr);
    vt_put(y + i, x + w - 1, VG_VLINE, VT_CS_GFX, attr);
  }
}

/* one map tile: two half-tile DRCS cells, resolved straight to slot chars */
void
vt_tile(int y, int x, int tile, int attr)
{
  int s;

  if (tile < 0 || tile >= VT_NALLTILE) tile = 0;
  for (s = 0; s < 2; s++) {
    VCell *c;
    if (y < 0 || y >= ScrH || x + s < 0 || x + s >= ScrW) continue;
    c = &Cur[y][x + s];
    c->ch = (unsigned short)(33 + HalfSlot[VtHalfAlias[tile * 2 + s]]);
    c->cs = VT_CS_DRCS;
    c->attr = (unsigned char)attr;
    c->prev = VtTilePrev[tile][s];
  }
}

/* ---- diff + emit ------------------------------------------------------------ */

static int EmAttr = -1, EmCS = -1;

static void
em_attr(int attr)
{
  char buf[24];
  int n;

  if (attr == EmAttr) return;
  n = sprintf(buf, "\033[0");
  if (attr & VA_BOLD)  n += sprintf(buf + n, ";1");
  if (attr & VA_UNDER) n += sprintf(buf + n, ";4");
  if (attr & VA_BLINK) n += sprintf(buf + n, ";5");
  if (attr & VA_REV)   n += sprintf(buf + n, ";7");
  buf[n++] = 'm';
  buf[n] = 0;
  outs(buf);
  EmAttr = attr;
}

static void
em_cs(int cs)
{
  if (cs == EmCS) return;
  if (cs == VT_CS_ASCII) outc(0x0f);	/* SI:  GL = G0 */
  else if (cs == VT_CS_DRCS) outc(0x0e);/* SO:  GL = G1 */
  else outs("\033n");			/* LS2: GL = G2 */
  EmCS = cs;
}

static void
em_cell(VCell *c)
{
  em_attr(c->attr);
  em_cs(c->cs);
  outc(c->ch);
}

void
vt_present(void)
{
  int y, x, r, gap;
  char buf[24];

  if (VtHeadless) {
    memcpy(Sent, Cur, sizeof Sent);
    return;
  }

  if (Winched) {			/* emulator window resized */
    Winched = 0;
    screen_size();
    NeedFull = 1;
  }

  if (NeedFull) {
    memset(Sent, 0xff, sizeof Sent);
    outs("\033[2J");
    NeedFull = 0;
    EmAttr = -1;
    EmCS = -1;
  }
  for (y = 0; y < ScrH; y++) {
    x = 0;
    while (x < ScrW) {
      if (!memcmp(&Cur[y][x], &Sent[y][x], sizeof(VCell))) { x++; continue; }
      sprintf(buf, "\033[%d;%dH", y + 1, x + 1);
      outs(buf);
      for (r = x; r < ScrW; r++) {
	if (!memcmp(&Cur[y][r], &Sent[y][r], sizeof(VCell))) {
	  gap = 0;
	  while (r + gap < ScrW && gap < 6 &&
		 !memcmp(&Cur[y][r + gap], &Sent[y][r + gap], sizeof(VCell)))
	    gap++;
	  if (r + gap >= ScrW || gap >= 6) break;
	}
	em_cell(&Cur[y][r]);
	Sent[y][r] = Cur[y][r];
      }
      x = r;
    }
  }
  em_attr(0);
  em_cs(VT_CS_ASCII);
  oflush();
}

/* ---- screenshot (headless testing) ------------------------------------------ */

void
vt_shot(const char *path)
{
  FILE *f = fopen(path, "w");
  int y, x;

  if (!f) return;
  for (y = 0; y < ScrH; y++) {
    for (x = 0; x < ScrW; x++) {
      int c = Cur[y][x].prev;
      if (c < 32 || c > 126) c = ' ';
      fputc(c, f);
    }
    fputc('\n', f);
  }
  fclose(f);
}
