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
#include <readline/readline.h>
#include <readline/history.h>

#include "parser.h"
#include "completion.h"
#include "jobs.h"
#include "terminal.h"
#include "user.h"

int main() {
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
    sigaction(SIGTTOU, &sa, NULL);

    // SIGCHLD handler only sets a flag (async-signal-safe); the loop below
    // does the actual printing work.
    install_sigchld_handler();

    terminal_save_shell_settings();

    setvbuf(stdout, NULL, _IOLBF, 0);
    while (1) {
        if (sigchld_pending) {
            process_pending_sigchld();
        }

        bool isRootUser = is_root();

        getcwd(buff, FILENAME_MAX);
        char *folder_name = strrchr(buff, '/');

        char prompt[512];
        snprintf(prompt, sizeof(prompt), "%s-citrus [%s] %s ",
                 username, folder_name+1, isRootUser ? "#" : "$");

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

        // Trailing "&" backgrounds the command instead of waiting on it.
        bool background = false;
        if (argc > 0 && strcmp(args[argc - 1], "&") == 0) {
            background = true;
            args[argc - 1] = NULL;
            argc--;
        }
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

        if (strcmp(args[0], "jobs") == 0) {
            print_jobs();
            free(line);
            continue;
        }

        if (strcmp(args[0], "bg") == 0) {
            job_t *j = resolve_job_arg(args[1]);
            if (!j) {
                fprintf(stderr, "bg: no suspended job\n");
                free(line);
                continue;
            }
            kill(j->pid, SIGCONT);
            j->state = JOB_RUNNING;
            touch_job(j);
            printf("[%d]+ %s &\n", j->job_id, j->name);
            free(line);
            continue;
        }

        if (strcmp(args[0], "fg") == 0) {
            job_t *j = resolve_job_arg(args[1]);
            if (!j) {
                fprintf(stderr, "fg: no suspended job\n");
                free(line);
                continue;
            }
            pid_t resumed = j->pid;
            char name[256];
            strncpy(name, j->name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            remove_job(j);

            tcsetpgrp(STDIN_FILENO, resumed); // Give terminal to the resumed job
            kill(resumed, SIGCONT);

            int status;
            waitpid(resumed, &status, WUNTRACED);

            tcsetpgrp(STDIN_FILENO, getpgrp()); // Reclaim terminal

            handle_wait_status(resumed, status, &last_status, name);
            terminal_restore_shell_settings();
            ensure_newline();
            free(line);
            continue;
        }

        if (strcmp(args[0], "kill") == 0) {
            if (argc < 2) {
                fprintf(stderr, "kill: usage: kill <pid> | <name> | %%<job_id> | %%-\n");
                free(line);
                continue;
            }

            pid_t target_pid = -1;
            char *target_arg = args[1];

            if (target_arg[0] == '%') {
                job_t *j = resolve_job_arg(target_arg);
                if (j) target_pid = j->pid;
            }
            else {
                //Check if it is a numeric PID (allow optional "-" for process groups)
                bool is_num = true;
                int start_idx = (target_arg[0] == '-') ? 1 : 0;
                for (int i = start_idx; target_arg[i] != '\0'; i++) {
                    if (target_arg[i] < '0' || target_arg[i] > '9') {
                        is_num = false;
                        break;
                    }
                }

                if (is_num) {
                    target_pid = atoi(target_arg);
                }
                else {
                    job_t *j = find_job_by_name(target_arg);
                    if (j) target_pid = j->pid;
                }
            }

            if (target_pid != -1) {
                if (kill(target_pid, SIGTERM) == -1) {
                    perror("kill");
                } else {
                    kill(target_pid, SIGCONT);
                    int status;
                    pid_t reaped = waitpid(target_pid, &status, WUNTRACED);
                    if (reaped > 0) {
                        job_t *j = find_job_by_pid(target_pid);
                        if (j) {
                            printf("[%d]+  Terminated  %s\n", j->job_id, j->name);
                            fflush(stdout);
                            remove_job(j);
                        }
                    }
                }
            } else {
                fprintf(stderr, "kill: %s: no such process or job\n", target_arg);
            }

            free(line);
            continue;
        }

        // Evaluate
        pid_t child_pid = fork();
        if (child_pid == 0) {
            // Put child in its own process group
            setpgid(0, 0);
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

        if (background) {
            setpgid(child_pid, child_pid); // Avoid race condition
            job_t *j = add_job(child_pid, args[0], JOB_RUNNING);
            if (j) printf("[%d] %d\n", j->job_id, child_pid);
        } else {
            setpgid(child_pid, child_pid); // Avoid race condition
            tcsetpgrp(STDIN_FILENO, child_pid); // Hand over terminal control to child

            int status;
            waitpid(child_pid, &status, WUNTRACED);

            tcsetpgrp(STDIN_FILENO, getpgrp()); //Reclaim terminal control

            terminal_restore_shell_settings();
            handle_wait_status(child_pid, status, &last_status, args[0]);
            ensure_newline();
        }
        free(line);
    }
    return 0;
}