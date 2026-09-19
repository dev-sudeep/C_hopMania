#define _POSIX_C_SOURCE 200809L
#include "terminal.h"
#include "game.h"
#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TARGET_FPS 30
#define FRAME_TIME_NS (1000000000L / TARGET_FPS)

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void print_help(const char *prog) {
    printf("Hop Mania (Crossy Chicken TUI) - ANSI C Replica\n\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Controls:\n");
    printf("  w / Up Arrow    : Hop forward\n");
    printf("  s / Down Arrow  : Hop backward\n");
    printf("  a / Left Arrow  : Hop left\n");
    printf("  d / Right Arrow : Hop right\n");
    printf("  p               : Pause / Resume game\n");
    printf("  r               : Restart game\n");
    printf("  q / ESC         : Quit game\n\n");
    printf("Options:\n");
    printf("  -h, --help      : Show this help message\n");
    printf("  -v, --version   : Display version information\n");
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            printf("Hop Mania v1.0.0 (TUI)\n");
            return 0;
        }
    }

    if (!terminal_init()) {
        fprintf(stderr, "Error: Terminal does not support raw mode or is not a TTY.\n");
        return 1;
    }

    render_init();

    GameState game;
    game_init(&game);

    double last_time = get_time_sec();

    while (1) {
        double current_time = get_time_sec();
        double dt = current_time - last_time;
        if (dt > 0.1) dt = 0.1; /* Clamp dt to prevent huge leaps */
        last_time = current_time;

        /* Poll all queued key inputs */
        KeyInput key;
        int moves_this_frame = 0;
        while ((key = terminal_read_key()) != KEY_NONE) {
            if (key == KEY_FORWARD || key == KEY_BACKWARD ||
                key == KEY_MOVE_LEFT || key == KEY_MOVE_RIGHT) {
                if (moves_this_frame < 1) {
                    game_handle_input(&game, key);
                    moves_this_frame++;
                }
                /* Discard excessive movement buffer buildup from any key repeat or delay */
            } else {
                game_handle_input(&game, key);
            }
        }

        /* Update simulation */
        game_update(&game, (float)dt);

        /* Render frame */
        int term_w, term_h;
        terminal_get_size(&term_w, &term_h);
        render_frame(&game, term_w, term_h);

        /* Frame rate regulation */
        double frame_end = get_time_sec();
        double elapsed = frame_end - current_time;
        double remaining = (1.0 / TARGET_FPS) - elapsed;
        if (remaining > 0) {
            struct timespec req;
            req.tv_sec = (time_t)remaining;
            req.tv_nsec = (long)((remaining - (time_t)remaining) * 1e9);
            nanosleep(&req, NULL);
        }
    }

    game_free(&game);
    render_cleanup();
    terminal_restore();
    return 0;
}
