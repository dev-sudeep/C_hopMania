#ifndef GAME_H
#define GAME_H

#include "entities.h"
#include "terminal.h"
#include <stdbool.h>

#define FIELD_WIDTH       50
#define VISIBLE_ROWS      20
#define INITIAL_SAFE_ROWS 4
#define IDLE_TIME_LIMIT   8.0f

/* Types of terrain rows */
typedef enum {
    ROW_GRASS = 0,
    ROW_ROAD,
    ROW_RIVER,
    ROW_RAILROAD
} RowType;

/* Obstacle types on grass */
typedef enum {
    OBSTACLE_NONE = 0,
    OBSTACLE_TREE,
    OBSTACLE_ROCK,
    OBSTACLE_FLOWER,
    OBSTACLE_MUSHROOM
} ObstacleType;

/* A single horizontal row in the game world */
typedef struct {
    int world_y;
    RowType type;
    int direction;              /* +1 (right) or -1 (left) */
    float speed;                /* Tiles per second */
    int lane_index;             /* 0-indexed within multi-lane cluster */
    int total_lanes;            /* e.g., 1 to 5 for roads */
    
    /* Road data */
    Vehicle vehicles[MAX_VEHICLES_PER_ROW];
    int num_vehicles;

    /* River data */
    Log logs[MAX_LOGS_PER_ROW];
    int num_logs;

    /* Railroad data */
    Train train;

    /* Grass data */
    ObstacleType obstacles[FIELD_WIDTH];
    bool coins[FIELD_WIDTH];
} WorldRow;

/* High-level game states */
typedef enum {
    STATE_TITLE = 0,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAMEOVER
} GameMode;

/* Master Game State */
typedef struct {
    GameMode mode;
    Player player;

    /* World rows ring/dynamic array */
    WorldRow *rows;
    int rows_capacity;
    int max_generated_y;

    /* Camera viewport */
    float camera_y;             /* Smooth floating camera coordinate */
    int score;                  /* Max world_y reached */
    int high_score;
    int coins_collected;

    /* Idle / Eagle timer */
    float idle_timer;
    bool eagle_active;
    float eagle_x;
    float eagle_y;
    float eagle_speed;

    /* Death info */
    DeathCause death_cause;
    int death_timer_ticks;

    /* Particles */
    Particle particles[MAX_PARTICLES];
    int particle_count;

    /* Settings & stats */
    bool sound_enabled;
    unsigned long frame_count;
    char status_msg[64];
    int status_timer;
} GameState;

/* Core functions */
void game_init(GameState *game);
void game_reset(GameState *game);
void game_free(GameState *game);
void game_update(GameState *game, float dt);
void game_handle_input(GameState *game, KeyInput key);
void game_load_highscore(GameState *game);
void game_save_highscore(GameState *game);

/* World generation */
WorldRow* game_get_row(GameState *game, int world_y);
void game_ensure_rows_up_to(GameState *game, int target_y);

/* Particles & FX */
void game_spawn_particles(GameState *game, float x, float y, int count, int type);

#endif /* GAME_H */
