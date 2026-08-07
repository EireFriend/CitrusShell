#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

#include "parser.h"

int get_username(char *username, size_t size) {
    // Uses POSIX getpwuid
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    strncpy(username, pw->pw_name, size - 1);
    username[size - 1] = '\0';
    return 0;
}

int main() {
    char *line = NULL;
    size_t len = 0;
    int last_status = 0;

    char username[256] = "user";
    get_username(username, sizeof(username));

    char buff[FILENAME_MAX];

    while (1) {
        bool isRootUser = (getuid() == 0);

        getcwd(buff, FILENAME_MAX);
        char *folder_name = strrchr(buff, '/');

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
            execvp(args[0], args); //Child replaces itself with the command
            // execvp only returns on failure
            switch (errno) {
                case ENOENT:
                    fprintf(stderr, "command not found: %s\n", args[0]);
                    break;
                case EACCES:
                    fprintf(stderr, "permission denied: %s\n", args[0]);
                    break;
                case ENOEXEC:
                    fprintf(stderr, "exec format error: %s\n", args[0]);
                    break;
                default:
                    fprintf(stderr, "%s: %s\n",strerror(errno), args[0]);
            }
            exit(127); //exit code for "couldn't exec"
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