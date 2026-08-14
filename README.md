# 🍋 Citrus Shell

A fast, lightweight command line shell written in C.

Citrus Shell is a custom shell built from scratch, focused on speed, simplicity, and a clean codebase you can actually read and extend.

---

## Features 

*   **Built-in Commands**: Includes core shell commands such as `cd`, `exit`, `jobs`, `bg`, and `fg`.
*   **Job Control**:
    *   Tracks up to 16 concurrent jobs.
    *   Supports running processes in the background by appending `&` to the command.
    *   Maintains job states as either running or stopped.
    *   Allows moving suspended jobs to the foreground with `fg` or resuming them in the background with `bg`.
*   **Expansions & Parsing**:
    *   **Tilde Expansion**: Resolves `~` to the current user's home directory and `~username` to a specific user's home directory.
    *   **Wildcard Expansion**: Supports inline wildcard matching using `*` and `?`.
    *   **Variable Expansion**: Expands environment variables (`$VARNAME`), the last command's exit status (`$?`), the shell's process ID (`$$`), and the shell name (`$0`).
    *   **Quoting**: Safely handles single (`'`) and double (`"`) quotes. Variable expansion functions inside double quotes but is ignored inside single quotes.
*   **Command Line Editing**: Integrates with GNU readline for command history and tab completion. It currently utilizes readline's built-in filename completion, though command and variable completion are not yet implemented.
*   **Dynamic Prompt**: Features a custom prompt formatted as `username-citrus [folder] $` (or `#` if running as the root user).
*   **Signal Handling**: The shell ignores `SIGINT`, `SIGTSTP`, and `SIGTTOU` to prevent it from exiting when a user interrupts a running foreground command.

---

## Prerequisites

*   **CMake**: Minimum version 3.8 is required.
*   **C Compiler**: Must support the C11 standard.
*   **GNU Readline**: The GNU readline library is required.
    > **macOS Note:** macOS ships with a BSD libedit under the readline name, so you must install the actual GNU readline (e.g., by running `brew install readline`) for the project to link correctly.

---

## Build 🛠️

```bash
mkdir build
cd build
cmake ..
cmake --build .
./CitrusShell