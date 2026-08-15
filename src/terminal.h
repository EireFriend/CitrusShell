#ifndef TERMINAL_H
#define TERMINAL_H

// Ensure a new line at the end of a process to improve prompt readability
// when the previous command's output didn't end with a trailing newline.
void ensure_newline(void);

// Call once at shell startup to remember the terminal settings
void terminal_save_shell_settings(void);

// Call after any point where a child process may have left the terminal
// in a different mode to force it back to the shell's own settings.
void terminal_restore_shell_settings(void);

#endif //TERMINAL_H