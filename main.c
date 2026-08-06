#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <limits.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif

#define MAX_ARGS 64

int get_username(char *username, size_t size) {
    //Uses POSIX getpwuid
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    strncpy(username, pw->pw_name, size-1);
    username[size - 1] = '\0';
    return 0;
}

int main() {
    char *line = NULL;
    size_t len = 0;

    char username[256];
    get_username(username, sizeof(username));

    while(1) {
        printf("%s-citrus$ ", username);
        if (getline(&line, &len, stdin) == -1) break; // Read (Handles Ctrl+D EOF)

        line[strcspn(line, "\n")] = 0; // Strip newline
        if (strlen(line) == 0) continue;
        if (strcmp(line, "exit") == 0) break;

        // Tokenization
        char *args[MAX_ARGS];
        int i = 0;

        char *token = strtok(line, " "); // first word
        while (token != NULL && i < MAX_ARGS - 1) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL; // execvp needs an array that ends with a null pointer

        if (strcmp(args[0], "exit") == 0) break;

        if (strcmp(args[0], "cd") == 0) {
            const char *target = args[1] ? args[1] : getenv("HOME");
            if (chdir(target) != 0) {
                perror("cd");
            }
            continue; // skip fork exec
        }

        // Evaluate
        if (fork() == 0) {
            execvp(args[0], args); // Child replaces itself with the command
            perror("Command not found");
            exit(1);
        }
        else {
            wait(NULL);
        }
    }
    free(line);
    return 0;
}