#define _POSIX_C_SOURCE 200809L
#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <string.h>

static struct termios orig_termios;
static bool raw_mode_enabled = false;
static volatile sig_atomic_t resize_pending = 0;

static void on_sigwinch(int sig) {
    (void)sig;
    resize_pending = 1;
}

static void on_sigint(int sig) {
    (void)sig;
    terminal_restore();
    exit(0);
}

static void safe_write(const char *buf, size_t len) {
    ssize_t res = write(STDOUT_FILENO, buf, len);
    (void)res;
}

bool terminal_init(void) {
    if (!isatty(STDIN_FILENO)) {
        return false;
    }

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return false;
    }

    struct termios raw = orig_termios;
    /* Input flags: disable break condition, CR-to-NL, parity check, strip 8th bit, flow control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* Output flags: disable post-processing */
    raw.c_oflag &= ~(OPOST);
    /* Control flags: set 8 bits per byte */
    raw.c_cflag |= (CS8);
    /* Local flags: disable echo, canonical mode, extended input processing, signals */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* Return immediately with whatever is available (non-blocking) */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return false;
    }

    raw_mode_enabled = true;
    atexit(terminal_restore);

    /* Setup signals */
    signal(SIGWINCH, on_sigwinch);
    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    /* Use alternate screen buffer and hide cursor */
    const char *setup_seq = "\033[?1049h\033[?25l\033[2J\033[H";
    safe_write(setup_seq, strlen(setup_seq));

    return true;
}

void terminal_restore(void) {
    if (raw_mode_enabled) {
        /* Restore cursor, reset colors, exit alternate screen buffer */
        const char *cleanup_seq = "\033[0m\033[?25h\033[?1049l";
        safe_write(cleanup_seq, strlen(cleanup_seq));
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = false;
    }
}

void terminal_get_size(int *width, int *height) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        *width = 80;
        *height = 24;
    } else {
        *width = ws.ws_col;
        *height = ws.ws_row;
    }
}

KeyInput terminal_read_key(void) {
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) {
        return KEY_NONE;
    }

    /* Check for escape sequence */
    if (c == '\033') {
        char seq[4];
        /* Try reading next characters */
        if (read(STDIN_FILENO, &seq[0], 1) <= 0) {
            return KEY_QUIT; /* Escape key alone */
        }
        if (read(STDIN_FILENO, &seq[1], 1) <= 0) {
            return KEY_NONE;
        }

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_FORWARD;    /* Up arrow = move forward */
                case 'B': return KEY_BACKWARD;   /* Down arrow = move backward */
                case 'C': return KEY_MOVE_RIGHT; /* Right arrow */
                case 'D': return KEY_MOVE_LEFT;  /* Left arrow */
                default: break;
            }
        }
        return KEY_NONE;
    }

    switch (c) {
        case 'w':
        case 'W':
            return KEY_FORWARD;
        case 's':
        case 'S':
            return KEY_BACKWARD;
        case 'a':
        case 'A':
            return KEY_MOVE_LEFT;
        case 'd':
        case 'D':
            return KEY_MOVE_RIGHT;
        case 'p':
        case 'P':
            return KEY_PAUSE;
        case 'r':
        case 'R':
            return KEY_RESTART;
        case 'q':
        case 'Q':
            return KEY_QUIT;
        case ' ':
        case '\n':
        case '\r':
            return KEY_ACTION;
        case 'h':
        case 'H':
        case '?':
            return KEY_HELP;
        default:
            return KEY_OTHER;
    }
}

void terminal_hide_cursor(void) {
    const char *seq = "\033[?25l";
    safe_write(seq, strlen(seq));
}

void terminal_show_cursor(void) {
    const char *seq = "\033[?25h";
    safe_write(seq, strlen(seq));
}

void terminal_clear(void) {
    const char *seq = "\033[2J\033[H";
    safe_write(seq, strlen(seq));
}
