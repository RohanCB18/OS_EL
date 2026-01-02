#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "namespace.h"
#include "policy.h"
#include "network.h"
#include "firewall.h"

/*
 * Ensure the program is run with root privileges.
 */
void check_root(void)
{
    if (geteuid() != 0)
    {
        fprintf(stderr, "Error: This program must be run as root\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Get the real user who invoked sudo
 */
const char *get_real_user(void)
{
    const char *user = getenv("SUDO_USER");
    if (!user)
    {
        fprintf(stderr, "[!] Error: SUDO_USER not set. Run using sudo.\n");
        exit(EXIT_FAILURE);
    }
    return user;
}

/*
 * Launch an interactive shell inside the sandbox.
 */
void spawn_shell(void)
{
    printf("[+] Launching sandboxed shell...\n");
    execl("/bin/bash", "/bin/bash", NULL);

    perror("execl");
    exit(EXIT_FAILURE);
}

int main(void)
{
    check_root();

    /* 1. Create mount namespace (filesystem isolation) */
    if (create_mount_namespace() != 0)
    {
        fprintf(stderr, "Failed to create mount namespace\n");
        return EXIT_FAILURE;
    }

    /* 2. Create network namespace (network isolation) */
    if (create_network_namespace() != 0)
    {
        fprintf(stderr, "Failed to create network namespace\n");
        return EXIT_FAILURE;
    }

    setup_loopback();   // Enable loopback inside namespace

    /* 3. Apply firewall rules (deny all except loopback) */
    setup_firewall();

    /* 4. Load security policy */
    Policy policy;
    if (load_policy("policy.yaml", &policy) != 0)
    {
        fprintf(stderr, "Failed to load policy\n");
        return EXIT_FAILURE;
    }

    print_policy(&policy);

    /* 5. Enforce file restrictions from policy */
    const char *user = get_real_user();

    for (int i = 0; i < policy.protected_count; i++)
    {
        char resolved_path[512];
        struct stat path_stat;

        /* Expand ~ to /home/<real-user> */
        if (policy.protected_files[i][0] == '~')
        {
            snprintf(resolved_path, sizeof(resolved_path),
                     "/home/%s%s", user,
                     policy.protected_files[i] + 1);
        }
        else
        {
            snprintf(resolved_path, sizeof(resolved_path),
                     "%s", policy.protected_files[i]);
        }

        if (stat(resolved_path, &path_stat) == 0)
        {
            if (S_ISDIR(path_stat.st_mode))
            {
                hide_directory(resolved_path);
            }
            else if (S_ISREG(path_stat.st_mode))
            {
                hide_file(resolved_path);
            }
            else
            {
                printf("[!] Warning: %s is neither file nor directory\n",
                       resolved_path);
            }
        }
        else
        {
            printf("[!] Note: %s does not exist, skipping\n", resolved_path);
        }
    }

    /* 6. Run tool inside sandbox */
    spawn_shell();

    return EXIT_SUCCESS;
}
