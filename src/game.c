#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#define HIGHSCORE_FILE ".c_hop_highscore"

static void init_row(WorldRow *row, int y, RowType type, int lane_idx, int total_lanes);

void game_init(GameState *game) {
    memset(game, 0, sizeof(GameState));
    srand((unsigned int)time(NULL));

    game->mode = STATE_TITLE;
    game->sound_enabled = true;
    game->rows_capacity = 256;
    game->rows = malloc(sizeof(WorldRow) * game->rows_capacity);
    if (!game->rows) {
        perror("Failed to allocate world rows");
        exit(1);
    }

    game_load_highscore(game);
    game_reset(game);
}

void game_reset(GameState *game) {
    game->player.x = (float)(FIELD_WIDTH / 2);
    game->player.y = 0;
    game->player.hop_anim_ticks = 0;
    game->player.facing = DIR_UP;
    game->player.is_alive = true;
    game->player.on_log = false;
    game->player.drift_vx = 0.0f;

    game->camera_y = 0.0f;
    game->score = 0;
    game->coins_collected = 0;
    game->idle_timer = IDLE_TIME_LIMIT;
    game->eagle_active = false;
    game->eagle_y = -10.0f;
    game->death_cause = DEATH_NONE;
    game->death_timer_ticks = 0;
    game->particle_count = 0;
    game->max_generated_y = -1;

    /* Generate starting rows */
    game_ensure_rows_up_to(game, VISIBLE_ROWS + 15);
}

void game_free(GameState *game) {
    if (game->rows) {
        free(game->rows);
        game->rows = NULL;
    }
}

WorldRow* game_get_row(GameState *game, int world_y) {
    if (world_y < 0) return NULL;
    game_ensure_rows_up_to(game, world_y);
    return &game->rows[world_y];
}

void game_ensure_rows_up_to(GameState *game, int target_y) {
    if (target_y >= game->rows_capacity) {
        int new_cap = game->rows_capacity * 2;
        while (new_cap <= target_y) new_cap *= 2;
        WorldRow *new_rows = realloc(game->rows, sizeof(WorldRow) * new_cap);
        if (!new_rows) {
            perror("Failed to realloc rows");
            return;
        }
        game->rows = new_rows;
        game->rows_capacity = new_cap;
    }

    while (game->max_generated_y < target_y) {
        int next_y = game->max_generated_y + 1;

        if (next_y < INITIAL_SAFE_ROWS) {
            /* Initial starting safe grass */
            init_row(&game->rows[next_y], next_y, ROW_GRASS, 0, 1);
            game->max_generated_y = next_y;
        } else {
            /* Generate clusters: Road (1-5 lanes), River (1-4 lanes), Railroad (1-2 lanes), or Grass */
            int roll = rand() % 100;
            RowType cluster_type;
            int num_lanes = 1;

            if (roll < 45) {
                /* Road: up to 5 lanes wide! */
                cluster_type = ROW_ROAD;
                num_lanes = 1 + (rand() % 5);
            } else if (roll < 80) {
                /* River: 1 to 4 lanes wide */
                cluster_type = ROW_RIVER;
                num_lanes = 1 + (rand() % 4);
            } else if (roll < 88) {
                /* Railroad track */
                cluster_type = ROW_RAILROAD;
                num_lanes = 1 + (rand() % 2);
            } else {
                /* Safe Grass rest area */
                cluster_type = ROW_GRASS;
                num_lanes = 1 + (rand() % 3);
            }

            /* Initialize cluster lanes */
            for (int l = 0; l < num_lanes; l++) {
                int curr_y = next_y + l;
                if (curr_y >= game->rows_capacity) {
                    game_ensure_rows_up_to(game, curr_y + 16);
                }
                init_row(&game->rows[curr_y], curr_y, cluster_type, l, num_lanes);
                game->max_generated_y = curr_y;
            }

            /* Always place at least 1 safe grass row after road or river clusters */
            int buffer_grass_y = game->max_generated_y + 1;
            if (buffer_grass_y >= game->rows_capacity) {
                game_ensure_rows_up_to(game, buffer_grass_y + 16);
            }
            init_row(&game->rows[buffer_grass_y], buffer_grass_y, ROW_GRASS, 0, 1);
            game->max_generated_y = buffer_grass_y;
        }
    }
}

