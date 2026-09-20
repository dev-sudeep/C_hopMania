#define _POSIX_C_SOURCE 200809L
#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>

static struct termios orig_termios;
static bool raw_mode_enabled = false;
static volatile sig_atomic_t resize_pending = 0;
static int orig_stdin_flags = 0;

#define INBUF_SIZE 256
static char inbuf[INBUF_SIZE];
static int inbuf_head = 0;
static int inbuf_tail = 0;

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

    orig_stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (orig_stdin_flags != -1) {
        fcntl(STDIN_FILENO, F_SETFL, orig_stdin_flags | O_NONBLOCK);
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
        if (orig_stdin_flags != -1) {
            fcntl(STDIN_FILENO, F_SETFL, orig_stdin_flags);
        }
        inbuf_head = inbuf_tail = 0;
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

static void refill_inbuf(void) {
    if (inbuf_head >= inbuf_tail) {
        inbuf_head = 0;
        inbuf_tail = 0;
        ssize_t n = read(STDIN_FILENO, inbuf, sizeof(inbuf));
        if (n > 0) {
            inbuf_tail = (int)n;
        }
    }
}

static int inbuf_has_data(void) {
    if (inbuf_head < inbuf_tail) return 1;
    refill_inbuf();
    return (inbuf_head < inbuf_tail);
}

static int wait_for_data(int timeout_ms) {
    if (inbuf_head < inbuf_tail) return 1;
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, timeout_ms) > 0) {
        refill_inbuf();
        return (inbuf_head < inbuf_tail);
    }
    return 0;
}

KeyInput terminal_read_key(void) {
    if (!inbuf_has_data()) {
        return KEY_NONE;
    }

    char c = inbuf[inbuf_head++];

    /* Check for escape sequence */
    if (c == '\033') {
        /* If no more data follows within 10ms, treat as standalone ESC */
        if (!wait_for_data(10)) {
            return KEY_QUIT;
        }

        char c1 = inbuf[inbuf_head++];
        if (c1 == '[' || c1 == 'O') {
            if (!wait_for_data(10)) {
                return KEY_NONE;
            }

            char c2 = inbuf[inbuf_head++];
            switch (c2) {
                case 'A': return KEY_FORWARD;    /* Up arrow */
                case 'B': return KEY_BACKWARD;   /* Down arrow */
                case 'C': return KEY_MOVE_RIGHT; /* Right arrow */
                case 'D': return KEY_MOVE_LEFT;  /* Left arrow */
                default: break;
            }

            /* Drain multi-character sequences (e.g. mouse or function keys [1~, [<35;...) */
            while (inbuf_has_data()) {
                char ch = inbuf[inbuf_head++];
                if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '~') {
                    break;
                }
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
