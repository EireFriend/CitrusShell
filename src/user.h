#ifndef USER_H
#define USER_H

#include <stddef.h>
#include <stdbool.h>

// Fills "username" with the current user's name via getpwuid().
// Returns 0 on success, -1 on failure
int get_username(char *username, size_t size);

// Returns true if the current process is running as root.
bool is_root(void);

// Returns the current user's home directory: $HOME if set, otherwise
// falls back to the password database. Returns NULL if neither is available.
const char *get_home_dir(void);

// Looks up another user's home directory by username (for ~username
// expansion). Returns NULL if no such user exists.
const char *resolve_user_home(const char *name);

// Fills "hostname" with the machine's hostname via gethostname().
// Returns 0 on success, -1 on failure
int get_hostname(char *hostname, size_t size);

#endif //USER_H