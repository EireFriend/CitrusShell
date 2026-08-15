#include "terminal.h"

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <stdbool.h>

static struct termios shell_termios;
static bool have_shell_termios = false;

void terminal_save_shell_settings(void) {
    if (tcgetattr(STDIN_FILENO, &shell_termios) == 0) {
        have_shell_termios = true;
    }
}

void terminal_restore_shell_settings(void) {
    if (have_shell_termios) {
        tcsetattr(STDIN_FILENO, TCSANOW, &shell_termios);
    }
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

    // Wait for the reply
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