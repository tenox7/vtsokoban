/* VT Sokoban -- DEC VT DRCS edition.
 *
 * The whole screen is drawn with a downloadable soft font (DRCS): map cells are
 * 16x16 tiles, and so are the title logo, the seven-segment counters and the
 * key caps -- each is a run of soft-font characters.  Only the plain labels are
 * the terminal's own font.  No curses; the backend (vt_term.c) downloads the
 * font once and diffs a cell buffer to the screen.  See vt.h / soko_tiles.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vt.h"
#include "soko_tiles.h"
#include "embedded_levels.h"
#include "levels.h"

typedef struct {
    char** map;
    int width;
    int height;
    int player_x;
    int player_y;
    int boxes_total;
    int boxes_on_goal;
} Game;

int current_level = 0;
int num_levels = 0;

/* map character -> tile index */
static int tile_for(char ch)
{
    switch (ch) {
    case WALL:          return TILE_WALL;
    case BOX:           return TILE_BOX;
    case BOX_ON_GOAL:   return TILE_BOX_ON_GOAL;
    case GOAL:          return TILE_GOAL;
    case PLAYER:        return TILE_PLAYER;
    case PLAYER_ON_GOAL: return TILE_PLAYER_ON_GOAL;
    default:            return TILE_FLOOR;
    }
}

/* Load a level from embedded data (unchanged from the curses version). */
static char** load_level(int level_index, int* width, int* height,
                         int* player_x, int* player_y, int* boxes)
{
    const char* level_data;
    const char* ptr;
    int max_width = 0, num_lines = 0;
    char** map;
    int line_idx, i, j, len;
    const char* line_start;

    level_data = embedded_levels[level_index].data;

    line_start = ptr = level_data;
    while (*ptr) {
        if (*ptr == '\n') {
            len = ptr - line_start;
            if (len > max_width) max_width = len;
            num_lines++;
            line_start = ptr + 1;
        }
        ptr++;
    }
    if (ptr > line_start) {
        len = ptr - line_start;
        if (len > max_width) max_width = len;
        num_lines++;
    }

    map = (char**)malloc(num_lines * sizeof(char*));
    for (i = 0; i < num_lines; i++) {
        map[i] = (char*)malloc((max_width + 1) * sizeof(char));
        for (j = 0; j < max_width; j++) map[i][j] = EMPTY;
        map[i][max_width] = '\0';
    }

    *boxes = 0;
    line_idx = 0;
    line_start = ptr = level_data;
    while (*ptr) {
        if (*ptr == '\n' || *(ptr + 1) == '\0') {
            len = ptr - line_start;
            if (*ptr != '\n' && *(ptr + 1) == '\0') len++;
            for (i = 0; i < len; i++) {
                map[line_idx][i] = line_start[i];
                if (line_start[i] == BOX || line_start[i] == BOX_ON_GOAL)
                    (*boxes)++;
                if (line_start[i] == PLAYER || line_start[i] == PLAYER_ON_GOAL) {
                    *player_x = i;
                    *player_y = line_idx;
                }
            }
            line_idx++;
            line_start = ptr + 1;
        }
        ptr++;
    }

    *width = max_width;
    *height = num_lines;
    return map;
}

static void free_map(char** map, int height)
{
    int i;
    for (i = 0; i < height; i++) free(map[i]);
    free(map);
}

#define URL "github.com/tenox7/vtsokoban"
#define MOVES "MOVE: ARROWS / WASD / HJKL"
#define MAXPIPS 10          /* past this the pip row is wider than it is useful */

static const struct { char key; const char* label; } Keys[] = {
    { 'R', "RESTART" }, { 'N', "NEXT" }, { 'P', "PREV" },
    { 'C', "REDRAW" }, { 'Q', "QUIT" }
};
#define NKEYS (int)(sizeof Keys / sizeof Keys[0])

/* Everything under the board is centred on the screen, not on the board, so
 * the panel holds still while levels change width. */
static int centre(int w)
{
    int x = (ScrW - w) / 2;
    return x < 0 ? 0 : x;
}

static int pips_of(const Game* game)
{
    return game->boxes_total <= MAXPIPS ? game->boxes_total : 0;
}

