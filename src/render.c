#define _POSIX_C_SOURCE 200809L
#include "render.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

static RenderBuffer current_buf;
static RenderBuffer prev_buf;
static bool force_full_redraw = true;
static int last_origin_x = -1;
static int last_origin_y = -1;
static int last_total_w = -1;
static int last_total_h = -1;
static char out_string[262144]; /* 256 KB buffer */

void render_force_redraw(void) {
    force_full_redraw = true;
}

void render_init(void) {
    memset(&current_buf, 0, sizeof(RenderBuffer));
    memset(&prev_buf, 0, sizeof(RenderBuffer));
    force_full_redraw = true;
    last_origin_x = -1;
    last_origin_y = -1;
    last_total_w = -1;
    last_total_h = -1;
}

static void safe_write(const char *buf, size_t len) {
    ssize_t res = write(STDOUT_FILENO, buf, len);
    (void)res;
}

void render_cleanup(void) {
    /* Reset terminal attributes */
    safe_write("\033[0m", 4);
}

/* Helper to set a cell */
static void set_cell(RenderBuffer *buf, int row, int col, const char *ch, const char *style) {
    if (row < 0 || row >= MAX_RENDER_ROWS || col < 0 || col >= MAX_RENDER_COLS) return;
    if (!ch || ch[0] == '\033') return; /* Never allow raw ESC in character content */

    strncpy(buf->cells[row][col].ch, ch, sizeof(buf->cells[row][col].ch) - 1);
    buf->cells[row][col].ch[sizeof(buf->cells[row][col].ch) - 1] = '\0';
    if (style) {
        strncpy(buf->cells[row][col].style, style, sizeof(buf->cells[row][col].style) - 1);
        buf->cells[row][col].style[sizeof(buf->cells[row][col].style) - 1] = '\0';
    } else {
        buf->cells[row][col].style[0] = '\0';
    }
}

/* Helper to draw a string into the buffer */
static void draw_text(RenderBuffer *buf, int row, int col, const char *str, const char *style) {
    int cur_col = col;
    const char *p = str;
    while (*p && cur_col < MAX_RENDER_COLS) {
        /* Skip any embedded ANSI escape sequences so raw escapes are never drawn as text */
        if (*p == '\033') {
            p++;
            if (*p == '[') {
                p++;
                while (*p && !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '~')) {
                    p++;
                }
                if (*p) p++;
            }
            continue;
        }

        char ch_buf[8] = {0};
        int len = 1;
        /* UTF-8 multi-byte handling */
        unsigned char c = (unsigned char)*p;
        if (c >= 0xC0 && c <= 0xDF) len = 2;
        else if (c >= 0xE0 && c <= 0xEF) len = 3;
        else if (c >= 0xF0 && c <= 0xF7) len = 4;

        for (int i = 0; i < len && *(p + i); i++) {
            ch_buf[i] = *(p + i);
        }
        set_cell(buf, row, cur_col, ch_buf, style);
        cur_col++;
        p += len;
    }
}

/* Vehicle sprite drawing */
static void draw_vehicle(RenderBuffer *buf, int screen_row, int base_col, const Vehicle *veh, int dir) {
    int v_start = (int)roundf(veh->x);
    int len = veh->length;

    const char *bg = ANSI_BG_ROAD;
    const char *fg = ANSI_FG_WHITE;

    switch (veh->color_id) {
        case 1: fg = "\033[38;2;255;70;90;1m"; break;  /* Crimson Sport */
        case 2: fg = "\033[38;2;60;180;255;1m"; break; /* Electric Cyan */
        case 3: fg = "\033[38;2;255;220;40;1m"; break; /* Taxi Yellow */
        case 4: fg = "\033[38;2;50;230;120;1m"; break; /* Lime Van */
        default: fg = "\033[38;2;240;140;40;1m"; break;/* Orange Truck */
    }

    char style_str[96];
    snprintf(style_str, sizeof(style_str), "%s%s", bg, fg);

    for (int i = 0; i < len; i++) {
        int col = base_col + v_start + i;
        if (col < base_col || col >= base_col + FIELD_WIDTH) continue;

        const char *part = "█";
        if (veh->type == VEHICLE_SPORT) {
            if (dir > 0) {
                if (i == 0) part = "▰";
                else if (i == len - 1) part = "►";
                else part = "█";
            } else {
                if (i == 0) part = "◄";
                else if (i == len - 1) part = "▰";
                else part = "█";
            }
        } else if (veh->type == VEHICLE_TRUCK) {
            if (dir > 0) {
                if (i == len - 1) part = "►";
                else if (i == 0) part = "▓";
                else if (i == len - 2) part = "█";
                else part = "▓";
            } else {
                if (i == 0) part = "◄";
                else if (i == len - 1) part = "▓";
                else if (i == 1) part = "█";
                else part = "▓";
            }
        } else if (veh->type == VEHICLE_BUS) {
            if (dir > 0) {
                if (i == len - 1) part = "►";
                else if (i == 0) part = "■";
                else if (i % 2 == 1) part = "░";
                else part = "■";
            } else {
                if (i == 0) part = "◄";
                else if (i == len - 1) part = "■";
                else if (i % 2 == 1) part = "░";
                else part = "■";
            }
        } else {
            /* Standard car */
            if (dir > 0) {
                if (i == 0) part = "▰";
                else if (i == len - 1) part = "►";
                else part = "█";
            } else {
                if (i == 0) part = "◄";
                else if (i == len - 1) part = "▰";
                else part = "█";
            }
        }
        set_cell(buf, screen_row, col, part, style_str);
    }
}

