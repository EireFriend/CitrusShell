#include "completion.h"

#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <readline/readline.h>

extern char **environ;

static const char *builtins[] = {
    "cd", "exit", "jobs", "bg", "fg", "kill", NULL
};

static bool is_line_blank(void) {
    for (const char *p = rl_line_buffer; *p; p++) {
        if (!isspace((unsigned char)*p)) return false;
    }
    return true;
}

static bool is_command_position(int start) {
    for (int i = 0; i < start; i++) {
        if (!isspace((unsigned char)rl_line_buffer[i])) return false;
    }
    return true;
}

// variable completion
static char *variable_generator(const char *text, int state) {
    static int list_index;
    static size_t prefix_len;

    const char *prefix = text + 1; // skip leading '$'

    if (state == 0) {
        list_index = 0;
        prefix_len = strlen(prefix);
    }

    while (environ[list_index]) {
        const char *entry = environ[list_index++];
        const char *eq = strchr(entry, '=');
        if (!eq) continue;

        size_t name_len = (size_t)(eq - entry);
        if (name_len >= prefix_len && strncmp(entry, prefix, prefix_len) == 0) {
            char *match = malloc(name_len + 2);
            if (!match) return NULL;
            match[0] = '$';
            memcpy(match + 1, entry, name_len);
            match[name_len + 1] = '\0';
            return match;
        }
    }
    return NULL;
}

//command completion
static char *command_generator(const char *text, int state) {
    static size_t text_len;
    static int builtin_index;
    static char *path_copy;
    static char *dir_saveptr;
    static char *current_dir;
    static DIR *dirp;

    if (state == 0) {
        text_len = strlen(text);
        builtin_index = 0;

        free(path_copy);
        if (dirp) closedir(dirp);
        dirp = NULL;

        const char *path_env = getenv("PATH");
        path_copy = strdup(path_env ? path_env : "");
        current_dir = path_copy ? strtok_r(path_copy, ":", &dir_saveptr) : NULL;
        if (current_dir) dirp = opendir(current_dir);
    }

    // matching builtins first.
    while (builtins[builtin_index]) {
        const char *name = builtins[builtin_index++];
        if (strncmp(name, text, text_len) == 0) {
            return strdup(name);
        }
    }

    // walk each $PATH directory for matching
    while (current_dir) {
        if (dirp) {
            struct dirent *entry;
            while ((entry = readdir(dirp)) != NULL) {
                if (strncmp(entry->d_name, text, text_len) != 0) continue;

                char full[PATH_MAX];
                snprintf(full, sizeof(full), "%s/%s", current_dir, entry->d_name);
                if (access(full, X_OK) != 0) continue; // not executable / not permitted

                return strdup(entry->d_name);
            }
            closedir(dirp);
            dirp = NULL;
        }
        current_dir = strtok_r(NULL, ":", &dir_saveptr);
        if (current_dir) dirp = opendir(current_dir);
    }
    return NULL;
}

char **completion(const char *text, int start, int end) {
    (void)end;
    rl_attempted_completion_over = 1; // stop readline from also trying its default

    if (is_line_blank()) {
        return NULL;
    }

    if (text[0] == '$') {
        return rl_completion_matches(text, variable_generator);
    }

    if (is_command_position(start) && !strchr(text, '/')) {
        return rl_completion_matches(text, command_generator);
    }

    return rl_completion_matches(text, rl_filename_completion_function);
}