static void init_row(WorldRow *row, int y, RowType type, int lane_idx, int total_lanes) {
    memset(row, 0, sizeof(WorldRow));
    row->world_y = y;
    row->type = type;
    row->lane_index = lane_idx;
    row->total_lanes = total_lanes;

    if (type == ROW_GRASS) {
        /* Grass obstacles and coins */
        for (int x = 1; x < FIELD_WIDTH - 1; x++) {
            if (y > 2 && (rand() % 100) < 14) {
                /* Place tree or rock, but avoid blocking too much */
                int obs_roll = rand() % 100;
                if (obs_roll < 60) row->obstacles[x] = OBSTACLE_TREE;
                else if (obs_roll < 85) row->obstacles[x] = OBSTACLE_ROCK;
                else row->obstacles[x] = OBSTACLE_FLOWER;
            } else if ((rand() % 100) < 8) {
                /* Collectible corn / coin */
                row->coins[x] = true;
            }
        }
        /* Always keep center lane clear at start */
        if (y < 4) {
            row->obstacles[FIELD_WIDTH / 2] = OBSTACLE_NONE;
            row->obstacles[FIELD_WIDTH / 2 - 1] = OBSTACLE_NONE;
            row->obstacles[FIELD_WIDTH / 2 + 1] = OBSTACLE_NONE;
        }
    }
    else if (type == ROW_ROAD) {
        /* Alternating direction across lanes, or random */
        row->direction = (lane_idx % 2 == 0) ? 1 : -1;
        /* Varying speeds: 7.0f to 22.0f */
        float base_speed = 8.0f + (float)(rand() % 12);
        if (lane_idx == 0 || lane_idx == total_lanes - 1) {
            base_speed += 2.0f;
        }
        row->speed = base_speed;

        /* Spawn 2 to 4 vehicles per road lane */
        int vcount = 2 + (rand() % 2);
        row->num_vehicles = vcount;

        float spacing = (float)FIELD_WIDTH / vcount;
        for (int i = 0; i < vcount; i++) {
            Vehicle *v = &row->vehicles[i];
            v->active = true;
            v->type = (VehicleType)(rand() % VEHICLE_COUNT);
            switch (v->type) {
                case VEHICLE_SPORT:
                    v->length = 3;
                    v->color_id = 1; /* Bright Red / Magenta */
                    break;
                case VEHICLE_TRUCK:
                    v->length = 7;
                    v->color_id = 2; /* Cyan / Blue */
                    break;
                case VEHICLE_BUS:
                    v->length = 6;
                    v->color_id = 3; /* Yellow */
                    break;
                case VEHICLE_CAR:
                default:
                    v->length = 4;
                    v->color_id = 4; /* Green / Orange */
                    break;
            }
            v->x = i * spacing + (rand() % 4);
        }
    }
    else if (type == ROW_RIVER) {
        row->direction = (lane_idx % 2 == 0) ? 1 : -1;
        row->speed = 6.0f + (float)(rand() % 8);

        /* Spawn logs: 2 to 3 floating logs */
        int log_count = 2 + (rand() % 2);
        row->num_logs = log_count;

        float spacing = (float)FIELD_WIDTH / log_count;
        for (int i = 0; i < log_count; i++) {
            Log *lg = &row->logs[i];
            lg->active = true;
            /* Log lengths: 5 to 9 characters */
            lg->length = 5 + (rand() % 5);
            lg->x = i * spacing + (rand() % 3);
            if ((rand() % 100) < 25) {
                lg->has_coin = true;
                lg->coin_offset = lg->length / 2;
            } else {
                lg->has_coin = false;
            }
        }
    }
    else if (type == ROW_RAILROAD) {
        row->direction = (rand() % 2 == 0) ? 1 : -1;
        row->train.speed = 45.0f;
        row->train.length = 26;
        row->train.x = (row->direction > 0) ? -35.0f : (FIELD_WIDTH + 35.0f);
        row->train.warning_timer = 4.0f + (float)(rand() % 5);
        row->train.warning_active = false;
        row->train.is_passing = false;
    }
}

