#define _GNU_SOURCE
#include <sched.h>
#include <sys/mount.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "namespace.h"
int create_mount_namespace(void) {
    // Create a new mount namespace
    if (unshare(CLONE_NEWNS) == -1) {
        perror("unshare(CLONE_NEWNS)");
        return -1;
    }

    // Make mounts private so they don't propagate to host
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("mount MS_PRIVATE");
        return -1;
    }

    return 0;
}
int hide_directory(const char *path) {
    // Mount an empty tmpfs over the directory
    if (mount("tmpfs", path, "tmpfs", 0, "") == -1) {
        fprintf(stderr, "Failed to hide %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    return 0;
}
