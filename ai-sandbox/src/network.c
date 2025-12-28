
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "network.h"

/*
 * Create a new network namespace for network isolation
 *
 * HOW IT WORKS:
 * - unshare(CLONE_NEWNET) creates a separate network stack
 * - Inside this namespace, the process has NO network interfaces
 * - Even "ping 127.0.0.1" won't work until we setup loopback
 *
 * SECURITY BENEFIT:
 * - AI agent cannot make ANY network connections by default
 * - We explicitly allow only what's in the policy
 */
int create_network_namespace(void)
{
    printf("[+] Creating network namespace...\n");

    if (unshare(CLONE_NEWNET) == -1)
    {
        perror("unshare(CLONE_NEWNET)");
        return -1;
    }

    printf("[+] Network namespace created successfully\n");
    return 0;
}

/*
 * Enable loopback interface inside the network namespace
 *
 * WHY NEEDED:
 * - New network namespaces have loopback (lo) interface DOWN
 * - Many applications need localhost (127.0.0.1) to function
 * - This allows local IPC without allowing external network
 *
 * COMMAND EQUIVALENT:
 * - Similar to: ip link set lo up
 */
int setup_loopback(void)
{
    printf("[+] Enabling loopback interface...\n");

    // Use system() to run ip command (simple approach for Phase 2)
    int ret = system("ip link set lo up 2>/dev/null");

    if (ret != 0)
    {
        fprintf(stderr, "[!] Warning: Could not enable loopback (install iproute2)\n");
        return -1;
    }

    printf("[+] Loopback interface enabled\n");
    return 0;
}