void game_spawn_particles(GameState *game, float x, float y, int count, int type) {
    for (int i = 0; i < count; i++) {
        if (game->particle_count >= MAX_PARTICLES) {
            /* Overwrite oldest particle */
            for (int j = 0; j < MAX_PARTICLES - 1; j++) {
                game->particles[j] = game->particles[j + 1];
            }
            game->particle_count = MAX_PARTICLES - 1;
        }

        Particle *p = &game->particles[game->particle_count++];
        p->active = true;
        p->x = x;
        p->y = y;
        p->vx = ((float)(rand() % 200) - 100.0f) / 40.0f;
        p->vy = ((float)(rand() % 200) - 100.0f) / 40.0f;
        p->life = 12 + (rand() % 10);
        p->max_life = p->life;

        if (type == 1) {
            /* Feather / Car splat */
            const char *feathers[] = {"*", "o", "+", "x"};
            strncpy(p->ch, feathers[rand() % 4], 3);
            p->color_code = (rand() % 2 == 0) ? 93 : 91; /* Yellow or Red */
        } else if (type == 2) {
            /* Water splash */
            const char *drops[] = {"~", ".", "*", "o"};
            strncpy(p->ch, drops[rand() % 4], 3);
            p->color_code = (rand() % 2 == 0) ? 96 : 94; /* Cyan or Blue */
        } else if (type == 3) {
            /* Coin sparkle */
            const char *sparkles[] = {"*", "$", "+", "^"};
            strncpy(p->ch, sparkles[rand() % 4], 3);
            p->color_code = 93; /* Bright Yellow/Gold */
        } else {
            /* Generic */
            strcpy(p->ch, ".");
            p->color_code = 97;
        }
    }
}

void game_handle_input(GameState *game, KeyInput key) {
    if (key == KEY_NONE) return;

    if (game->mode == STATE_TITLE) {
        if (key == KEY_ACTION || key == KEY_FORWARD || key == KEY_RESTART) {
            game_reset(game);
            game->mode = STATE_PLAYING;
        } else if (key == KEY_QUIT) {
            exit(0);
        }
        return;
    }

    if (game->mode == STATE_GAMEOVER) {
        if (key == KEY_RESTART || key == KEY_ACTION || key == KEY_FORWARD) {
            game_reset(game);
            game->mode = STATE_PLAYING;
        } else if (key == KEY_QUIT) {
            exit(0);
        }
        return;
    }

    if (key == KEY_PAUSE) {
        if (game->mode == STATE_PLAYING) game->mode = STATE_PAUSED;
        else if (game->mode == STATE_PAUSED) game->mode = STATE_PLAYING;
        return;
    }

    if (game->mode == STATE_PAUSED) {
        if (key == KEY_QUIT) exit(0);
        return;
    }

    if (key == KEY_QUIT) {
        exit(0);
    }

    if (key == KEY_RESTART) {
        game_reset(game);
        return;
    }

    if (!game->player.is_alive) return;

    int cur_px = (int)roundf(game->player.x);
    int target_x = cur_px;
    int target_y = game->player.y;
    Direction new_facing = game->player.facing;

    switch (key) {
        case KEY_FORWARD: /* 'w' or Up arrow */
            target_y++;
            new_facing = DIR_UP;
            break;
        case KEY_BACKWARD: /* 's' or Down arrow */
            if (target_y > 0 && target_y > (int)(game->camera_y - 1)) {
                target_y--;
            }
            new_facing = DIR_DOWN;
            break;
        case KEY_MOVE_LEFT: /* 'a' or Left arrow */
            if (target_x > 1) {
                target_x--;
            }
            new_facing = DIR_LEFT;
            break;
        case KEY_MOVE_RIGHT: /* 'd' or Right arrow */
            if (target_x < FIELD_WIDTH - 2) {
                target_x++;
            }
            new_facing = DIR_RIGHT;
            break;
        default:
            break;
    }

    game->player.facing = new_facing;

    /* Check obstacle on target tile */
    WorldRow *target_row = game_get_row(game, target_y);
    if (target_row && target_row->type == ROW_GRASS) {
        if (target_row->obstacles[target_x] == OBSTACLE_TREE || 
            target_row->obstacles[target_x] == OBSTACLE_ROCK) {
            /* Blocked by solid obstacle! */
            return;
        }
    }

    /* Hop succeeded */
    game->player.x = (float)target_x;
    game->player.y = target_y;
    game->player.hop_anim_ticks = 4;

    /* Check if coin collected */
    if (target_row && target_row->type == ROW_GRASS && target_row->coins[target_x]) {
        target_row->coins[target_x] = false;
        game->coins_collected++;
        game_spawn_particles(game, (float)target_x, (float)target_y, 4, 3);
    }

    /* Score increment when reaching new maximum forward distance */
    if (game->player.y > game->score) {
        game->score = game->player.y;
        game->idle_timer = IDLE_TIME_LIMIT; /* Reset stagnation timer */
        if (game->score > game->high_score) {
            game->high_score = game->score;
            game_save_highscore(game);
        }
    }
}

