# Citrus Shell

A fast, lightweight command line shell written in C.

Citrus Shell is a custom shell built from scratch, focused on speed and simplicity.

---

## Features

*   **Built in Commands**: Includes core shell commands such as `cd`, `exit`, `jobs`, `bg`, `fg`, and `kill`.
*   **Job Control**:
    *   Tracks up to 16 concurrent jobs, each with its own process group.
    *   Supports running processes in the background by appending `&` to the command.
    *   Maintains job states as either running or stopped.
    *   Allows moving suspended jobs to the foreground with `fg` or resuming them in the background with `bg`, either by job id (`%n`), by the previous job (`%-`), or by defaulting to the current job.
    *   Displays the current (`+`) and previous (`-`) job in `jobs` output.
    *   `kill` accepts a raw PID, a job id (`%n`), or a command name.
    *   Hands terminal control to the foreground job and reclaims it afterward, so job control doesn't corrupt terminal state.
*   **Expansions & Parsing**:
    *   **Tilde Expansion**: Resolves `~` to the current user's home directory and `~username` to a specific user's home directory.
    *   **Wildcard Expansion**: Supports inline wildcard matching using `*` and `?`.
    *   **Variable Expansion**: Expands environment variables (`$VARNAME`), the last command's exit status (`$?`), the shell's process ID (`$$`), and the shell name (`$0`).
    *   **Quoting**: Handles single (`'`) and double (`"`) quotes. Variable expansion functions inside double quotes but is ignored inside single quotes.
*   **Command Line Editing**: Integrates with GNU readline for command history and tab completion. It currently utilizes readline's built in filename completion, though command and variable completion are not yet implemented.
*   **Dynamic Prompt**: Features a custom prompt formatted as `username-citrus [folder] $` (or `#` if running as the root user).
*   **Signal Handling**: The shell ignores `SIGINT`, `SIGTSTP`, and `SIGTTOU` so it survives user interrupts and terminal control signals, while restoring default behavior in child processes so they can still be interrupted and suspended normally.
*   **Terminal Recovery**: Restores terminal settings after a foreground job exits or stops, and ensures the next prompt always starts on a clean line even if the previous command's output didn't end with a newline.

---

## Project Structure

```text
CitrusShell/
├── CMakeLists.txt
├── LICENSE
├── README.md
└── src/
    ├── main.c            # REPL loop and builtin dispatch
    ├── parser.c/.h       # Tokenizer, quoting, and expansions
    ├── completion.c/.h   # Readline tab completion
    ├── jobs.c/.h         # Job table and job control
    ├── terminal.c/.h     # Terminal state management
    └── user.c/.h         # User info helpers
```

---

## Prerequisites

*   **CMake**: Minimum version 4.2 is required.
*   **C Compiler**: Must support the C11 standard.
*   **GNU Readline**: The GNU readline library is required.

> **macOS Note:** macOS ships with a BSD libedit under the readline name, so you must install the actual GNU readline (e.g., by running `brew install readline`) for the project to link correctly.

---

## Build

```
mkdir build
cd build
cmake ..
cmake --build .
./CitrusShell
```