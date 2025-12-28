#include <stdlib.h>
#include <stdio.h>
#include "firewall.h"

int setup_firewall(void)
{
    printf("[+] Applying firewall rules (deny all)...\n");

    // Flush existing rules
    system("iptables -F");
    system("iptables -X");

    // Default deny policy
    system("iptables -P INPUT DROP");
    system("iptables -P OUTPUT DROP");
    system("iptables -P FORWARD DROP");

    // Allow loopback traffic
    system("iptables -A INPUT -i lo -j ACCEPT");
    system("iptables -A OUTPUT -o lo -j ACCEPT");

    return 0;
}
