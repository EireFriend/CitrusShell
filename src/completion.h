#ifndef COMPLETION_H
#define COMPLETION_H

// Assigned to rl_attempted_completion_function in main().
// text-the partial word being completed
// start-index in rl_line_buffer where "text" starts
// end-index in rl_line_buffer where "text" ends
char **completion(const char *text, int start, int end);

#endif //COMPLETION_H