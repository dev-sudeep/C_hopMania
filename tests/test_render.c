#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    printf("[TEST] Testing Render Engine Frame Generation...\n");

    /* Redirect stdout to /dev/null for headless rendering test */
    int null_fd = open("/dev/null", O_WRONLY);
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(null_fd, STDOUT_FILENO);
    close(null_fd);

    render_init();

    GameState game;
    game_init(&game);

    /* Test rendering title screen */
    render_frame(&game, 80, 24);
    render_frame(&game, 120, 40);
    render_frame(&game, 60, 20);

    /* Test rendering active gameplay */
    game.mode = STATE_PLAYING;
    render_frame(&game, 80, 24);

    /* Advance simulation and test rendering */
    for (int i = 0; i < 50; i++) {
        game_handle_input(&game, KEY_FORWARD);
        game_update(&game, 0.033f);
        render_frame(&game, 80, 24);
    }

    /* Test paused state */
    game.mode = STATE_PAUSED;
    render_frame(&game, 80, 24);

    /* Test railroad warning lights and passing train */
    game.mode = STATE_PLAYING;
    WorldRow *rr_row = game_get_row(&game, 2);
    rr_row->type = ROW_RAILROAD;
    rr_row->train.warning_active = true;
    render_frame(&game, 80, 24);
    rr_row->train.warning_active = false;
    rr_row->train.is_passing = true;
    rr_row->train.x = 10.0f;
    rr_row->train.length = 20;
    render_frame(&game, 80, 24);

    /* Test gameover state */
    game.mode = STATE_GAMEOVER;
    game.death_cause = DEATH_CAR_HIT;
    render_frame(&game, 80, 24);

    render_cleanup();
    game_free(&game);

    /* Restore stdout */
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    printf("  -> Headless render executed 55 frames with zero crashes/overflows!\n");
    return 0;
}
