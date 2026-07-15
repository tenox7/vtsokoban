#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>

/* Include the embedded levels and game definitions */
#include "embedded_levels.h"
#include "levels.h"

/* Color constants for Win32 console */
/* Fancy mode - colored backgrounds (reverse video) */
#define COLOR_WALL_FANCY      (BACKGROUND_BLUE | BACKGROUND_INTENSITY | FOREGROUND_WHITE)
#define COLOR_PLAYER_FANCY    (BACKGROUND_GREEN | FOREGROUND_WHITE | FOREGROUND_INTENSITY)
#define COLOR_BOX_FANCY       (BACKGROUND_RED | FOREGROUND_WHITE | FOREGROUND_INTENSITY)
#define COLOR_GOAL_FANCY      (BACKGROUND_RED | BACKGROUND_GREEN | FOREGROUND_WHITE)
#define COLOR_BOX_GOAL_FANCY  (BACKGROUND_RED | BACKGROUND_BLUE | FOREGROUND_WHITE | FOREGROUND_INTENSITY)
#define COLOR_FLOOR_FANCY     (BACKGROUND_BLACK | FOREGROUND_WHITE)

/* ASCII mode - foreground colors only */
#define COLOR_WALL_ASCII      (FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define COLOR_PLAYER_ASCII    (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_BOX_ASCII       (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define COLOR_GOAL_ASCII      (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_BOX_GOAL_ASCII  (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define COLOR_FLOOR_ASCII     (FOREGROUND_WHITE)

#define COLOR_DEFAULT   (FOREGROUND_WHITE)
#define COLOR_TITLE     (FOREGROUND_RED | FOREGROUND_INTENSITY)

/* Define missing constants for older compilers */
#ifndef FOREGROUND_WHITE
#define FOREGROUND_WHITE (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#endif
#ifndef FOREGROUND_BLACK  
#define FOREGROUND_BLACK 0
#endif
#ifndef BACKGROUND_WHITE
#define BACKGROUND_WHITE (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE)
#endif
#ifndef BACKGROUND_BLACK
#define BACKGROUND_BLACK 0
#endif
#ifndef BACKGROUND_CYAN
#define BACKGROUND_CYAN (BACKGROUND_BLUE | BACKGROUND_GREEN)
#endif
#ifndef BACKGROUND_MAGENTA
#define BACKGROUND_MAGENTA (BACKGROUND_RED | BACKGROUND_BLUE)
#endif
#ifndef BACKGROUND_YELLOW
#define BACKGROUND_YELLOW (BACKGROUND_RED | BACKGROUND_GREEN)
#endif

/* Display characters */
#define DISP_WALL_FANCY ' '          /* Space character with colored background */
#define DISP_PLAYER_FANCY '@'        /* @ character for player */
#define DISP_BOX_FANCY '#'           /* # character for boxes */
#define DISP_BOX_ON_GOAL_FANCY '*'   /* * character for box on goal */
#define DISP_GOAL_FANCY '.'          /* . character for goals */

#define DISP_WALL_ASCII_H '-'  /* Horizontal wall */
#define DISP_WALL_ASCII_V '|'  /* Vertical wall */
#define DISP_WALL_ASCII '#'    /* Default wall */
#define DISP_PLAYER_ASCII '@'
#define DISP_BOX_ASCII '#'
#define DISP_BOX_ON_GOAL_ASCII '*'
#define DISP_GOAL_ASCII 'O'

/* Game state */
typedef struct {
    char** map;
    int width;
    int height;
    int player_x;
    int player_y;
    int boxes_total;
    int boxes_on_goal;
    char* level_name;
    int use_ascii_borders;
    int use_colors;
    int use_ascii_mode;
} Game;

/* Global variables */
int current_level = 0;  /* Current level index */
int num_levels = 0;     /* Total number of levels */
int start_y = 0;        /* Start Y position for the map */
int start_x = 0;        /* Start X position for the map */

/* Win32 console handles */
HANDLE hStdout;
HANDLE hStdin;
CONSOLE_SCREEN_BUFFER_INFO csbi;
WORD original_attributes;

