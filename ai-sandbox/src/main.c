#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
    for (int i = 0; i < policy.protected_count; i++)
    {
        hide_directory(policy.protected_files[i]);
    }

    /* 6. Run tool inside sandbox */
    spawn_shell();

    return EXIT_SUCCESS;
}