/* Floating log sprite drawing */
static void draw_log(RenderBuffer *buf, int screen_row, int base_col, const Log *lg) {
    int start_x = (int)roundf(lg->x);
    int len = lg->length;

    const char *style = "\033[48;2;125;65;20m\033[38;2;220;160;100m";
    const char *coin_style = "\033[48;2;125;65;20m\033[38;2;255;220;0;1;5m";

    for (int i = 0; i < len; i++) {
        int col = base_col + start_x + i;
        if (col < base_col || col >= base_col + FIELD_WIDTH) continue;

        if (lg->has_coin && i == lg->coin_offset) {
            set_cell(buf, screen_row, col, "★", coin_style);
        } else {
            const char *ch = "═";
            if (i == 0) ch = "❪";
            else if (i == len - 1) ch = "❫";
            else if (i % 2 == 0) ch = "▓";
            set_cell(buf, screen_row, col, ch, style);
        }
    }
}

void render_frame(const GameState *game, int term_w, int term_h) {
    int field_w = FIELD_WIDTH;
    int field_h = VISIBLE_ROWS;
    int panel_w = 26;
    int total_w = field_w + 2 + panel_w + 2;
    int total_h = field_h + 4;

    /* Clamp buffer boundaries */
    if (total_w > MAX_RENDER_COLS) total_w = MAX_RENDER_COLS;
    if (total_h > MAX_RENDER_ROWS) total_h = MAX_RENDER_ROWS;

    int origin_x = (term_w - total_w) / 2;
    int origin_y = (term_h - total_h) / 2;
    if (origin_x < 0) origin_x = 0;
    if (origin_y < 0) origin_y = 0;

    /* Clear virtual buffer */
    for (int r = 0; r < total_h; r++) {
        for (int c = 0; c < total_w; c++) {
            set_cell(&current_buf, r, c, " ", "\033[48;2;15;17;24m");
        }
    }

    /* 1. Header Banner */
    char header_style[64] = "\033[48;2;24;28;38m\033[38;2;255;210;40;1m";
    char title[128];
    snprintf(title, sizeof(title), " 🐔 HOP MANIA :: CROSSY CHICKEN TUI ");
    draw_text(&current_buf, 0, 2, title, header_style);

    char ver_tag[64];
    snprintf(ver_tag, sizeof(ver_tag), "[ANSI C EDITION]");
    draw_text(&current_buf, 0, total_w - 20, ver_tag, "\033[48;2;24;28;38m\033[38;2;120;140;180m");

    /* Top border of field */
    int top_y = 1;
    set_cell(&current_buf, top_y, 0, "┌", "\033[38;2;100;110;140m");
    for (int c = 1; c <= field_w; c++) {
        set_cell(&current_buf, top_y, c, "─", "\033[38;2;100;110;140m");
    }
    set_cell(&current_buf, top_y, field_w + 1, "┐", "\033[38;2;100;110;140m");

    /* 2. Game Field Rows */
    int cam_base_y = (int)roundf(game->camera_y);

    for (int sr = 0; sr < field_h; sr++) {
        int buffer_row = top_y + 1 + sr;
        int world_y = cam_base_y + (field_h - 1 - sr);

        /* Field Left & Right borders */
        set_cell(&current_buf, buffer_row, 0, "│", "\033[38;2;100;110;140m");
        set_cell(&current_buf, buffer_row, field_w + 1, "│", "\033[38;2;100;110;140m");

        if (world_y < 0) {
            /* Out of world bounds below start */
            for (int fc = 0; fc < field_w; fc++) {
                set_cell(&current_buf, buffer_row, 1 + fc, " ", "\033[48;2;12;14;18m");
            }
            continue;
        }

        WorldRow *row = &game->rows[world_y];

        /* Render Row Background */
        if (row->type == ROW_GRASS) {
            const char *bg = (world_y % 2 == 0) ? ANSI_BG_GRASS : ANSI_BG_GRASS_ALT;
            for (int fc = 0; fc < field_w; fc++) {
                const char *ch = " ";
                const char *fg = "\033[38;2;60;170;60m";
                char cell_style[96];

                /* Decorative grass tufts */
                if ((fc * 7 + world_y * 13) % 19 == 0) {
                    ch = "·";
                } else if ((fc * 11 + world_y * 17) % 23 == 0) {
                    ch = "ˏ";
                }

                /* Obstacles & Coins */
                if (row->obstacles[fc] == OBSTACLE_TREE) {
                    ch = "▲"; /* Tree canopy */
                    fg = "\033[38;2;40;220;60;1m";
                } else if (row->obstacles[fc] == OBSTACLE_ROCK) {
                    ch = "●";
                    fg = "\033[38;2;170;170;185;1m";
                } else if (row->obstacles[fc] == OBSTACLE_FLOWER) {
                    ch = "✿";
                    fg = "\033[38;2;255;120;180;1m";
                } else if (row->coins[fc]) {
                    ch = "★";
                    fg = "\033[38;2;255;220;0;1;5m"; /* Sparkling golden star */
                }

                snprintf(cell_style, sizeof(cell_style), "%s%s", bg, fg);
                set_cell(&current_buf, buffer_row, 1 + fc, ch, cell_style);
            }
        }
        else if (row->type == ROW_ROAD) {
            for (int fc = 0; fc < field_w; fc++) {
                const char *ch = " ";
                const char *fg = "\033[38;2;220;200;60m";

                /* Road dashes */
                if (fc % 5 == 2) {
                    ch = "┄";
                }

                char cell_style[96];
                snprintf(cell_style, sizeof(cell_style), "%s%s", ANSI_BG_ROAD, fg);
                set_cell(&current_buf, buffer_row, 1 + fc, ch, cell_style);
            }

            /* Draw vehicles */
            for (int v = 0; v < row->num_vehicles; v++) {
                draw_vehicle(&current_buf, buffer_row, 1, &row->vehicles[v], row->direction);
            }
        }
        else if (row->type == ROW_RIVER) {
            for (int fc = 0; fc < field_w; fc++) {
                const char *ch = "≈";
                const char *fg = ((fc + (int)(game->frame_count / 4)) % 4 == 0) 
                                 ? "\033[38;2;130;205;255m" 
                                 : "\033[38;2;45;110;200m";
                char cell_style[96];
                snprintf(cell_style, sizeof(cell_style), "%s%s", ANSI_BG_RIVER, fg);
                set_cell(&current_buf, buffer_row, 1 + fc, ch, cell_style);
            }

            /* Draw logs */
            for (int l = 0; l < row->num_logs; l++) {
                draw_log(&current_buf, buffer_row, 1, &row->logs[l]);
            }
        }
        else if (row->type == ROW_RAILROAD) {
            for (int fc = 0; fc < field_w; fc++) {
                const char *ch = (fc % 3 == 0) ? "╫" : "═";
                const char *fg = "\033[38;2;160;145;130m";
                char cell_style[96];
                snprintf(cell_style, sizeof(cell_style), "%s%s", ANSI_BG_TRACKS, fg);
                set_cell(&current_buf, buffer_row, 1 + fc, ch, cell_style);
            }

            /* Railroad warning lights */
            if (row->train.warning_active) {
                const char *sig_text = ((game->frame_count / 4) % 2 == 0) 
                                       ? "[! ⚠ TRAIN ⚠ !]" 
                                       : "[   TRAIN   ]";
                const char *sig_style = ((game->frame_count / 4) % 2 == 0)
                                        ? "\033[48;2;200;30;30m\033[38;2;255;255;255;1m"
                                        : "\033[48;2;40;40;40m\033[38;2;255;60;60;1m";
                draw_text(&current_buf, buffer_row, 1 + field_w / 2 - 7, sig_text, sig_style);
            }

            /* Railroad Train */
            if (row->train.is_passing) {
                int tx = (int)roundf(row->train.x);
                int tlen = row->train.length;
                for (int ti = 0; ti < tlen; ti++) {
                    int c = 1 + tx + ti;
                    if (c >= 1 && c <= field_w) {
                        const char *tch = "═";
                        if (row->direction > 0) {
                            if (ti == tlen - 1) tch = "►";
                            else if (ti == 0) tch = "■";
                            else tch = "█";
                        } else {
                            if (ti == 0) tch = "◄";
                            else if (ti == tlen - 1) tch = "■";
                            else tch = "█";
                        }
                        set_cell(&current_buf, buffer_row, c, tch, "\033[48;2;220;40;40m\033[38;2;255;255;255;1m");
                    }
                }
            }
        }
    }

    /* Bottom border of field */
    int bottom_y = top_y + field_h + 1;
    set_cell(&current_buf, bottom_y, 0, "└", "\033[38;2;100;110;140m");
    for (int c = 1; c <= field_w; c++) {
        set_cell(&current_buf, bottom_y, c, "─", "\033[38;2;100;110;140m");
    }
    set_cell(&current_buf, bottom_y, field_w + 1, "┘", "\033[38;2;100;110;140m");

    /* 3. Render Chicken / Player */
    int player_world_y = game->player.y;
    int player_sr = (field_h - 1) - (player_world_y - cam_base_y);
    int player_buf_row = top_y + 1 + player_sr;
    int player_buf_col = 1 + (int)roundf(game->player.x);

    if (player_sr >= 0 && player_sr < field_h && player_buf_col >= 1 && player_buf_col <= field_w) {
        if (game->player.is_alive) {
            /* Chicken Sprite with directional facing */
            const char *wing_l = "ˏ";
            const char *body = "●";
            const char *wing_r = "ˎ";
            const char *chicken_fg = "\033[38;2;255;235;40;1m"; /* Bright Neon Yellow */

            if (game->player.hop_anim_ticks > 0) {
                wing_l = "╲";
                body = "★";
                wing_r = "╱";
                chicken_fg = "\033[38;2;255;255;130;1m";
            } else {
                switch (game->player.facing) {
                    case DIR_UP:
                        wing_l = "ˏ";
                        body = "▲";
                        wing_r = "ˎ";
                        break;
                    case DIR_DOWN:
                        wing_l = "ˏ";
                        body = "▼";
                        wing_r = "ˎ";
                        break;
                    case DIR_LEFT:
                        wing_l = "◄";
                        body = "●";
                        wing_r = "ˎ";
                        break;
                    case DIR_RIGHT:
                        wing_l = "ˏ";
                        body = "●";
                        wing_r = "►";
                        break;
                }
            }

            /* Determine background under chicken */
            WorldRow *cur_row = &game->rows[player_world_y];
            const char *bg = ANSI_BG_GRASS;
            if (cur_row->type == ROW_ROAD) bg = ANSI_BG_ROAD;
            else if (cur_row->type == ROW_RIVER) bg = ANSI_BG_LOG;
            else if (cur_row->type == ROW_RAILROAD) bg = ANSI_BG_TRACKS;

            char chk_style[96];
            snprintf(chk_style, sizeof(chk_style), "%s%s", bg, chicken_fg);

            /* Render 3-cell chicken: Left wing, Body, Right beak/crest */
            set_cell(&current_buf, player_buf_row, player_buf_col - 1, wing_l, chk_style);
            set_cell(&current_buf, player_buf_row, player_buf_col, body, "\033[38;2;255;50;50;1m"); /* Red crest/beak */
            set_cell(&current_buf, player_buf_row, player_buf_col + 1, wing_r, chk_style);
        } else {
            /* Dead chicken sprite */
            const char *dead_ch = "✖";
            const char *dead_fg = "\033[38;2;255;50;50;1m";
            if (game->death_cause == DEATH_DROWNED) {
                dead_ch = "≈";
                dead_fg = "\033[38;2;80;200;255;1m";
            }
            set_cell(&current_buf, player_buf_row, player_buf_col, dead_ch, dead_fg);
        }
    }

    /* 4. Render Eagle / Stagnation */
    if (game->idle_timer < 2.5f && game->player.is_alive) {
        /* Eagle shadow warning above player */
        int shadow_row = player_buf_row - 1;
        if (shadow_row >= top_y + 1 && shadow_row <= bottom_y - 1) {
            set_cell(&current_buf, shadow_row, player_buf_col, "▼", "\033[38;2;40;40;40;1m");
        }
    }
    if (game->eagle_active) {
        int eagle_sr = (field_h - 1) - ((int)roundf(game->eagle_y) - cam_base_y);
        int eagle_buf_row = top_y + 1 + eagle_sr;
        int eagle_buf_col = 1 + (int)roundf(game->eagle_x);
        if (eagle_buf_row >= top_y + 1 && eagle_buf_row <= bottom_y - 1) {
            set_cell(&current_buf, eagle_buf_row, eagle_buf_col - 1, "◤", "\033[38;2;255;255;255;1m");
            set_cell(&current_buf, eagle_buf_row, eagle_buf_col, "█", "\033[38;2;255;200;40;1m");
            set_cell(&current_buf, eagle_buf_row, eagle_buf_col + 1, "◥", "\033[38;2;255;255;255;1m");
        }
    }

    /* 5. Particles */
    for (int p = 0; p < game->particle_count; p++) {
        const Particle *pt = &game->particles[p];
        if (!pt->active) continue;
        int psr = (field_h - 1) - ((int)roundf(pt->y) - cam_base_y);
        int prow = top_y + 1 + psr;
        int pcol = 1 + (int)roundf(pt->x);
        if (prow >= top_y + 1 && prow <= bottom_y - 1 && pcol >= 1 && pcol <= field_w) {
            char pstyle[64];
            snprintf(pstyle, sizeof(pstyle), "\033[%d;1m", pt->color_code);
            set_cell(&current_buf, prow, pcol, pt->ch, pstyle);
        }
    }

    /* 6. Side Dashboard Panel */
    int panel_left = field_w + 3;
    char panel_style[64] = "\033[38;2;200;210;230m\033[48;2;22;26;36m";

    /* Side Panel Box */
    for (int r = top_y; r <= bottom_y; r++) {
        for (int c = panel_left; c < panel_left + panel_w; c++) {
            set_cell(&current_buf, r, c, " ", "\033[48;2;22;26;36m");
        }
    }
    /* Side panel box border */
    for (int c = panel_left; c < panel_left + panel_w; c++) {
        set_cell(&current_buf, top_y, c, "─", "\033[38;2;100;110;140m\033[48;2;22;26;36m");
        set_cell(&current_buf, bottom_y, c, "─", "\033[38;2;100;110;140m\033[48;2;22;26;36m");
    }
    set_cell(&current_buf, top_y, panel_left, "┌", "\033[38;2;100;110;140m\033[48;2;22;26;36m");
    set_cell(&current_buf, top_y, panel_left + panel_w - 1, "┐", "\033[38;2;100;110;140m\033[48;2;22;26;36m");
    set_cell(&current_buf, bottom_y, panel_left, "└", "\033[38;2;100;110;140m\033[48;2;22;26;36m");
    set_cell(&current_buf, bottom_y, panel_left + panel_w - 1, "┘", "\033[38;2;100;110;140m\033[48;2;22;26;36m");
    for (int r = top_y + 1; r < bottom_y; r++) {
        set_cell(&current_buf, r, panel_left, "│", "\033[38;2;100;110;140m\033[48;2;22;26;36m");
        set_cell(&current_buf, r, panel_left + panel_w - 1, "│", "\033[38;2;100;110;140m\033[48;2;22;26;36m");
    }

    draw_text(&current_buf, top_y + 1, panel_left + 4, "◈ DASHBOARD ◈", "\033[38;2;255;210;40;1m\033[48;2;22;26;36m");

    /* Score */
    char buf_text[64];
    snprintf(buf_text, sizeof(buf_text), "◈ DISTANCE : %-5d", game->score);
    draw_text(&current_buf, top_y + 3, panel_left + 2, buf_text, "\033[38;2;80;240;160;1m\033[48;2;22;26;36m");

    snprintf(buf_text, sizeof(buf_text), "★ RECORD   : %-5d", game->high_score);
    draw_text(&current_buf, top_y + 4, panel_left + 2, buf_text, "\033[38;2;255;215;0;1m\033[48;2;22;26;36m");

    snprintf(buf_text, sizeof(buf_text), "✦ CORN/COIN: %-5d", game->coins_collected);
    draw_text(&current_buf, top_y + 5, panel_left + 2, buf_text, "\033[38;2;255;180;40;1m\033[48;2;22;26;36m");

    /* Stagnation / Eagle Meter using block characters */
    draw_text(&current_buf, top_y + 7, panel_left + 2, "EAGLE ALERT:", "\033[38;2;220;220;220m\033[48;2;22;26;36m");
    float idle_pct = game->idle_timer / IDLE_TIME_LIMIT;
    if (idle_pct < 0.0f) idle_pct = 0.0f;
    int filled = (int)(idle_pct * 12.0f);
    char meter[64];
    strcpy(meter, "[");
    for (int m = 0; m < 12; m++) {
        if (m < filled) strcat(meter, "█");
        else strcat(meter, "░");
    }
    strcat(meter, "]");
    const char *meter_color = (idle_pct > 0.4f) ? "\033[38;2;60;220;60;1m" : "\033[38;2;255;50;50;1;5m";
    char meter_style[64];
    snprintf(meter_style, sizeof(meter_style), "%s\033[48;2;22;26;36m", meter_color);
    draw_text(&current_buf, top_y + 8, panel_left + 2, meter, meter_style);

    /* Current Terrain Info */
    WorldRow *prow_info = &game->rows[game->player.y];
    const char *zone_str = "▲ SAFE GRASS";
    const char *zone_col = "\033[38;2;80;220;80;1m";
    if (prow_info->type == ROW_ROAD) {
        static char r_info[32];
        snprintf(r_info, sizeof(r_info), "■ %d-LANE ROAD", prow_info->total_lanes);
        zone_str = r_info;
        zone_col = "\033[38;2;255;100;100;1m";
    } else if (prow_info->type == ROW_RIVER) {
        zone_str = "≈ RUSHING RIVER";
        zone_col = "\033[38;2;80;180;255;1m";
    } else if (prow_info->type == ROW_RAILROAD) {
        zone_str = "╫ RAILROAD TRACK";
        zone_col = "\033[38;2;255;200;40;1m";
    }
    draw_text(&current_buf, top_y + 10, panel_left + 2, "CURRENT ZONE:", "\033[38;2;160;170;190m\033[48;2;22;26;36m");
    char zone_style[64];
    snprintf(zone_style, sizeof(zone_style), "%s\033[48;2;22;26;36m", zone_col);
    draw_text(&current_buf, top_y + 11, panel_left + 2, zone_str, zone_style);

    /* Controls Legend */
    draw_text(&current_buf, top_y + 13, panel_left + 4, "◈ CONTROLS ◈", "\033[38;2;255;210;40;1m\033[48;2;22;26;36m");
    draw_text(&current_buf, top_y + 14, panel_left + 2, " W / ↑ : Hop Forward", panel_style);
    draw_text(&current_buf, top_y + 15, panel_left + 2, " S / ↓ : Hop Backward", panel_style);
    draw_text(&current_buf, top_y + 16, panel_left + 2, " A / ← : Move Left", panel_style);
    draw_text(&current_buf, top_y + 17, panel_left + 2, " D / → : Move Right", panel_style);
    draw_text(&current_buf, top_y + 18, panel_left + 2, " P     : Pause Game", panel_style);
    draw_text(&current_buf, top_y + 19, panel_left + 2, " R     : Restart", panel_style);
    draw_text(&current_buf, top_y + 20, panel_left + 2, " Q /Esc: Quit Game", panel_style);

    /* 7. Modal Overlays */
    if (game->mode == STATE_TITLE) {
        int mw = 44;
        int mh = 14;
        int mx = 1 + (field_w - mw) / 2;
        int my = top_y + 3;

        /* Box background */
        for (int r = my; r < my + mh; r++) {
            for (int c = mx; c < mx + mw; c++) {
                set_cell(&current_buf, r, c, " ", "\033[48;2;16;20;30m");
            }
        }
        /* Rounded Border */
        for (int c = mx + 1; c < mx + mw - 1; c++) {
            set_cell(&current_buf, my, c, "─", "\033[38;2;255;210;40;1m\033[48;2;16;20;30m");
            set_cell(&current_buf, my + mh - 1, c, "─", "\033[38;2;255;210;40;1m\033[48;2;16;20;30m");
        }
        set_cell(&current_buf, my, mx, "╭", "\033[38;2;255;210;40;1m\033[48;2;16;20;30m");
        set_cell(&current_buf, my, mx + mw - 1, "╮", "\033[38;2;255;210;40;1m\033[48;2;16;20;30m");
        set_cell(&current_buf, my + mh - 1, mx, "╰", "\033[38;2;255;210;40;1m\033[48;2;16;20;30m");
        set_cell(&current_buf, my + mh - 1, mx + mw - 1, "╯", "\033[38;2;255;210;40;1m\033[48;2;16;20;30m");
        for (int r = my + 1; r < my + mh - 1; r++) {
            set_cell(&current_buf, r, mx, "│", "\033[38;2;255;210;40;1m\033[48;2;16;20;30m");
            set_cell(&current_buf, r, mx + mw - 1, "│", "\033[38;2;255;210;40;1m\033[48;2;16;20;30m");
        }

        draw_text(&current_buf, my + 1, mx + 12, "★ HOP MANIA ★", "\033[38;2;255;220;0;1m\033[48;2;16;20;30m");
        draw_text(&current_buf, my + 3, mx + 4, "Cross up to 5-lane highways,", "\033[38;2;220;230;240m\033[48;2;16;20;30m");
        draw_text(&current_buf, my + 4, mx + 4, "leap onto drifting river logs,", "\033[38;2;220;230;240m\033[48;2;16;20;30m");
        draw_text(&current_buf, my + 5, mx + 4, "dodge bullet express trains!", "\033[38;2;220;230;240m\033[48;2;16;20;30m");

        draw_text(&current_buf, my + 7, mx + 6, "Controls: W (Forward), S (Back)", "\033[38;2;160;220;255m\033[48;2;16;20;30m");
        draw_text(&current_buf, my + 8, mx + 6, "          A (Left),    D (Right)", "\033[38;2;160;220;255m\033[48;2;16;20;30m");

        const char *press_style = ((game->frame_count / 8) % 2 == 0)
                                  ? "\033[38;2;255;240;50;1;5m\033[48;2;16;20;30m"
                                  : "\033[38;2;180;180;180m\033[48;2;16;20;30m";
        draw_text(&current_buf, my + 10, mx + 6, ">>> PRESS [W] OR [SPACE] TO PLAY <<<", press_style);
        draw_text(&current_buf, my + 12, mx + 13, "Press [Q] to Quit", "\033[38;2;140;150;170m\033[48;2;16;20;30m");
    }
    else if (game->mode == STATE_PAUSED) {
        int mw = 32;
        int mh = 6;
        int mx = 1 + (field_w - mw) / 2;
        int my = top_y + 7;

        for (int r = my; r < my + mh; r++) {
            for (int c = mx; c < mx + mw; c++) {
                set_cell(&current_buf, r, c, " ", "\033[48;2;28;32;44m");
            }
        }
        for (int c = mx + 1; c < mx + mw - 1; c++) {
            set_cell(&current_buf, my, c, "─", "\033[38;2;80;180;255;1m\033[48;2;28;32;44m");
            set_cell(&current_buf, my + mh - 1, c, "─", "\033[38;2;80;180;255;1m\033[48;2;28;32;44m");
        }
        set_cell(&current_buf, my, mx, "╭", "\033[38;2;80;180;255;1m\033[48;2;28;32;44m");
        set_cell(&current_buf, my, mx + mw - 1, "╮", "\033[38;2;80;180;255;1m\033[48;2;28;32;44m");
        set_cell(&current_buf, my + mh - 1, mx, "╰", "\033[38;2;80;180;255;1m\033[48;2;28;32;44m");
        set_cell(&current_buf, my + mh - 1, mx + mw - 1, "╯", "\033[38;2;80;180;255;1m\033[48;2;28;32;44m");
        for (int r = my + 1; r < my + mh - 1; r++) {
            set_cell(&current_buf, r, mx, "│", "\033[38;2;80;180;255;1m\033[48;2;28;32;44m");
            set_cell(&current_buf, r, mx + mw - 1, "│", "\033[38;2;80;180;255;1m\033[48;2;28;32;44m");
        }
        draw_text(&current_buf, my + 2, mx + 7, "◈ GAME PAUSED ◈", "\033[38;2;80;220;255;1m\033[48;2;28;32;44m");
        draw_text(&current_buf, my + 3, mx + 4, "Press [P] to Resume Play", "\033[38;2;220;220;220m\033[48;2;28;32;44m");
        draw_text(&current_buf, my + 4, mx + 6, "Press [Q] to Quit Game", "\033[38;2;160;160;180m\033[48;2;28;32;44m");
    }
    else if (game->mode == STATE_GAMEOVER) {
        int mw = 42;
        int mh = 12;
        int mx = 1 + (field_w - mw) / 2;
        int my = top_y + 4;

        for (int r = my; r < my + mh; r++) {
            for (int c = mx; c < mx + mw; c++) {
                set_cell(&current_buf, r, c, " ", "\033[48;2;32;16;20m");
            }
        }
        for (int c = mx + 1; c < mx + mw - 1; c++) {
            set_cell(&current_buf, my, c, "─", "\033[38;2;255;60;80;1m\033[48;2;32;16;20m");
            set_cell(&current_buf, my + mh - 1, c, "─", "\033[38;2;255;60;80;1m\033[48;2;32;16;20m");
        }
        set_cell(&current_buf, my, mx, "╭", "\033[38;2;255;60;80;1m\033[48;2;32;16;20m");
        set_cell(&current_buf, my, mx + mw - 1, "╮", "\033[38;2;255;60;80;1m\033[48;2;32;16;20m");
        set_cell(&current_buf, my + mh - 1, mx, "╰", "\033[38;2;255;60;80;1m\033[48;2;32;16;20m");
        set_cell(&current_buf, my + mh - 1, mx + mw - 1, "╯", "\033[38;2;255;60;80;1m\033[48;2;32;16;20m");
        for (int r = my + 1; r < my + mh - 1; r++) {
            set_cell(&current_buf, r, mx, "│", "\033[38;2;255;60;80;1m\033[48;2;32;16;20m");
            set_cell(&current_buf, r, mx + mw - 1, "│", "\033[38;2;255;60;80;1m\033[48;2;32;16;20m");
        }

        draw_text(&current_buf, my + 1, mx + 13, "✖ GAME OVER ✖", "\033[38;2;255;50;50;1;5m\033[48;2;32;16;20m");

        const char *cause_str = "You died!";
        switch (game->death_cause) {
            case DEATH_CAR_HIT:
                cause_str = "Squashed by a vehicle on the road!";
                break;
            case DEATH_DROWNED:
                cause_str = "Fell into deep water and drowned!";
                break;
            case DEATH_SWEPT_AWAY:
                cause_str = "Swept off-screen by river rapids!";
                break;
            case DEATH_EAGLE_SNATCHED:
                cause_str = "Stagnated too long! Eagle snatched you!";
                break;
            case DEATH_TRAIN_HIT:
                cause_str = "Hit by the high-speed bullet train!";
                break;
            default:
                break;
        }
        draw_text(&current_buf, my + 3, mx + 2, cause_str, "\033[38;2;255;160;160m\033[48;2;32;16;20m");

        char final_score[64];
        snprintf(final_score, sizeof(final_score), "Final Distance: %d  |  Corn: %d", game->score, game->coins_collected);
        draw_text(&current_buf, my + 5, mx + 4, final_score, "\033[38;2;255;220;60;1m\033[48;2;32;16;20m");

        if (game->score >= game->high_score && game->score > 0) {
            draw_text(&current_buf, my + 7, mx + 8, "* NEW ALL-TIME HIGH SCORE! *", "\033[38;2;255;240;0;1m\033[48;2;32;16;20m");
        }

        draw_text(&current_buf, my + 9, mx + 6, "Press [R] or [W] to Play Again", "\033[38;2;120;240;160;1m\033[48;2;32;16;20m");
        draw_text(&current_buf, my + 10, mx + 11, "Press [Q] to Quit to Shell", "\033[38;2;180;180;180m\033[48;2;32;16;20m");
    }

    /* 8. Differential Serialized Frame Output to Terminal */
    static int last_game_mode = -1;
    if (origin_x != last_origin_x || origin_y != last_origin_y ||
        total_w != last_total_w || total_h != last_total_h ||
        (int)game->mode != last_game_mode) {
        force_full_redraw = true;
        last_origin_x = origin_x;
        last_origin_y = origin_y;
        last_total_w = total_w;
        last_total_h = total_h;
        last_game_mode = (int)game->mode;
    }

    int out_pos = 0;

    if (force_full_redraw) {
        /* Full redraw: clear and redraw all rows */
        out_pos += snprintf(out_string + out_pos, sizeof(out_string) - out_pos, "\033[2J\033[H");

        const char *last_style = "";
        for (int r = 0; r < total_h; r++) {
            out_pos += snprintf(out_string + out_pos, sizeof(out_string) - out_pos,
                                "\033[%d;%dH", origin_y + 1 + r, origin_x + 1);

            for (int c = 0; c < total_w; c++) {
                RenderCell *cell = &current_buf.cells[r][c];
                if (strcmp(cell->style, last_style) != 0) {
                    out_pos += snprintf(out_string + out_pos, sizeof(out_string) - out_pos,
                                        "\033[0m%s", cell->style);
                    last_style = cell->style;
                }
                const char *ch = (cell->ch[0] != '\0') ? cell->ch : " ";
                int ch_len = strlen(ch);
                if (out_pos + ch_len < (int)sizeof(out_string) - 64) {
                    memcpy(out_string + out_pos, ch, ch_len);
                    out_pos += ch_len;
                }
            }
        }
        out_pos += snprintf(out_string + out_pos, sizeof(out_string) - out_pos, "\033[0m");

        memcpy(&prev_buf, &current_buf, sizeof(RenderBuffer));
        force_full_redraw = false;
    } else {
        /* Differential update: emit only spans of cells that changed */
        const char *last_style = "";

        for (int r = 0; r < total_h; r++) {
            int c = 0;
            while (c < total_w) {
                /* Skip unchanged cells */
                while (c < total_w &&
                       strcmp(current_buf.cells[r][c].ch, prev_buf.cells[r][c].ch) == 0 &&
                       strcmp(current_buf.cells[r][c].style, prev_buf.cells[r][c].style) == 0) {
                    c++;
                }

                if (c >= total_w) break;

                /* Move cursor to changed cell */
                int span_start = c;
                out_pos += snprintf(out_string + out_pos, sizeof(out_string) - out_pos,
                                    "\033[%d;%dH", origin_y + 1 + r, origin_x + 1 + span_start);

                /* Write consecutive changed cells */
                while (c < total_w) {
                    RenderCell *cur = &current_buf.cells[r][c];
                    RenderCell *prv = &prev_buf.cells[r][c];

                    if (strcmp(cur->ch, prv->ch) == 0 && strcmp(cur->style, prv->style) == 0) {
                        break;
                    }

                    if (c == span_start || strcmp(cur->style, last_style) != 0) {
                        out_pos += snprintf(out_string + out_pos, sizeof(out_string) - out_pos,
                                            "\033[0m%s", cur->style);
                        last_style = cur->style;
                    }

                    const char *ch = (cur->ch[0] != '\0') ? cur->ch : " ";
                    int ch_len = strlen(ch);
                    if (out_pos + ch_len < (int)sizeof(out_string) - 64) {
                        memcpy(out_string + out_pos, ch, ch_len);
                        out_pos += ch_len;
                    }

                    memcpy(prv, cur, sizeof(RenderCell));
                    c++;
                }
            }
        }

        if (out_pos > 0) {
            out_pos += snprintf(out_string + out_pos, sizeof(out_string) - out_pos, "\033[0m");
        }
    }

    /* Single write system call */
    if (out_pos > 0) {
        safe_write(out_string, out_pos);
    }
}