void game_update(GameState *game, float dt) {
    game->frame_count++;

    /* Update particles */
    for (int i = 0; i < game->particle_count; i++) {
        Particle *p = &game->particles[i];
        if (!p->active) continue;
        p->x += p->vx * dt * 20.0f;
        p->y += p->vy * dt * 20.0f;
        p->life--;
        if (p->life <= 0) {
            p->active = false;
        }
    }
    /* Compact inactive particles */
    int active_particles = 0;
    for (int i = 0; i < game->particle_count; i++) {
        if (game->particles[i].active) {
            game->particles[active_particles++] = game->particles[i];
        }
    }
    game->particle_count = active_particles;

    if (game->mode != STATE_PLAYING) return;

    /* Update rows within visible range */
    int start_y = (int)game->camera_y - 2;
    if (start_y < 0) start_y = 0;
    int end_y = (int)game->camera_y + VISIBLE_ROWS + 5;
    game_ensure_rows_up_to(game, end_y);

    for (int y = start_y; y <= end_y; y++) {
        WorldRow *row = &game->rows[y];
        if (row->type == ROW_ROAD) {
            /* Update vehicles */
            for (int v = 0; v < row->num_vehicles; v++) {
                Vehicle *veh = &row->vehicles[v];
                veh->x += row->direction * row->speed * dt;
                /* Wrap around */
                if (row->direction > 0 && veh->x > FIELD_WIDTH + 4) {
                    veh->x = -veh->length - 2.0f;
                } else if (row->direction < 0 && veh->x < -veh->length - 4) {
                    veh->x = FIELD_WIDTH + 2.0f;
                }
            }
        }
        else if (row->type == ROW_RIVER) {
            /* Update logs */
            for (int l = 0; l < row->num_logs; l++) {
                Log *lg = &row->logs[l];
                lg->x += row->direction * row->speed * dt;
                /* Wrap around */
                if (row->direction > 0 && lg->x > FIELD_WIDTH + 6) {
                    lg->x = -lg->length - 2.0f;
                } else if (row->direction < 0 && lg->x < -lg->length - 6) {
                    lg->x = FIELD_WIDTH + 2.0f;
                }
            }
        }
        else if (row->type == ROW_RAILROAD) {
            /* Update railroad */
            Train *tr = &row->train;
            tr->warning_timer -= dt;
            if (tr->warning_timer <= 2.0f && tr->warning_timer > 0.0f) {
                tr->warning_active = true;
            } else if (tr->warning_timer <= 0.0f) {
                tr->is_passing = true;
                tr->x += row->direction * tr->speed * dt;
                if ((row->direction > 0 && tr->x > FIELD_WIDTH + 30) ||
                    (row->direction < 0 && tr->x < -tr->length - 30)) {
                    /* Train has passed, reset timer */
                    tr->is_passing = false;
                    tr->warning_active = false;
                    tr->warning_timer = 5.0f + (float)(rand() % 6);
                    tr->x = (row->direction > 0) ? -35.0f : (FIELD_WIDTH + 35.0f);
                }
            }
        }
    }

    /* Decrement hop anim ticks */
    if (game->player.hop_anim_ticks > 0) {
        game->player.hop_anim_ticks--;
    }

    /* Player logic if alive */
    if (game->player.is_alive) {
        WorldRow *cur_row = game_get_row(game, game->player.y);

        /* River log riding and drowning check */
        if (cur_row && cur_row->type == ROW_RIVER) {
            bool found_log = false;
            float px = game->player.x;

            for (int l = 0; l < cur_row->num_logs; l++) {
                Log *lg = &cur_row->logs[l];
                /* Check if player center is on log */
                if (px >= lg->x - 0.4f && px <= lg->x + lg->length - 0.6f) {
                    found_log = true;
                    game->player.on_log = true;
                    /* Drift with log */
                    float drift = cur_row->direction * cur_row->speed * dt;
                    game->player.x += drift;
                    game->player.drift_vx = drift;

                    /* Coin on log pickup */
                    if (lg->has_coin && fabsf(px - (lg->x + lg->coin_offset)) < 0.8f) {
                        lg->has_coin = false;
                        game->coins_collected++;
                        game_spawn_particles(game, game->player.x, (float)game->player.y, 4, 3);
                    }
                    break;
                }
            }

            if (!found_log) {
                /* DROWNED IN WATER! */
                game->player.is_alive = false;
                game->player.on_log = false;
                game->death_cause = DEATH_DROWNED;
                game_spawn_particles(game, game->player.x, (float)game->player.y, 16, 2);
            } else {
                /* Check if swept off left or right edge */
                if (game->player.x < 1.0f || game->player.x >= FIELD_WIDTH - 2.0f) {
                    game->player.is_alive = false;
                    game->death_cause = DEATH_SWEPT_AWAY;
                    game_spawn_particles(game, game->player.x, (float)game->player.y, 12, 2);
                }
            }
        } else {
            game->player.on_log = false;
            game->player.drift_vx = 0.0f;
        }

        /* Road vehicle collision check */
        if (cur_row && cur_row->type == ROW_ROAD) {
            float px = game->player.x;
            for (int v = 0; v < cur_row->num_vehicles; v++) {
                Vehicle *veh = &cur_row->vehicles[v];
                /* Hitbox check: player occupies roughly [px - 0.3, px + 0.3] */
                if (px >= veh->x - 0.3f && px <= veh->x + veh->length - 0.3f) {
                    /* SPLAT! */
                    game->player.is_alive = false;
                    game->death_cause = DEATH_CAR_HIT;
                    game_spawn_particles(game, game->player.x, (float)game->player.y, 18, 1);
                    break;
                }
            }
        }

        /* Railroad train collision check */
        if (cur_row && cur_row->type == ROW_RAILROAD) {
            Train *tr = &cur_row->train;
            if (tr->is_passing) {
                float px = game->player.x;
                if (px >= tr->x - 0.5f && px <= tr->x + tr->length - 0.5f) {
                    game->player.is_alive = false;
                    game->death_cause = DEATH_TRAIN_HIT;
                    game_spawn_particles(game, game->player.x, (float)game->player.y, 22, 1);
                }
            }
        }

        /* Idle stagnation timer */
        game->idle_timer -= dt;
        if (game->idle_timer <= 0.0f && !game->eagle_active) {
            game->eagle_active = true;
            game->eagle_x = game->player.x;
            game->eagle_y = game->camera_y + VISIBLE_ROWS + 4.0f;
            game->eagle_speed = 32.0f;
        }

        /* Eagle swooping down */
        if (game->eagle_active) {
            game->eagle_y -= game->eagle_speed * dt;
            if (game->eagle_y <= (float)game->player.y + 0.5f) {
                /* Eagle caught the chicken! */
                game->player.is_alive = false;
                game->death_cause = DEATH_EAGLE_SNATCHED;
                game_spawn_particles(game, game->player.x, (float)game->player.y, 14, 1);
            }
        }

        /* If camera moves past player or player is below bottom screen */
        if (game->player.y < (int)(game->camera_y - 1.0f)) {
            game->player.is_alive = false;
            game->death_cause = DEATH_EAGLE_SNATCHED;
            game_spawn_particles(game, game->player.x, (float)game->player.y, 12, 1);
        }
    } else {
        /* Player is dead: count death timer before switching to gameover screen */
        game->death_timer_ticks++;
        if (game->death_timer_ticks > 40) { /* ~1.3 seconds death delay */
            game->mode = STATE_GAMEOVER;
        }
    }

    /* Smooth camera scrolling */
    /* Target camera Y keeps the chicken around row 4-5 from the bottom */
    float target_cam_y = (float)game->player.y - 4.0f;
    if (target_cam_y < 0.0f) target_cam_y = 0.0f;

    /* Camera creeps forward slightly or follows player */
    if (target_cam_y > game->camera_y) {
        game->camera_y += (target_cam_y - game->camera_y) * 9.0f * dt;
    } else {
        /* Very slow follow backward to give leeway */
        game->camera_y += (target_cam_y - game->camera_y) * 2.5f * dt;
    }
}

void game_load_highscore(GameState *game) {
    game->high_score = 0;
    const char *home = getenv("HOME");
    char path[256];
    if (home) {
        snprintf(path, sizeof(path), "%s/%s", home, HIGHSCORE_FILE);
    } else {
        snprintf(path, sizeof(path), "./%s", HIGHSCORE_FILE);
    }

    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%d", &game->high_score) != 1) {
            game->high_score = 0;
        }
        fclose(f);
    }
}

void game_save_highscore(GameState *game) {
    const char *home = getenv("HOME");
    char path[256];
    if (home) {
        snprintf(path, sizeof(path), "%s/%s", home, HIGHSCORE_FILE);
    } else {
        snprintf(path, sizeof(path), "./%s", HIGHSCORE_FILE);
    }

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d\n", game->high_score);
        fclose(f);
    }
}
