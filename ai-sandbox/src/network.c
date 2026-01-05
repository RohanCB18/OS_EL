
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "network.h"

/* Network configuration constants */
#define VETH_HOST "veth-host"
#define VETH_SANDBOX "veth-sandbox"
#define SANDBOX_IP "10.200.1.2"
#define HOST_IP "10.200.1.1"
#define SUBNET_MASK "24"
#define DNS_SERVER "8.8.8.8"

/*
 * Execute a command and return the exit status
 */
static int run_cmd(const char *cmd)
{
    int ret = system(cmd);
    if (ret != 0)
    {
        fprintf(stderr, "[!] Command failed: %s\n", cmd);
    }
    return ret;
}

/*
 * Execute a command silently (suppress output on success)
 */
static int run_cmd_quiet(const char *cmd)
{
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "%s >/dev/null 2>&1", cmd);
    return system(full_cmd);
}

/*
 * Cleanup any existing veth interfaces from previous runs
 */
int cleanup_veth(void)
{
    /* Remove existing veth pair if it exists */
    run_cmd_quiet("ip link delete " VETH_HOST);
    return 0;
}

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

    int ret = run_cmd_quiet("ip link set lo up");

    if (ret != 0)
    {
        fprintf(stderr, "[!] Warning: Could not enable loopback (install iproute2)\n");
        return -1;
    }

    printf("[+] Loopback interface enabled\n");
    return 0;
}

/*
 * Setup veth pair from HOST namespace (called by parent process)
 *
 * HOW IT WORKS:
 * - Creates a virtual ethernet pair (like a pipe for network packets)
 * - One end (veth-host) stays in host namespace with IP 10.200.1.1
 * - Other end (veth-sandbox) moves to sandbox namespace with IP 10.200.1.2
 * - Traffic flows between them like a physical cable
 *
 * ARCHITECTURE:
 *   [Sandbox]                    [Host]
 *   veth-sandbox                 veth-host
 *   10.200.1.2   <--> veth <-->  10.200.1.1 --> Internet
 */
int setup_veth_from_host(pid_t sandbox_pid)
{
    char cmd[256];
    
    printf("[+] Setting up veth pair from host namespace...\n");
    
    /* Cleanup any existing veth from previous runs */
    cleanup_veth();
    
    /* Create veth pair */
    snprintf(cmd, sizeof(cmd),
             "ip link add %s type veth peer name %s",
             VETH_HOST, VETH_SANDBOX);
    if (run_cmd(cmd) != 0)
    {
        fprintf(stderr, "[!] Failed to create veth pair\n");
        return -1;
    }
    
    /* Move sandbox end into the sandbox namespace */
    snprintf(cmd, sizeof(cmd),
             "ip link set %s netns %d",
             VETH_SANDBOX, sandbox_pid);
    if (run_cmd(cmd) != 0)
    {
        fprintf(stderr, "[!] Failed to move veth to sandbox namespace\n");
        return -1;
    }
    
    /* Configure host end */
    snprintf(cmd, sizeof(cmd),
             "ip addr add %s/%s dev %s",
             HOST_IP, SUBNET_MASK, VETH_HOST);
    run_cmd(cmd);
    
    snprintf(cmd, sizeof(cmd), "ip link set %s up", VETH_HOST);
    run_cmd(cmd);
    
    printf("[+] Host side veth configured (IP: %s)\n", HOST_IP);
    return 0;
}

/*
 * Setup veth interface from INSIDE sandbox namespace
 *
 * Called after veth-sandbox has been moved into the namespace.
 * Configures IP address and default route.
 */
int setup_veth_in_sandbox(void)
{
    char cmd[256];
    
    printf("[+] Configuring veth inside sandbox...\n");
    
    /* Assign IP to sandbox end */
    snprintf(cmd, sizeof(cmd),
             "ip addr add %s/%s dev %s",
             SANDBOX_IP, SUBNET_MASK, VETH_SANDBOX);
    run_cmd(cmd);
    
    /* Bring up the interface */
    snprintf(cmd, sizeof(cmd), "ip link set %s up", VETH_SANDBOX);
    run_cmd(cmd);
    
    /* Add default route via host */
    snprintf(cmd, sizeof(cmd),
             "ip route add default via %s dev %s",
             HOST_IP, VETH_SANDBOX);
    run_cmd(cmd);
    
    printf("[+] Sandbox veth configured (IP: %s, Gateway: %s)\n", SANDBOX_IP, HOST_IP);
    return 0;
}

/*
 * Setup NAT (Network Address Translation) on host
 *
 * WHY NEEDED:
 * - Sandbox has private IP (10.200.1.2) - not routable on internet
 * - Host needs to translate sandbox traffic to its own IP
 * - This is same as how your home router works
 *
 * COMMANDS:
 * - Enable IP forwarding (allow kernel to route packets)
 * - Add MASQUERADE rule (replace source IP with host's IP)
 */
int setup_nat(void)
{
    printf("[+] Setting up NAT for sandbox internet access...\n");
    
    /* Enable IP forwarding */
    run_cmd("sysctl -w net.ipv4.ip_forward=1 >/dev/null 2>&1");
    
    /* Add MASQUERADE rule for sandbox subnet */
    run_cmd("iptables -t nat -A POSTROUTING -s 10.200.1.0/24 ! -o " VETH_HOST " -j MASQUERADE");
    
    /* Allow forwarding for sandbox traffic */
    run_cmd("iptables -A FORWARD -i " VETH_HOST " -j ACCEPT");
    run_cmd("iptables -A FORWARD -o " VETH_HOST " -j ACCEPT");
    
    printf("[+] NAT configured - sandbox can access internet\n");
    return 0;
}

/*
 * Setup DNS resolver inside sandbox
 *
 * WHY NEEDED:
 * - /etc/resolv.conf tells the system where to send DNS queries
 * - We bind-mount our own resolv.conf pointing to 8.8.8.8
 * - This ensures DNS works even if host has complex DNS setup
 */
int setup_dns(void)
{
    printf("[+] Configuring DNS resolver...\n");
    
    /* Create temporary resolv.conf */
    const char *tmp_resolv = "/tmp/sandbox_resolv.conf";
    FILE *f = fopen(tmp_resolv, "w");
    if (!f)
    {
        perror("fopen resolv.conf");
        return -1;
    }
    
    fprintf(f, "# Sandbox DNS configuration\n");
    fprintf(f, "nameserver %s\n", DNS_SERVER);
    fprintf(f, "nameserver 8.8.4.4\n");
    fclose(f);
    
    /* Bind mount over /etc/resolv.conf */
    if (mount(tmp_resolv, "/etc/resolv.conf", NULL, MS_BIND, NULL) == -1)
    {
        /* If bind mount fails, try direct copy as fallback */
        fprintf(stderr, "[!] Bind mount failed, trying copy...\n");
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "cp %s /etc/resolv.conf 2>/dev/null || true", tmp_resolv);
        system(cmd);
    }
    
    printf("[+] DNS configured (using %s)\n", DNS_SERVER);
    return 0;
}

/*
 * Full network setup for sandbox with external connectivity
 *
 * This is the main entry point that orchestrates all network setup.
 * Returns the sandbox PID for the parent process to configure veth.
 */
int setup_sandbox_network(void)
{
    /* Setup from inside sandbox namespace */
    setup_loopback();
    setup_veth_in_sandbox();
    setup_dns();
    
    return 0;
}
