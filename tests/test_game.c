#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

void test_initialization(void) {
    printf("[TEST] Testing Game Initialization...\n");
    GameState game;
    game_init(&game);

    assert(game.mode == STATE_TITLE);
    assert(game.player.is_alive == true);
    assert(game.player.y == 0);
    assert(game.player.x == (float)(FIELD_WIDTH / 2));
    assert(game.score == 0);

    /* Test starting safe rows */
    for (int y = 0; y < INITIAL_SAFE_ROWS; y++) {
        WorldRow *row = game_get_row(&game, y);
        assert(row != NULL);
        assert(row->type == ROW_GRASS);
    }
    printf("  -> Passed!\n");
    game_free(&game);
}

void test_multi_lane_generation(void) {
    printf("[TEST] Testing Multi-lane Road and River Generation...\n");
    GameState game;
    game_init(&game);

    /* Generate 200 rows to ensure procedural variety */
    game_ensure_rows_up_to(&game, 200);

    int max_road_lanes_found = 0;
    int max_river_lanes_found = 0;
    int total_roads = 0;
    int total_rivers = 0;

    for (int y = 0; y < 200; y++) {
        WorldRow *row = game_get_row(&game, y);
        assert(row != NULL);
        if (row->type == ROW_ROAD) {
            total_roads++;
            if (row->total_lanes > max_road_lanes_found) {
                max_road_lanes_found = row->total_lanes;
            }
            /* Roads must have vehicles and speed */
            assert(row->num_vehicles > 0);
            assert(row->speed > 0);
        } else if (row->type == ROW_RIVER) {
            total_rivers++;
            if (row->total_lanes > max_river_lanes_found) {
                max_river_lanes_found = row->total_lanes;
            }
            /* Rivers must have floating logs */
            assert(row->num_logs > 0);
            assert(row->speed > 0);
        }
    }

    printf("  -> Generated %d road lanes (Max cluster width: %d lanes, up to 5)\n", total_roads, max_road_lanes_found);
    printf("  -> Generated %d river lanes (Max cluster width: %d lanes)\n", total_rivers, max_river_lanes_found);
    assert(max_road_lanes_found >= 2);
    assert(max_road_lanes_found <= 5);
    assert(total_roads > 0);
    assert(total_rivers > 0);
    printf("  -> Passed!\n");
    game_free(&game);
}

void test_movement_and_scoring(void) {
    printf("[TEST] Testing Movement (w, s, a, d) and Scoring...\n");
    GameState game;
    game_init(&game);
    game.mode = STATE_PLAYING;

    int initial_x = (int)game.player.x;
    int initial_y = game.player.y;

    /* Move forward with 'w' */
    game_handle_input(&game, KEY_FORWARD);
    assert(game.player.y == initial_y + 1);
    assert(game.score == initial_y + 1);
    assert(game.player.facing == DIR_UP);

    /* Move backward with 's' */
    game_handle_input(&game, KEY_BACKWARD);
    assert(game.player.y == initial_y);
    assert(game.score == initial_y + 1); /* Score stays at max distance */
    assert(game.player.facing == DIR_DOWN);

    /* Move left with 'a' */
    game_handle_input(&game, KEY_MOVE_LEFT);
    assert((int)game.player.x == initial_x - 1);
    assert(game.player.facing == DIR_LEFT);

    /* Move right with 'd' */
    game_handle_input(&game, KEY_MOVE_RIGHT);
    assert((int)game.player.x == initial_x);
    assert(game.player.facing == DIR_RIGHT);

    printf("  -> Passed!\n");
    game_free(&game);
}

void test_river_log_mechanics(void) {
    printf("[TEST] Testing River and Floating Log Mechanics...\n");
    GameState game;
    game_init(&game);
    game.mode = STATE_PLAYING;

    /* Create a controlled river row at y=1 */
    WorldRow *river_row = game_get_row(&game, 1);
    river_row->type = ROW_RIVER;
    river_row->direction = 1; /* moving right */
    river_row->speed = 10.0f;
    river_row->num_logs = 1;
    river_row->logs[0].active = true;
    river_row->logs[0].x = 20.0f;
    river_row->logs[0].length = 8; /* spans 20 to 28 */
    river_row->logs[0].has_coin = true;
    river_row->logs[0].coin_offset = 3;

    /* Case A: Chicken hops directly onto the log at x=23 */
    game.player.x = 23.0f;
    game.player.y = 0;
    game_handle_input(&game, KEY_FORWARD); /* hop onto y=1, x=23 */
    assert(game.player.y == 1);
    assert(game.player.is_alive == true);

    /* Update game simulation for 0.1 seconds */
    game_update(&game, 0.1f);
    assert(game.player.is_alive == true);
    assert(game.player.on_log == true);
    /* Drifting: should have drifted to the right */
    assert(game.player.x > 23.0f);
    printf("  -> Successfully hopped on log and drifted with current!\n");

    /* Case B: Chicken hops into open water (no log) */
    game.player.x = 5.0f; /* No log here (log is at x=21+) */
    game.player.y = 1;
    game_update(&game, 0.05f);
    assert(game.player.is_alive == false);
    assert(game.death_cause == DEATH_DROWNED);
    printf("  -> Correctly drowned when landing in water without a log!\n");

    game_free(&game);
}

void test_road_vehicle_collision(void) {
    printf("[TEST] Testing Road Vehicle Collision...\n");
    GameState game;
    game_init(&game);
    game.mode = STATE_PLAYING;

    /* Create a controlled road row at y=1 */
    WorldRow *road = game_get_row(&game, 1);
    road->type = ROW_ROAD;
    road->direction = 1;
    road->speed = 15.0f;
    road->num_vehicles = 1;
    road->vehicles[0].active = true;
    road->vehicles[0].x = 22.0f;
    road->vehicles[0].length = 6; /* Spans 22 to 28 */

    /* Player hops into vehicle path at x=24 */
    game.player.x = 24.0f;
    game.player.y = 1;
    game.player.is_alive = true;

    game_update(&game, 0.02f);
    assert(game.player.is_alive == false);
    assert(game.death_cause == DEATH_CAR_HIT);
    printf("  -> Collision accurately detected: SPLAT on road!\n");

    game_free(&game);
}

void test_idle_eagle_timer(void) {
    printf("[TEST] Testing Idle Stagnation and Swooping Eagle...\n");
    GameState game;
    game_init(&game);
    game.mode = STATE_PLAYING;
    game.player.is_alive = true;
    game.idle_timer = 0.5f;

    /* Pass 0.6 seconds without moving */
    game_update(&game, 0.6f);
    assert(game.eagle_active == true);

    /* Eagle swoops down and catches chicken */
    for (int i = 0; i < 30 && game.player.is_alive; i++) {
        game_update(&game, 0.1f);
    }
    assert(game.player.is_alive == false);
    assert(game.death_cause == DEATH_EAGLE_SNATCHED);
    printf("  -> Eagle snatched idle chicken!\n");

    game_free(&game);
}

int main(void) {
    printf("=========================================\n");
    printf("  RUNNING HOP MANIA VERIFICATION SUITE   \n");
    printf("=========================================\n\n");

    test_initialization();
    test_multi_lane_generation();
    test_movement_and_scoring();
    test_river_log_mechanics();
    test_road_vehicle_collision();
    test_idle_eagle_timer();

    printf("\n=========================================\n");
    printf("  ALL UNIT AND INTEGRATION TESTS PASSED! \n");
    printf("=========================================\n");
    return 0;
}