/* LEVEL 01/24    BOXES 0/4 (o)(o)[x][x]  -- labels are engraved plates, the
 * counts are seven-segment glyphs, and each box is a goal ring that becomes a
 * crate as you fill it. */
static void draw_status(const Game* game, int row)
{
    char lv[16], bx[16];
    int i, x, np = pips_of(game);

    snprintf(lv, sizeof lv, "%02d/%02d", current_level + 1, num_levels);
    snprintf(bx, sizeof bx, "%d/%d", game->boxes_on_goal, game->boxes_total);

    x = centre(7 + (int)strlen(lv) * 2 + 4 + 7 + (int)strlen(bx) * 2
               + (np ? 1 + np * 2 : 0));
    vt_puts(row, x, " LEVEL ", VA_REV);
    x += 7;
    x += vt_num(row, x, lv, 0);
    x += 4;
    vt_puts(row, x, " BOXES ", VA_REV);
    x += 7;
    x += vt_num(row, x, bx, 0);
    if (!np) return;
    x += 1;
    for (i = 0; i < np; i++)
        vt_tile(row, x + i * 2,
                i < game->boxes_on_goal ? TILE_BOX_ON_GOAL : TILE_GOAL, 0);
}

static void draw_keys(int row)
{
    int i, w = 0, x;

    for (i = 0; i < NKEYS; i++) w += (i ? 2 : 0) + 3 + (int)strlen(Keys[i].label);
    x = centre(w);
    for (i = 0; i < NKEYS; i++) {
        if (i) x += 2;
        x += vt_keycap(row, x, Keys[i].key, 0) + 1;
        vt_puts(row, x, Keys[i].label, 0);
        x += (int)strlen(Keys[i].label);
    }
}

/* A plate with the N key set into it: reverse video knocks the cap out dark,
 * so it reads as engraved rather than as a second, brighter thing. */
static void draw_done(int row)
{
    const char* a = " LEVEL COMPLETE -- PRESS ";
    const char* b = " FOR THE NEXT ONE ";
    int x = centre((int)strlen(a) + 2 + (int)strlen(b));

    vt_puts(row, x, a, VA_REV);
    x += (int)strlen(a);
    x += vt_keycap(row, x, 'N', VA_REV);
    vt_puts(row, x, b, VA_REV);
}

/* Draw the whole board + panel into the cell buffer and present it.  The
 * board is small, so a full redraw every frame is cheaper than tracking
 * dirty cells -- vt_present sends only the cells that actually changed. */
static void draw_board(const Game* game, int complete)
{
    int y, x, sy, row, bh = game->height;

    vt_clear();

    sy = (ScrH - (bh + 9)) / 2;
    if (sy < 0) sy = 0;

    for (y = 0; y < bh; y++)
        for (x = 0; x < game->width; x++)
            vt_tile(sy + y, centre(game->width * 2) + x * 2,
                    tile_for(game->map[y][x]), 0);

    row = sy + bh + 1;
    vt_logo(row, centre(VT_LOGO_W), 0);
    vt_puts(row + 2, centre((int)strlen(URL)), URL, 0);
    draw_status(game, row + 4);
    if (complete) draw_done(row + 5);
    draw_keys(row + 6);
    vt_puts(row + 7, centre((int)strlen(MOVES)), MOVES, 0);

    vt_present();
}

/* Apply a move; returns 1 if the player moved.  Pure game-state mutation --
 * rendering happens in draw_board. */
static int move_player(Game* game, int dx, int dy)
{
    int nx = game->player_x + dx, ny = game->player_y + dy, bx, by;
    char cur;

    if (nx < 0 || nx >= game->width || ny < 0 || ny >= game->height) return 0;
    if (game->map[ny][nx] == WALL) return 0;

    if (game->map[ny][nx] == BOX || game->map[ny][nx] == BOX_ON_GOAL) {
        bx = nx + dx;
        by = ny + dy;
        if (bx < 0 || bx >= game->width || by < 0 || by >= game->height) return 0;
        if (game->map[by][bx] != EMPTY && game->map[by][bx] != GOAL) return 0;

        if (game->map[ny][nx] == BOX_ON_GOAL) {
            game->map[ny][nx] = GOAL;
            game->boxes_on_goal--;
        } else {
            game->map[ny][nx] = EMPTY;
        }
        if (game->map[by][bx] == GOAL) {
            game->map[by][bx] = BOX_ON_GOAL;
            game->boxes_on_goal++;
        } else {
            game->map[by][bx] = BOX;
        }
    }

    cur = (game->map[game->player_y][game->player_x] == PLAYER_ON_GOAL)
          ? GOAL : EMPTY;
    game->map[game->player_y][game->player_x] = cur;
    game->map[ny][nx] = (game->map[ny][nx] == GOAL) ? PLAYER_ON_GOAL : PLAYER;
    game->player_x = nx;
    game->player_y = ny;
    return 1;
}

