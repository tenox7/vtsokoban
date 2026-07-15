/* VT Sokoban -- DEC VT DRCS edition.
 *
 * The board is drawn with a downloadable soft font (DRCS): every map cell is
 * one 16x16 tile rendered as two adjacent soft-font characters.  No curses;
 * the terminal backend (vt_term.c) downloads the tiles once and diffs a cell
 * buffer to the screen.  See vt.h / soko_tiles.h.
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
    char* level_name;
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

/* Draw the whole board + status into the cell buffer and present it.  The
 * board is small, so a full redraw every frame is cheaper than tracking
 * dirty cells -- vt_present sends only the cells that actually changed. */
static void draw_board(const Game* game, int complete)
{
    int y, x, sy, sx, bw = game->width * 2, bh = game->height;
    char line[128];

    vt_clear();

    sx = (ScrW - bw) / 2;
    sy = (ScrH - bh - 6) / 2;
    if (sx < 0) sx = 0;
    if (sy < 1) sy = 1;

    for (y = 0; y < bh; y++)
        for (x = 0; x < game->width; x++)
            vt_tile(sy + y, sx + x * 2, tile_for(game->map[y][x]), 0);

    y = sy + bh + 1;
    vt_puts(y, sx, "VT SOKOBAN - github.com/tenox7/vtsokoban", VA_BOLD);
    snprintf(line, sizeof line, "Level: %s (%d/%d)",
             game->level_name, current_level + 1, num_levels);
    vt_puts(y + 1, sx, line, 0);
    if (complete)
        vt_puts(y + 2, sx, "Level complete! Press N for next level.", VA_BOLD | VA_REV);
    else {
        snprintf(line, sizeof line, "Boxes: %d/%d",
                 game->boxes_on_goal, game->boxes_total);
        vt_puts(y + 2, sx, line, 0);
    }
    vt_puts(y + 3, sx, "Arrows/WASD/hjkl move", 0);
    vt_puts(y + 4, sx, "[R]estart [N]ext [P]rev [C]lear [Q]uit", 0);

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
    game->level_name = strdup(embedded_levels[level].name);
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
        free(game.level_name);
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
            free(game.level_name);
            load_into(&game, current_level);
            complete = 0;
            break;
        case 'n': case 'N':
            if (complete || current_level < num_levels - 1) {
                free_map(game.map, game.height);
                free(game.level_name);
                current_level = complete
                    ? (current_level + 1) % num_levels : current_level + 1;
                load_into(&game, current_level);
                complete = 0;
            }
            break;
        case 'p': case 'P':
            if (current_level > 0) {
                free_map(game.map, game.height);
                free(game.level_name);
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
    free(game.level_name);
    return 0;
}
