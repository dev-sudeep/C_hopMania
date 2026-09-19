#ifndef RENDER_H
#define RENDER_H

#include "game.h"

/* Terminal dimension limits */
#define MAX_RENDER_COLS 160
#define MAX_RENDER_ROWS 60

/* Cell representation in the off-screen buffer */
typedef struct {
    char ch[8];       /* UTF-8 or ASCII character string */
    char style[48];   /* ANSI style/color escape sequences */
} RenderCell;

/* Render buffer */
typedef struct {
    int width;
    int height;
    RenderCell cells[MAX_RENDER_ROWS][MAX_RENDER_COLS];
} RenderBuffer;

/* Rendering lifecycle */
void render_init(void);
void render_cleanup(void);
void render_frame(const GameState *game, int term_w, int term_h);
void render_force_redraw(void);

/* Color constants / ANSI helpers */
#define ANSI_RESET        "\033[0m"
#define ANSI_BOLD         "\033[1m"
#define ANSI_DIM          "\033[2m"
#define ANSI_ITALIC       "\033[3m"
#define ANSI_UNDERLINE    "\033[4m"
#define ANSI_BLINK        "\033[5m"
#define ANSI_REVERSE      "\033[7m"

/* Standard 16-color foregrounds */
#define ANSI_FG_BLACK     "\033[30m"
#define ANSI_FG_RED       "\033[31m"
#define ANSI_FG_GREEN     "\033[32m"
#define ANSI_FG_YELLOW    "\033[33m"
#define ANSI_FG_BLUE      "\033[34m"
#define ANSI_FG_MAGENTA   "\033[35m"
#define ANSI_FG_CYAN      "\033[36m"
#define ANSI_FG_WHITE     "\033[37m"

#define ANSI_FG_BRED      "\033[91;1m"
#define ANSI_FG_BGREEN    "\033[92;1m"
#define ANSI_FG_BYELLOW   "\033[93;1m"
#define ANSI_FG_BBLUE     "\033[94;1m"
#define ANSI_FG_BMAGENTA  "\033[95;1m"
#define ANSI_FG_BCYAN     "\033[96;1m"
#define ANSI_FG_BWHITE    "\033[97;1m"

/* 256-color / 24-bit presets */
#define ANSI_BG_GRASS     "\033[48;2;34;110;34m"
#define ANSI_BG_GRASS_ALT "\033[48;2;28;95;28m"
#define ANSI_BG_ROAD      "\033[48;2;38;38;42m"
#define ANSI_BG_RIVER     "\033[48;2;16;65;145m"
#define ANSI_BG_LOG       "\033[48;2;115;60;20m"
#define ANSI_BG_TRACKS    "\033[48;2;55;45;40m"
#define ANSI_BG_HUD       "\033[48;2;18;22;30m"
#define ANSI_BG_PANEL     "\033[48;2;24;28;38m"

#endif /* RENDER_H */
