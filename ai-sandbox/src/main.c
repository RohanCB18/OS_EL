#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "namespace.h"

/*
 * Ensure the program is run with root privileges.
 * Required for unshare() and mount().
 */
void check_root(void) {
    if (geteuid() != 0) {
        fprintf(stderr, "Error: This program must be run as root\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Hide sensitive directories from the sandboxed process.
 * These paths will appear empty inside the mount namespace.
 *
 * NOTE: Replace /home/rohan if your username is different.
 */
void hide_sensitive_directories(void) {
    hide_directory("/home/rohan/.ssh");
    hide_directory("/home/rohan/.env");
    hide_directory("/home/rohan/.aws");
}

/*
 * Launch an interactive shell inside the sandbox.
 * execl() replaces the current process, so the shell
 * inherits the mount namespace.
 */
void spawn_shell(void) {
    printf("[+] Launching sandboxed shell...\n");

    execl("/bin/bash", "/bin/bash", NULL);

    // This line executes only if execl fails
    perror("execl");
    exit(EXIT_FAILURE);
}

int main(void) {
    check_root();

    if (create_mount_namespace() != 0) {
        fprintf(stderr, "Failed to create mount namespace\n");
        return EXIT_FAILURE;
    }

    hide_sensitive_directories();
    spawn_shell();

    return EXIT_SUCCESS;
}