static void load_into(Game* game, int level)
{
    game->map = load_level(level, &game->width, &game->height,
                           &game->player_x, &game->player_y, &game->boxes_total);
    game->boxes_on_goal = 0;
}

static void show_help(const char* prog)
{
    printf("VT Sokoban - a DEC VT DRCS Sokoban game\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  -h            Show this help and exit\n");
    printf("  -t 320|340|420 Force terminal font geometry (skip DA query)\n");
    printf("  -w 5..15      Override DRCS glyph width (match emulator cells)\n");
    printf("  -noquery      Skip the DA1/DA2 terminal probe\n");
    printf("  -shot FILE    Headless: dump an ASCII render of level 1 and exit\n");
    printf("\nControls: Arrows/WASD/hjkl move, R restart, N next, P prev,\n");
    printf("          C redraw, Q quit\n");
}

int main(int argc, char *argv[])
{
    Game game;
    int running = 1, complete = 0, ch, i;
    int fontsel = 0, gwidth = 0, noquery = 0;
    char* shot_path = NULL;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            show_help(argv[0]);
            return 0;
        } else if (!strcmp(argv[i], "-t") && i + 1 < argc) {
            fontsel = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-w") && i + 1 < argc) {
            gwidth = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-noquery")) {
            noquery = 1;
        } else if (!strcmp(argv[i], "-shot") && i + 1 < argc) {
            shot_path = argv[++i];
            VtHeadless = 1;
        }
    }

    current_level = 0;
    num_levels = NUM_EMBEDDED_LEVELS;
    if (num_levels == 0) {
        fprintf(stderr, "No embedded levels found.\n");
        return 1;
    }

    load_into(&game, current_level);

    if (shot_path) {
        draw_board(&game, 0);
        vt_shot(shot_path);
        free_map(game.map, game.height);
        return 0;
    }

    vt_open(fontsel, gwidth, noquery);
    draw_board(&game, complete);

    while (running) {
        ch = vt_getkey(-1);
        if (ch == VK_NONE) { draw_board(&game, complete); continue; }

        switch (ch) {
        case VK_UP: case 'w': case 'W': case 'k': case 'K':
            if (move_player(&game, 0, -1)) complete = 0;
            break;
        case VK_DOWN: case 's': case 'S': case 'j': case 'J':
            if (move_player(&game, 0, 1)) complete = 0;
            break;
        case VK_LEFT: case 'a': case 'A': case 'h': case 'H':
            if (move_player(&game, -1, 0)) complete = 0;
            break;
        case VK_RIGHT: case 'd': case 'D': case 'l': case 'L':
            if (move_player(&game, 1, 0)) complete = 0;
            break;
        case 'r': case 'R':
            free_map(game.map, game.height);
            load_into(&game, current_level);
            complete = 0;
            break;
        case 'n': case 'N':
            if (complete || current_level < num_levels - 1) {
                free_map(game.map, game.height);
                current_level = complete
                    ? (current_level + 1) % num_levels : current_level + 1;
                load_into(&game, current_level);
                complete = 0;
            }
            break;
        case 'p': case 'P':
            if (current_level > 0) {
                free_map(game.map, game.height);
                current_level--;
                load_into(&game, current_level);
                complete = 0;
            }
            break;
        case 'c': case 'C':
            vt_repaint();
            break;
        case 'q': case 'Q': case 3:
            running = 0;
            continue;
        }

        if (game.boxes_on_goal == game.boxes_total && game.boxes_total > 0)
            complete = 1;
        draw_board(&game, complete);
    }

    vt_close();
    free_map(game.map, game.height);
    return 0;
}
