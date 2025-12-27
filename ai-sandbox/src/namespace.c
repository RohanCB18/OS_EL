#define _GNU_SOURCE
#include <sched.h>
#include <sys/mount.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "namespace.h"

/*
 * Create a new mount namespace for filesystem isolation
 */
int create_mount_namespace(void)
{
    printf("[+] Creating mount namespace...\n");

    // Create new mount namespace
    if (unshare(CLONE_NEWNS) == -1)
    {
        perror("unshare(CLONE_NEWNS)");
        return -1;
    }

    printf("[+] Mount namespace created successfully\n");

    // Make all mounts private (changes don't propagate to host)
    printf("[+] Making all mounts private...\n");
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1)
    {
        perror("mount(MS_PRIVATE)");
        return -1;
    }

    return 0;
}

/*
 * Hide a directory by mounting empty tmpfs over it
 */
int hide_directory(const char *path)
{
    printf("[+] Hiding: %s\n", path);

    // Mount empty tmpfs over the target directory
    if (mount("tmpfs", path, "tmpfs", 0, "size=1k") == -1)
    {
        // Don't fail if directory doesn't exist, just warn
        printf("[!] Warning: Could not hide %s (%s)\n", path, strerror(errno));
        return 0; // Continue anyway
    }

    printf("[+] Successfully hidden: %s\n", path);
    return 0;
}
/*
 * Hide a file by bind-mounting /dev/null over it
 */
int hide_file(const char *path)
{
    printf("[+] Hiding file: %s\n", path);

    // Bind mount /dev/null over the target file
    if (mount("/dev/null", path, NULL, MS_BIND, NULL) == -1)
    {
        printf("[!] Warning: Could not hide %s (%s)\n", path, strerror(errno));
        return 0;
    }

    printf("[+] Successfully hidden: %s\n", path);
    return 0;
}
