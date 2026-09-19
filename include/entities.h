#ifndef ENTITIES_H
#define ENTITIES_H

#include <stdbool.h>

#define MAX_VEHICLES_PER_ROW 8
#define MAX_LOGS_PER_ROW     8
#define MAX_COINS_PER_ROW    4
#define MAX_PARTICLES        64
#define MAX_OBSTACLES        10

/* Facing directions for chicken */
typedef enum {
    DIR_UP = 0,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

/* Player / Chicken structure */
typedef struct {
    float x;              /* Sub-tile x position for smooth drifting on logs */
    int y;                /* Current row in world coordinates */
    int hop_anim_ticks;   /* Ticks remaining in hop animation */
    Direction facing;     /* Facing direction */
    bool is_alive;        /* Living state */
    bool on_log;          /* Currently standing on a log */
    float drift_vx;       /* Current drift velocity from log */
} Player;

/* Vehicle types */
typedef enum {
    VEHICLE_CAR = 0,
    VEHICLE_SPORT,
    VEHICLE_TRUCK,
    VEHICLE_BUS,
    VEHICLE_COUNT
} VehicleType;

/* Vehicle structure */
typedef struct {
    float x;              /* Left edge x coordinate */
    int length;           /* Width in columns */
    VehicleType type;     /* Type of vehicle */
    int color_id;         /* Color variation index */
    bool active;
} Vehicle;

/* Floating log structure */
typedef struct {
    float x;              /* Left edge x coordinate */
    int length;           /* Length in columns */
    bool has_coin;        /* Coin resting on log */
    int coin_offset;      /* Character offset from left of log */
    bool active;
} Log;

/* Train structure for railroad rows */
typedef struct {
    float x;              /* X position */
    int length;           /* Train length */
    float speed;          /* Super fast */
    bool warning_active;  /* Red warning lights flashing */
    float warning_timer;  /* Seconds of warning remaining */
    bool is_passing;      /* Currently crossing screen */
} Train;

/* Particle for visual effects (splat feathers, water droplets, coin sparkles) */
typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    int life;
    int max_life;
    char ch[8];           /* UTF-8 or ASCII char */
    int color_code;       /* ANSI color */
    bool active;
} Particle;

/* Cause of death */
typedef enum {
    DEATH_NONE = 0,
    DEATH_CAR_HIT,
    DEATH_DROWNED,
    DEATH_SWEPT_AWAY,
    DEATH_EAGLE_SNATCHED,
    DEATH_TRAIN_HIT
} DeathCause;

#endif /* ENTITIES_H */