/* Function prototypes */
void init_console(void);
void cleanup_console(void);
char** load_level(int level_index, int* width, int* height, int* player_x, int* player_y, int* boxes);
void free_map(char** map, int height);
void draw_map(const Game* game);
int move_player(Game* game, int dx, int dy);
void show_help(const char* program_name);
void set_cursor_position(int x, int y);
void write_char_at(int x, int y, char ch, WORD attributes);
void write_string_at(int x, int y, const char* str, WORD attributes);
void clear_screen(void);
void clear_screen_with_bg(WORD bg_attributes);
char get_wall_char_ascii(const Game* game, int x, int y);
int get_key_input(void);

/* Function to display help */
void show_help(const char* program_name) {
    printf("TTY Sokoban Win32 - a Windows console-based Sokoban game\n");
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help     Show this help message and exit\n");
    printf("  -a, --ascii    Use ASCII characters for walls instead of box drawing characters\n");
    printf("  -b, -bw        Black and white mode (disable colors)\n");
    printf("  -n             Use simple ASCII characters (# @ O) instead of fancy graphics\n");
    printf("\nControls:\n");
    printf("  Arrow keys, WASD, HJKL    Move player\n");
    printf("  R                         Restart current level\n");
    printf("  N                         Next level\n");
    printf("  P                         Previous level\n");
    printf("  C                         Force screen redraw\n");
    printf("  Q                         Quit game\n");
}

