#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "network.h"

int create_network_namespace(void)
{
    if (unshare(CLONE_NEWNET) == -1)
    {
        perror("unshare(CLONE_NEWNET)");
        return -1;
    }
    return 0;
}

int setup_loopback(void)
{
    // Bring up loopback interface inside the namespace
    system("ip link set lo up");
    return 0;
}
