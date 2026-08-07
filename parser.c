#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

// Appends up to src_len bytes of src into out, clamped so out_len never
// exceeds (always leaving room for a trailing '\0' later).
// Silently appends instead of overflowing if there isn't enough room.
static void safe_append(char *out, size_t *out_len, size_t out_cap, const char *src, size_t src_len) {
    if (*out_len >= out_cap - 1) return; // no room
    size_t space = out_cap - 1 - *out_len;
    size_t n = src_len < space ? src_len : space;
    memcpy(out + *out_len, src, n);
    *out_len += n;
}

int expand_var(const char *in, char *out, size_t *out_len, size_t out_cap, int last_status, pid_t pid) {
    int consumed = 1; //skip "$"
    char name[256];
    int n = 0;

    if (in[1] == '?') {
        char buf[32];
        int written = snprintf(buf, sizeof(buf), "%d", last_status);
        if (written > 0) safe_append(out, out_len, out_cap, buf, (size_t)written);
        return 2;
    }
    if (in[1] == '$') {
        char buf[32];
        int written = snprintf(buf, sizeof(buf), "%d", (int)pid);
        if (written > 0) safe_append(out, out_len, out_cap, buf, (size_t)written);
        return 2;
    }
    if (in[1] == '0') {
        const char *s = "-citrus";
        safe_append(out, out_len, out_cap, s, strlen(s));
        return 2;
    }

    //General $VARNAME
    while (in[consumed] && (isalnum((unsigned char)in[consumed]) || in[consumed] == '_') && n < 255) {
        name[n++] = in[consumed++];
    }
    name[n] = '\0';

    if (n == 0) { //"$" with nothing valid after it
        if (*out_len < out_cap - 1) {
            out[(*out_len)++] = '$';
        }
        return 1;
    }

    char *val = getenv(name);
    if (val) {
        safe_append(out, out_len, out_cap, val, strlen(val));
    }
    return consumed;
}

char **tokenize(char *line, int *out_argc, int last_status) {
    static char *args[MAX_ARGS];
    static char storage[MAX_ARGS][MAX_WORD];
    int argc = 0;

    enum { NORMAL, IN_SINGLE, IN_DOUBLE } state = NORMAL;
    size_t word_len = 0;
    bool in_word = false;
    pid_t pid = getpid();

    char *word = storage[0];

    for (size_t i = 0; line[i] != '\0'; i++) {
        char c = line[i];

        switch (state) {
            case NORMAL:
                if (c == '\'') { state = IN_SINGLE; in_word = true; continue; }
                if (c == '"')  { state = IN_DOUBLE; in_word = true; continue; }
                if (isspace((unsigned char)c)) {
                    if (in_word) {
                        word[word_len] = '\0';
                        //Leave room for the NULL slot in args.
                        if (argc < MAX_ARGS - 1) {
                            args[argc++] = word;
                            word = storage[argc];
                            word_len = 0;
                        } else {
                            // Arg limit reached: drop further words instead of writing past args.
                            word_len = 0;
                        }
                        in_word = false;
                    }
                    continue;
                }
                if (c == '$') {
                    in_word = true;
                    i += expand_var(line + i, word, &word_len, MAX_WORD, last_status, pid) - 1;
                    continue;
                }
                if (word_len < MAX_WORD - 1) word[word_len++] = c;
                in_word = true;
                continue;

            case IN_SINGLE:
                if (c == '\'') { state = NORMAL; continue; } //no expansion
                if (word_len < MAX_WORD - 1) word[word_len++] = c;
                continue;

            case IN_DOUBLE:
                if (c == '"') { state = NORMAL; continue; }
                if (c == '$') {
                    i += expand_var(line + i, word, &word_len, MAX_WORD, last_status, pid)-1;
                    continue;
                }
                if (word_len < MAX_WORD-1) word[word_len++] = c;
                continue;
        }
    }

    if (in_word) {
        word[word_len] = '\0';
        if (argc < MAX_ARGS - 1) {
            args[argc++] = word;
        }
    }
    args[argc] = NULL;
    *out_argc = argc;
    return args;
}