/* Initialize Win32 console */
void init_console(void) {
    DWORD mode;
    
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    
    /* Get console info */
    GetConsoleScreenBufferInfo(hStdout, &csbi);
    original_attributes = csbi.wAttributes;
    
    /* Set console mode for input */
    GetConsoleMode(hStdin, &mode);
    SetConsoleMode(hStdin, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
}

/* Cleanup console */
void cleanup_console(void) {
    DWORD mode;
    
    /* Restore original console attributes */
    SetConsoleTextAttribute(hStdout, original_attributes);
    
    /* Restore console mode */
    GetConsoleMode(hStdin, &mode);
    SetConsoleMode(hStdin, mode | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
}

/* Set cursor position */
void set_cursor_position(int x, int y) {
    COORD coord;
    coord.X = (SHORT)x;
    coord.Y = (SHORT)y;
    SetConsoleCursorPosition(hStdout, coord);
}

/* Write character at position with attributes */
void write_char_at(int x, int y, char ch, WORD attributes) {
    DWORD written;
    
    set_cursor_position(x, y);
    SetConsoleTextAttribute(hStdout, attributes);
    WriteConsole(hStdout, &ch, 1, &written, NULL);
}

/* Write string at position with attributes */
void write_string_at(int x, int y, const char* str, WORD attributes) {
    DWORD written;
    
    set_cursor_position(x, y);
    SetConsoleTextAttribute(hStdout, attributes);
    WriteConsole(hStdout, str, (DWORD)strlen(str), &written, NULL);
}

/* Clear screen */
void clear_screen(void) {
    COORD coord = {0, 0};
    DWORD written;
    DWORD size;
    
    GetConsoleScreenBufferInfo(hStdout, &csbi);
    size = csbi.dwSize.X * csbi.dwSize.Y;
    
    FillConsoleOutputCharacter(hStdout, ' ', size, coord, &written);
    FillConsoleOutputAttribute(hStdout, original_attributes, size, coord, &written);
    SetConsoleCursorPosition(hStdout, coord);
}

/* Clear screen with background color */
void clear_screen_with_bg(WORD bg_attributes) {
    COORD coord = {0, 0};
    DWORD written;
    DWORD size;
    
    GetConsoleScreenBufferInfo(hStdout, &csbi);
    size = csbi.dwSize.X * csbi.dwSize.Y;
    
    FillConsoleOutputCharacter(hStdout, ' ', size, coord, &written);
    FillConsoleOutputAttribute(hStdout, bg_attributes, size, coord, &written);
    SetConsoleCursorPosition(hStdout, coord);
}

/* Get appropriate ASCII wall character based on neighbors */
char get_wall_char_ascii(const Game* game, int x, int y) {
    int has_wall_left = (x > 0 && game->map[y][x-1] == WALL);
    int has_wall_right = (x < game->width-1 && game->map[y][x+1] == WALL);
    int has_wall_up = (y > 0 && game->map[y-1][x] == WALL);
    int has_wall_down = (y < game->height-1 && game->map[y+1][x] == WALL);
    
    /* Prioritize horizontal connections */
    if (has_wall_left || has_wall_right) {
        return DISP_WALL_ASCII_H;
    }
    /* Then vertical connections */
    if (has_wall_up || has_wall_down) {
        return DISP_WALL_ASCII_V;
    }
    /* Default to horizontal for isolated walls */
    return DISP_WALL_ASCII_H;
}

/* Get key input */
int get_key_input(void) {
    INPUT_RECORD input_record;
    DWORD events_read;
    
    while (1) {
        ReadConsoleInput(hStdin, &input_record, 1, &events_read);
        
        if (input_record.EventType == KEY_EVENT && input_record.Event.KeyEvent.bKeyDown) {
            WORD key = input_record.Event.KeyEvent.wVirtualKeyCode;
            char ascii = input_record.Event.KeyEvent.uChar.AsciiChar;
            
            switch (key) {
                case VK_UP: return 1; /* Use control char for arrows */
                case VK_DOWN: return 2;
                case VK_LEFT: return 3;
                case VK_RIGHT: return 4;
                case VK_ESCAPE: return 'q';
                default:
                    if (ascii >= 'a' && ascii <= 'z') return ascii - 32; /* Convert to uppercase */
                    if (ascii >= 'A' && ascii <= 'Z') return ascii;
                    break;
            }
        }
    }
}

/* Main function */
int main(int argc, char *argv[]) {
    /* Game state */
    Game game;
    int game_running = 1;
    int level_complete = 0;
    int ch;
    int i;

    /* Initialize level variables */
    current_level = 0;
    num_levels = NUM_EMBEDDED_LEVELS;

    /* Check for command line flags */
    game.use_ascii_borders = 0;
    game.use_colors = 1;  /* Colors enabled by default */
    game.use_ascii_mode = 0;  /* Fancy graphics by default */

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help(argv[0]);
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--ascii") == 0) {
            game.use_ascii_borders = 1;
        }
        if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "-bw") == 0) {
            game.use_colors = 0;  /* Disable colors */
        }
        if (strcmp(argv[i], "-n") == 0) {
            game.use_ascii_mode = 1;  /* Use simple ASCII characters */
        }
    }

    /* Initialize console */
    init_console();

    if (num_levels == 0) {
        cleanup_console();
        fprintf(stderr, "No embedded levels found.\n");
        return EXIT_FAILURE;
    }

    /* Load first level */
    game.map = load_level(current_level, &game.width, &game.height,
                          &game.player_x, &game.player_y, &game.boxes_total);
    game.boxes_on_goal = 0;
    game.level_name = _strdup(embedded_levels[current_level].name);

    /* Do initial full screen draw */
    clear_screen();
    draw_map(&game);
    
    /* Game loop */
    while (game_running) {
        /* Check if level is complete */
        if (game.boxes_on_goal == game.boxes_total) {
            level_complete = 1;
            write_string_at(start_x, start_y + game.height + 3, 
                          "Level complete! Press 'N' for next level.", 
                          game.use_colors ? (FOREGROUND_WHITE | BACKGROUND_GREEN) : COLOR_DEFAULT);
        }

        /* Get input */
        ch = get_key_input();

        /* Process input */
        switch (ch) {
            case 1: /* Up arrow */
            case 'W': /* W key */
            case 'K': /* K key */
                move_player(&game, 0, -1);
                break;
            case 2: /* Down arrow */
            case 'S': /* S key */
            case 'J': /* J key */
                move_player(&game, 0, 1);
                break;
            case 3: /* Left arrow */
            case 'A': /* A key */
            case 'H': /* H key */
                move_player(&game, -1, 0);
                break;
            case 4: /* Right arrow */
            case 'D': /* D key */
            case 'L': /* L key */
                move_player(&game, 1, 0);
                break;
            case 'R':
                /* Restart level */
                free_map(game.map, game.height);
                game.map = load_level(current_level, &game.width, &game.height,
                                      &game.player_x, &game.player_y, &game.boxes_total);
                game.boxes_on_goal = 0;
                clear_screen();
                draw_map(&game);
                break;
            case 'C':
                /* Force a full redraw */
                clear_screen();
                draw_map(&game);
                break;
            case 'N':
                /* Next level */
                if (current_level < num_levels - 1 || level_complete) {
                    free_map(game.map, game.height);
                    free(game.level_name);

                    if (level_complete) {
                        current_level = (current_level + 1) % num_levels;
                        level_complete = 0;
                    } else {
                        current_level++;
                    }

                    game.map = load_level(current_level, &game.width, &game.height,
                                         &game.player_x, &game.player_y, &game.boxes_total);
                    game.boxes_on_goal = 0;
                    game.level_name = _strdup(embedded_levels[current_level].name);
                    
                    clear_screen();
                    draw_map(&game);
                }
                break;
            case 'P':
                /* Previous level */
                if (current_level > 0) {
                    free_map(game.map, game.height);
                    free(game.level_name);
                    current_level--;
                    game.map = load_level(current_level, &game.width, &game.height,
                                         &game.player_x, &game.player_y, &game.boxes_total);
                    game.boxes_on_goal = 0;
                    game.level_name = _strdup(embedded_levels[current_level].name);
                    level_complete = 0;
                    
                    clear_screen();
                    draw_map(&game);
                }
                break;
            case 'Q':
                /* Quit */
                game_running = 0;
                break;
        }
    }

    /* Clean up */
    free_map(game.map, game.height);
    free(game.level_name);
    cleanup_console();

    return EXIT_SUCCESS;
}

