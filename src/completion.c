#include "completion.h"

#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>

// falls back to readline's built in filename completion now
// NO VARIABLE COMPLETION, NO COMMAND COMPLETION IMPLEMENTED YET

char **completion(const char *text, int start, int end) {
    (void)start;
    (void)end;
    rl_attempted_completion_over = 1; // stop readline from also trying its default
    return rl_completion_matches(text, rl_filename_completion_function);
}