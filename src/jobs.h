#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <stdbool.h>
#include <signal.h>

#define MAX_JOBS 16

typedef enum { JOB_RUNNING, JOB_STOPPED } job_state_t;

typedef struct {
    int job_id;
    pid_t pid;
    char name[256];
    job_state_t state;
    bool in_use;
    unsigned long last_touched;
} job_t;

// Marks a job as most recently interacted, for +/- display in "jobs".
void touch_job(job_t *j);

// Returns the current (+) and previous (-) jobs, either may be NULL.
void get_current_and_previous(job_t **current, job_t **previous);

// Resolves a job from an optional "%n" or "%-" argument, falling back to the current job if no argument is given.
job_t *resolve_job_arg(char *arg);

// Registers a new job in the table. Returns NULL if the table is full.
job_t *add_job(pid_t pid, const char *name, job_state_t state);

// Looks up a job by its exact command name.
job_t *find_job_by_name(const char *name);

// Looks up a job by its process ID.
job_t *find_job_by_pid(pid_t pid);

// Looks up a job by job id (the "%n" number).
job_t *find_job_by_id(int id);

// Returns the most recently added job, or NULL if none.
job_t *find_most_recent_job(void);

// Frees a job table slot for reuse
void remove_job(job_t *j);

// Prints all currently tracked jobs
void print_jobs(void);

// Set (async-signal-safe only) by the SIGCHLD handler;checked and cleared by the main loop.
extern volatile sig_atomic_t sigchld_pending;

// Installs the SIGCHLD handler.
void install_sigchld_handler(void);

// Call from the main loop whenever sigchld_pending is set. Does the waitpid/printf/redisplay work safely, outside signal context.
void process_pending_sigchld(void);

// Interprets a waitpid() status for a foreground
void handle_wait_status(pid_t pid, int status, int *last_status, const char *cmd_name);

#endif //JOBS_H