/* Load a level from embedded data */
char** load_level(int level_index, int* width, int* height, int* player_x, int* player_y, int* boxes) {
    const char* level_data;
    const char* ptr;
    int max_width = 0;
    int num_lines = 0;
    char** map;
    int line_idx;
    int i, j, len;
    const char* line_start;
    
    if (level_index < 0 || level_index >= NUM_EMBEDDED_LEVELS) {
        cleanup_console();
        fprintf(stderr, "Invalid level index: %d\n", level_index);
        exit(EXIT_FAILURE);
    }

    level_data = embedded_levels[level_index].data;
    ptr = level_data;

    /* Count the number of lines and find the longest line */
    line_start = ptr;
    while (*ptr) {
        if (*ptr == '\n') {
            len = (int)(ptr - line_start);
            if (len > max_width) {
                max_width = len;
            }
            num_lines++;
            line_start = ptr + 1;
        }
        ptr++;
    }

    /* Check for last line without newline */
    if (ptr > line_start) {
        len = (int)(ptr - line_start);
        if (len > max_width) {
            max_width = len;
        }
        num_lines++;
    }

    /* Allocate memory for the map */
    map = (char**)malloc(num_lines * sizeof(char*));
    for (i = 0; i < num_lines; i++) {
        map[i] = (char*)malloc((max_width + 1) * sizeof(char));
        /* Initialize with spaces */
        for (j = 0; j < max_width; j++) {
            map[i][j] = EMPTY;
        }
        map[i][max_width] = '\0';
    }

    /* Parse the map data */
    *boxes = 0;
    line_idx = 0;
    ptr = level_data;

    line_start = ptr;
    while (*ptr) {
        if (*ptr == '\n' || *(ptr + 1) == '\0') {
            /* If it's the last character and not a newline, include it */
            len = (int)(ptr - line_start);
            if (*ptr != '\n' && *(ptr + 1) == '\0') {
                len++;
            }

            /* Copy the line */
            for (i = 0; i < len; i++) {
                map[line_idx][i] = line_start[i];

                /* Count boxes and find player position */
                if (line_start[i] == BOX || line_start[i] == BOX_ON_GOAL) {
                    (*boxes)++;
                }
                if (line_start[i] == PLAYER) {
                    *player_x = i;
                    *player_y = line_idx;
                }
                if (line_start[i] == PLAYER_ON_GOAL) {
                    *player_x = i;
                    *player_y = line_idx;
                }
            }

            line_idx++;
            line_start = ptr + 1;
        }
        ptr++;
    }

    /* Set the width and height */
    *width = max_width;
    *height = num_lines;

    return map;
}

