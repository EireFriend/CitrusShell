#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>
#include <limits.h>
#include <stdbool.h>
#include <ctype.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif

#ifdef _WIN32
    #include <direct.h>
    #define GetCurrentDir _getcwd
    #define PATH_SEP '\\'
#else
    #include <unistd.h>
    #define GetCurrentDir getcwd
    #define PATH_SEP '/'
#endif

#define MAX_ARGS 64
#define MAX_WORD 1024

int get_username(char *username, size_t size) {
    // Uses POSIX getpwuid
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    strncpy(username, pw->pw_name, size - 1);
    username[size - 1] = '\0';
    return 0;
}

// Given input string starting at $ extract the variable name,
// expand it append the result into "out" (tracked via *out_len),
// and return how many characters were consumed from "in".
int expand_var(const char *in, char *out, size_t *out_len, int last_status, pid_t pid) {
    int consumed = 1; //skip "$"
    char name[256];
    int n = 0;

    if (in[1] == '?') {
        *out_len += snprintf(out + *out_len, 64, "%d", last_status);
        return 2;
    }
    if (in[1] == '$') {
        *out_len += snprintf(out + *out_len, 64, "%d", (int)pid);
        return 2;
    }
    if (in[1] == '0') {
        const char *s = "-citrus";
        strcpy(out + *out_len, s);
        *out_len += strlen(s);
        return 2;
    }

    //General $VARNAME (letters, digits, underscore, can't start with digit)
    while (in[consumed] && (isalnum((unsigned char)in[consumed]) || in[consumed] == '_') && n < 255) {
        name[n++] = in[consumed++];
    }
    name[n] = '\0';

    if (n == 0) { // alone "$" with nothing valid after it -> literal
        out[(*out_len)++] = '$';
        return 1;
    }

    char *val = getenv(name);
    if (val) {
        size_t vlen = strlen(val);
        memcpy(out + *out_len, val, vlen);
        *out_len += vlen;
    }
    return consumed;
}

//quote aware tokenizer with inline $VAR expansion.
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

        if (state == NORMAL) {
            if (c == '\'') { state = IN_SINGLE; in_word = true; continue; }
            if (c == '"')  { state = IN_DOUBLE; in_word = true; continue; }
            if (isspace((unsigned char)c)) {
                if (in_word) {
                    word[word_len] = '\0';
                    args[argc++] = word;
                    if (argc < MAX_ARGS) {
                        word = storage[argc];
                        word_len = 0;
                    }
                    in_word = false;
                }
                continue;
            }
            if (c == '$') {
                in_word = true;
                i += expand_var(line + i, word, &word_len, last_status, pid) - 1;
                continue;
            }
            word[word_len++] = c;
            in_word = true;
            continue;
        }

        if (state == IN_SINGLE) {
            if (c == '\'') { state = NORMAL; continue; } // literal, no expansion
            word[word_len++] = c;
            continue;
        }

        if (state == IN_DOUBLE) {
            if (c == '"') { state = NORMAL; continue; }
            if (c == '$') {
                i += expand_var(line + i, word, &word_len, last_status, pid) - 1;
                continue;
            }
            word[word_len++] = c;
            continue;
        }
    }

    if (in_word) {
        word[word_len] = '\0';
        args[argc++] = word;
    }
    args[argc] = NULL;
    *out_argc = argc;
    return args;
}

int main() {
    char *line = NULL;
    size_t len = 0;
    int last_status = 0;

    char username[256];
    get_username(username, sizeof(username));

    char buff[FILENAME_MAX];

    while (1) {
        bool isRootUser = (getuid() == 0);

        getcwd(buff, FILENAME_MAX);
        char *folder_name = strrchr(buff, PATH_SEP);

        printf("%s-citrus [%s] %s ", username, folder_name + 1, isRootUser ? "#" : "$");
        if (getline(&line, &len, stdin) == -1) break; // Read (Handles Ctrl+D EOF)

        line[strcspn(line, "\n")] = 0; // Strip newline
        if (strlen(line) == 0) continue;

        int argc;
        char **args = tokenize(line, &argc, last_status);
        if (argc == 0) continue;

        if (strcmp(args[0], "exit") == 0) break;

        if (strcmp(args[0], "cd") == 0) {
            const char *target = args[1] ? args[1] : getenv("HOME");
            if (chdir(target) != 0) {
                perror("cd");
                last_status = 1;
            } else {
                last_status = 0;
            }
            continue; // skip fork/exec
        }

        // Evaluate
        pid_t pid = fork();
        if (pid == 0) {
            execvp(args[0], args); // Child replaces itself with the command
            perror("Command not found");
            exit(1);
        }
        else {
            int status;
            wait(&status);
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                last_status = 128 + WTERMSIG(status);
            }
        }
    }
    free(line);
    return 0;
}