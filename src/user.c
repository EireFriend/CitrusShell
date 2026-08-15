#include "user.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>

int get_username(char *username, size_t size) {
    // Uses POSIX getpwuid
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    strncpy(username, pw->pw_name, size - 1);
    username[size - 1] = '\0';
    return 0;
}

bool is_root(void) {
    return getuid() == 0;
}

const char *get_home_dir(void) {
    const char *home = getenv("HOME");
    if (home) return home;

    // Fall back to the password database if $HOME isn't set
    struct passwd *pw = getpwuid(getuid());
    return pw ? pw->pw_dir : NULL;
}

const char *resolve_user_home(const char *name) {
    struct passwd *pw = getpwnam(name);
    return pw ? pw->pw_dir : NULL;
}

int get_hostname(char *hostname, size_t size) {
    if (gethostname(hostname, size) != 0) return -1;
    hostname[size - 1] = '\0'; // guard against truncation without a null terminator
    return 0;
}