/* Free the map memory */
void free_map(char** map, int height) {
    int i;

    for (i = 0; i < height; i++) {
        free(map[i]);
    }
    free(map);
}

/* Draw the map */
void draw_map(const Game* game) {
    int y, x;
    char ch;
    WORD attributes;
    int screen_width, screen_height;
    char status_line[256];

    /* Get console info */
    GetConsoleScreenBufferInfo(hStdout, &csbi);
    screen_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    screen_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    /* Calculate centering offsets with 3-line top margin */
    start_y = (screen_height - game->height) / 2 + 3;
    start_x = (screen_width - game->width) / 2;

    /* Make sure we don't go off screen */
    start_y = (start_y < 5) ? 5 : start_y;
    start_x = (start_x < 0) ? 0 : start_x;

    /* Clear the screen with appropriate background */
    if (game->use_colors && !game->use_ascii_mode) {
        /* Fancy mode - use dark background */
        clear_screen_with_bg(BACKGROUND_BLACK);
    } else {
        /* ASCII mode or no colors - use default */
        clear_screen();
    }

    /* Draw the map elements */
    for (y = 0; y < game->height; y++) {
        for (x = 0; x < game->width; x++) {
            ch = game->map[y][x];
            attributes = COLOR_DEFAULT;

            /* Apply colors if enabled */
            if (game->use_colors) {
                if (game->use_ascii_mode) {
                    /* ASCII mode - use foreground colors */
                    switch (ch) {
                        case WALL:
                            attributes = COLOR_WALL_ASCII;
                            break;
                        case PLAYER:
                        case PLAYER_ON_GOAL:
                            attributes = COLOR_PLAYER_ASCII;
                            break;
                        case BOX:
                            attributes = COLOR_BOX_ASCII;
                            break;
                        case GOAL:
                            attributes = COLOR_GOAL_ASCII;
                            break;
                        case BOX_ON_GOAL:
                            attributes = COLOR_BOX_GOAL_ASCII;
                            break;
                        case EMPTY:
                            attributes = COLOR_FLOOR_ASCII;
                            break;
                        default:
                            attributes = COLOR_DEFAULT;
                            break;
                    }
                } else {
                    /* Fancy mode - use background colors */
                    switch (ch) {
                        case WALL:
                            attributes = COLOR_WALL_FANCY;
                            break;
                        case PLAYER:
                        case PLAYER_ON_GOAL:
                            attributes = COLOR_PLAYER_FANCY;
                            break;
                        case BOX:
                            attributes = COLOR_BOX_FANCY;
                            break;
                        case GOAL:
                            attributes = COLOR_GOAL_FANCY;
                            break;
                        case BOX_ON_GOAL:
                            attributes = COLOR_BOX_GOAL_FANCY;
                            break;
                        case EMPTY:
                            attributes = COLOR_FLOOR_FANCY;
                            break;
                        default:
                            attributes = COLOR_DEFAULT;
                            break;
                    }
                }
            }
            
            /* Draw map elements with appropriate display characters */
            if (game->use_ascii_mode) {
                /* ASCII mode */
                if (ch == WALL) {
                    write_char_at(start_x + x, start_y + y, get_wall_char_ascii(game, x, y), attributes);
                } else if (ch == PLAYER || ch == PLAYER_ON_GOAL) {
                    write_char_at(start_x + x, start_y + y, DISP_PLAYER_ASCII, attributes);
                } else if (ch == BOX) {
                    write_char_at(start_x + x, start_y + y, DISP_BOX_ASCII, attributes);
                } else if (ch == BOX_ON_GOAL) {
                    write_char_at(start_x + x, start_y + y, DISP_BOX_ON_GOAL_ASCII, attributes);
                } else if (ch == GOAL) {
                    write_char_at(start_x + x, start_y + y, DISP_GOAL_ASCII, attributes);
                } else {
                    write_char_at(start_x + x, start_y + y, ch, attributes);
                }
            } else {
                /* Fancy mode */
                if (ch == WALL) {
                    write_char_at(start_x + x, start_y + y, DISP_WALL_FANCY, attributes);
                } else if (ch == PLAYER || ch == PLAYER_ON_GOAL) {
                    write_char_at(start_x + x, start_y + y, DISP_PLAYER_FANCY, attributes);
                } else if (ch == BOX) {
                    write_char_at(start_x + x, start_y + y, DISP_BOX_FANCY, attributes);
                } else if (ch == BOX_ON_GOAL) {
                    write_char_at(start_x + x, start_y + y, DISP_BOX_ON_GOAL_FANCY, attributes);
                } else if (ch == GOAL) {
                    write_char_at(start_x + x, start_y + y, DISP_GOAL_FANCY, attributes);
                } else {
                    write_char_at(start_x + x, start_y + y, ch, attributes);
                }
            }
        }
    }

    /* Show status info */
    write_string_at(start_x, start_y + game->height + 1, 
                   "NANO SOKOBAN - github.com/tenox7/ttysokoban", 
                   game->use_colors ? COLOR_TITLE : COLOR_DEFAULT);
    
    sprintf(status_line, "Level: %s (%d/%d)", 
            game->level_name, current_level + 1, num_levels);
    write_string_at(start_x, start_y + game->height + 2, status_line, FOREGROUND_WHITE);
    
    sprintf(status_line, "Boxes: %d/%d", game->boxes_on_goal, game->boxes_total);
    write_string_at(start_x, start_y + game->height + 3, status_line, FOREGROUND_WHITE);

    /* Only display legend if there's enough screen space */
    if (start_y + game->height + 6 < screen_height) {
        write_string_at(start_x, start_y + game->height + 4, 
                       "Arrows/WASD move", FOREGROUND_WHITE);
        write_string_at(start_x, start_y + game->height + 5, 
                       "[R]estart, [N]ext, [P]rev, [Q]uit, [C]lear", FOREGROUND_WHITE);
    }
}

