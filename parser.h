#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <sys/types.h>

#define MAX_ARGS 64
#define MAX_WORD 1024

// Given input string starting at '$', extract the variable name, expand it,
// and append the result into "out". Returns how many
// characters were consumed from "in".
int expand_var(const char *in, char *out, size_t *out_len, size_t out_cap, int last_status, pid_t pid);

// Quote aware tokenizer with inline $VAR expansion. Splits "line" into
// an array, expanding $VAR / $? / $$ / $0 outside
// single quotes. "line" is modified in place. (tokenize
// copies into internal storage rather than mutating "line").
// The returned array (and the strings it points to) are owned by static
// storage inside tokenize() and remain valid only until the next call to
// tokenize() copy anything you need to keep past that point.
char **tokenize(char *line, int *out_argc, int last_status);

#endif //PARSER_H