#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <poll.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "parser.h"
#include "completion.h"

int get_username(char *username, size_t size) {
    // Uses POSIX getpwuid
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    strncpy(username, pw->pw_name, size - 1);
    username[size - 1] = '\0';
    return 0;
}

//Improve prompt readability when the previous command's output didn't end with a trailing newline.
void ensure_newline(void) {
    struct termios orig_termios;
    struct termios new_termios;

    // Save current settings to be used in restoring at the end
    if (tcgetattr(STDIN_FILENO, &orig_termios) != 0) return;

    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO); //raw input
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    //Flush stdio first so ordering vs the raw write() below is correct
    fflush(stdout);
    // Ask terminal for cursor position. Reply arrives on stdin as \033[row;colR
    write(STDOUT_FILENO, "\033[6n", 4);

    char buf[32] = {0};
    size_t total = 0;

    // Wait for the reply, with a timeout in case the terminal never answers.
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    while (total < sizeof(buf) - 1) {
        int ret = poll(&pfd, 1, 50); //50ms deadline
        if (ret <= 0) break; //timeout or error

        if (pfd.revents & POLLIN) {
            char c;
            if (read(STDIN_FILENO, &c, 1) <= 0) break; //EOF error
            buf[total++] = c;
            if (c == 'R') break; //R ends the response
        }
    }

    // restore original terminal settings.
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    // Parse "\033[row;colR" for the column value.
    int row = 0, col = 0;
    if (sscanf(buf, "\033[%d;%dR", &row, &col) == 2) {
        //Cursor not at column 1, means last output had no trailing newline
        if (col != 1) {
            printf("\n");
            fflush(stdout);
        }
    }
}

static void handle_wait_status(pid_t pid, int status, int *last_status, pid_t *suspended_pid, char *suspended_name, const char *cmd_name) {
    if (WIFEXITED(status)) {
        *last_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        *last_status = 128 + WTERMSIG(status);
        putchar('\n');
        fflush(stdout);
    } else if (WIFSTOPPED(status)) {
        printf("\ncitrus: [suspended] %s (pid %d)\n", cmd_name, pid);
        *suspended_pid = pid;
        if (suspended_name != cmd_name) {   //avoid self copy
            strncpy(suspended_name, cmd_name, 255);
            suspended_name[255] = '\0';
        }
    }
}

int main() {
    static pid_t suspended_pid = -1;
    static char suspended_name[256] = "";

    char *line = NULL;
    int last_status = 0;

    char username[256] = "user";
    get_username(username, sizeof(username));

    char buff[FILENAME_MAX];

    rl_attempted_completion_function = completion; //Tab completion
    rl_bind_key('\t', rl_menu_complete); // cycle matches in place on repeated Tab
    rl_variable_bind("completion-ignore-case", "on");

    // Ignore SIGINT (Ctrl+C) in the shell itself so it survives when the
    // user interrupts a running foreground command. Child processes reset
    // this back to default before execvp() so Ctrl+C still kills them
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    while (1) {
        bool isRootUser = (getuid() == 0);

        getcwd(buff, FILENAME_MAX);
        char *folder_name = strrchr(buff, '/');

        char prompt[512];
        snprintf(prompt, sizeof(prompt), "%s-citrus [%s] %s ",
                 username, folder_name + 1, isRootUser ? "#" : "$");

        line = readline(prompt); // Read (readline handles editing, Ctrl+D EOF)
        if (!line) break;

        line[strcspn(line, "\n")] = 0; // Strip newline
        if (strlen(line) == 0) {
            free(line);
            continue;
        }

        add_history(line);

        int argc;
        char **args = tokenize(line, &argc, last_status);
        if (argc == 0) {
            free(line);
            continue;
        }

        if (strcmp(args[0], "exit") == 0) {
            free(line);
            break;
        }

        if (strcmp(args[0], "cd") == 0) {
            const char *target = args[1] ? args[1] : getenv("HOME");
            if (chdir(target) != 0) {
                perror("cd");
                last_status = 1;
            } else {
                last_status = 0;
            }
            free(line);
            continue; // skip fork/exec
        }

        if (strcmp(args[0], "fg") == 0) {
            if (suspended_pid == -1) {
                fprintf(stderr, "fg: no suspended job\n");
                free(line);
                continue;
            }
            pid_t resumed = suspended_pid;
            suspended_pid = -1;

            kill(resumed, SIGCONT);

            int status;
            waitpid(resumed, &status, WUNTRACED);
            handle_wait_status(resumed, status, &last_status, &suspended_pid, suspended_name, suspended_name);
            ensure_newline();
            free(line);
            continue;
        }

        // Evaluate
        pid_t child_pid = fork();
        if (child_pid == 0) {
            //Restore default SIGINT behavior in the child only
            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(SIGINT, &sa, NULL);
            sigaction(SIGTSTP, &sa, NULL);

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
            waitpid(child_pid, &status, WUNTRACED);
            handle_wait_status(child_pid, status, &last_status, &suspended_pid, suspended_name, args[0]);
        }
        ensure_newline();
        free(line);
    }
    return 0;
}