/* Move the player */
int move_player(Game* game, int dx, int dy) {
    int new_x = game->player_x + dx;
    int new_y = game->player_y + dy;
    int box_new_x, box_new_y;
    char current_box;
    char current_pos;

    /* Check if new position is within bounds */
    if (new_x < 0 || new_x >= game->width || new_y < 0 || new_y >= game->height) {
        return 0;
    }

    /* Check if new position is a wall */
    if (game->map[new_y][new_x] == WALL) {
        return 0;
    }

    /* Check if new position has a box */
    if (game->map[new_y][new_x] == BOX || game->map[new_y][new_x] == BOX_ON_GOAL) {
        box_new_x = new_x + dx;
        box_new_y = new_y + dy;

        /* Check if box can be pushed */
        if (box_new_x < 0 || box_new_x >= game->width || box_new_y < 0 || box_new_y >= game->height) {
            return 0;
        }

        /* Check if the position the box would be pushed to is free */
        if (game->map[box_new_y][box_new_x] != EMPTY && game->map[box_new_y][box_new_x] != GOAL) {
            return 0;
        }

        /* Push the box */
        current_box = game->map[new_y][new_x];

        /* If the box is on a goal, reveal the goal when the box is moved */
        if (current_box == BOX_ON_GOAL) {
            game->map[new_y][new_x] = GOAL;
            game->boxes_on_goal--;
        } else {
            game->map[new_y][new_x] = EMPTY;
        }

        /* If the box is pushed onto a goal, mark it as such */
        if (game->map[box_new_y][box_new_x] == GOAL) {
            game->map[box_new_y][box_new_x] = BOX_ON_GOAL;
            game->boxes_on_goal++;
        } else {
            game->map[box_new_y][box_new_x] = BOX;
        }
    }

    /* Move the player */
    current_pos = (game->map[game->player_y][game->player_x] == PLAYER_ON_GOAL) ? GOAL : EMPTY;
    game->map[game->player_y][game->player_x] = current_pos;

    if (game->map[new_y][new_x] == GOAL) {
        game->map[new_y][new_x] = PLAYER_ON_GOAL;
    } else {
        game->map[new_y][new_x] = PLAYER;
    }

    game->player_x = new_x;
    game->player_y = new_y;

    /* Redraw the entire map for simplicity */
    draw_map(game);
    
    return 1;
}