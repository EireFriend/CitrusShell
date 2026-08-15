#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <readline/readline.h>

static job_t jobs[MAX_JOBS];

static unsigned long touch_seq = 0;

void touch_job(job_t *j) {
    if (j) j->last_touched = ++touch_seq;
}

void get_current_and_previous(job_t **current, job_t **previous) {
    job_t *first = NULL, *second = NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].in_use) continue;
        if (!first || jobs[i].last_touched > first->last_touched) {
            second = first;
            first = &jobs[i];
        } else if (!second || jobs[i].last_touched > second->last_touched) {
            second = &jobs[i];
        }
    }
    *current = first;
    *previous = second;
}

job_t *resolve_job_arg(char *arg) {
    if (arg && arg[0] == '%') {
        if (arg[1] == '-' && arg[2] == '\0') {
            job_t *current, *previous;
            get_current_and_previous(&current, &previous);
            return previous;
        }
        return find_job_by_id(atoi(arg + 1));
    }
    job_t *current, *previous;
    get_current_and_previous(&current, &previous);
    return current;
}

job_t *add_job(pid_t pid, const char *name, job_state_t state) {
    //Find the lowest available job ID
    int lowest_id = 1;
    while (1) {
        bool found = false;
        for (int i = 0; i < MAX_JOBS; i++) {
            if (jobs[i].in_use && jobs[i].job_id == lowest_id) {
                found = true;
                break;
            }
        }
        if (!found) break;
        lowest_id++;
    }

    //Assign the job using the lowest_id
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].in_use) {
            jobs[i].in_use = true;
            jobs[i].job_id = lowest_id;
            jobs[i].pid = pid;
            jobs[i].state = state;
            strncpy(jobs[i].name, name, sizeof(jobs[i].name)-1);
            jobs[i].name[sizeof(jobs[i].name) - 1] = '\0';
            touch_job(&jobs[i]);   // add this
            return &jobs[i];
        }
    }
    return NULL;
}

job_t *find_job_by_name(const char *name) {
    for (int i = 0; i < MAX_JOBS; i++) {
        // Find an active job where the stored name exactly matches the requested name
        if (jobs[i].in_use && strcmp(jobs[i].name, name) == 0) {
            return &jobs[i];
        }
    }
    return NULL;
}

job_t *find_job_by_pid(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use && jobs[i].pid == pid) return &jobs[i];
    }
    return NULL;
}

job_t *find_job_by_id(int id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use && jobs[i].job_id == id) return &jobs[i];
    }
    return NULL;
}

job_t *find_most_recent_job(void) {
    job_t *best = NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use && (!best || jobs[i].job_id > best->job_id)) {
            best = &jobs[i];
        }
    }
    return best;
}

void remove_job(job_t *j) {
    j->in_use = false;
}

void print_jobs(void) {
    job_t *current, *previous;
    get_current_and_previous(&current, &previous);

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use) {
            char marker = ' ';
            if (&jobs[i] == current) marker = '+';
            else if (&jobs[i] == previous) marker = '-';
            printf("[%d] %c  %-8s %s\n", jobs[i].job_id, marker,
                   jobs[i].state == JOB_STOPPED ? "Stopped" : "Running",
                   jobs[i].name);
        }
    }
}

volatile sig_atomic_t sigchld_pending = 0;

// Only async signal safe operation allowed.
static void sigchld_flag_handler(int sig) {
    (void)sig;
    sigchld_pending = 1;
}

void install_sigchld_handler(void) {
    struct sigaction sa;
    sa.sa_handler = sigchld_flag_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}

void process_pending_sigchld(void) {
    sigchld_pending = 0;
    int status;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].in_use) continue;

        pid_t pid = waitpid(jobs[i].pid, &status, WNOHANG | WUNTRACED);
        if (pid <= 0) continue;

        if (WIFEXITED(status)) {
            printf("\r\033[K[%d] +  Done  %s\n", jobs[i].job_id, jobs[i].name);
            fflush(stdout);
            remove_job(&jobs[i]);
            rl_on_new_line();
            rl_redisplay();
        }
        else if (WIFSIGNALED(status)) {
            printf("\r\033[K[%d] +  Terminated  %s\n", jobs[i].job_id, jobs[i].name);
            fflush(stdout);
            remove_job(&jobs[i]);
            rl_on_new_line();
            rl_redisplay();
        }
        else if (WIFSTOPPED(status)) {
            jobs[i].state = JOB_STOPPED;
            touch_job(&jobs[i]);
            printf("\r\033[K[%d] +  Stopped  %s\n", jobs[i].job_id, jobs[i].name);
            fflush(stdout);
            rl_on_new_line();
            rl_redisplay();
        }
    }
}

void handle_wait_status(pid_t pid, int status, int *last_status, const char *cmd_name) {
    if (WIFEXITED(status)) {
        *last_status = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status)) {
        *last_status = 128 + WTERMSIG(status);
        putchar('\n');
        fflush(stdout);
    }
    else if (WIFSTOPPED(status)) {
        job_t *j = find_job_by_pid(pid);
        if (!j) j = add_job(pid, cmd_name, JOB_STOPPED);
        else j->state = JOB_STOPPED;
        touch_job(j);
        if (j) {
            printf("\ncitrus: [%d] +  Stopped  %s\n", j->job_id, j->name);
            fflush(stdout);
        }
    }
}