#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>
#include <stdbool.h>

/* Key codes */
typedef enum {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_FORWARD,   /* 'w' or 'W' */
    KEY_BACKWARD,  /* 's' or 'S' */
    KEY_MOVE_LEFT, /* 'a' or 'A' */
    KEY_MOVE_RIGHT,/* 'd' or 'D' */
    KEY_PAUSE,     /* 'p' or 'P' */
    KEY_RESTART,   /* 'r' or 'R' */
    KEY_QUIT,      /* 'q' or 'Q' or ESC */
    KEY_ACTION,    /* Space or Enter */
    KEY_HELP,      /* 'h' or '?' */
    KEY_OTHER
} KeyInput;

/* Terminal utilities */
bool terminal_init(void);
void terminal_restore(void);
void terminal_get_size(int *width, int *height);
KeyInput terminal_read_key(void);
void terminal_hide_cursor(void);
void terminal_show_cursor(void);
void terminal_clear(void);

#endif /* TERMINAL_H */
