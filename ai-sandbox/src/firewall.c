#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "firewall.h"

static int run_cmd(const char *cmd)
{
    return system(cmd);
}

static int whitelist_domain(const char *domain)
{
    struct addrinfo hints, *res, *p;
    char ip_str[INET6_ADDRSTRLEN];
    char cmd[512];
    int resolved = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    printf("[+] Resolving: %s\n", domain);

    int status = getaddrinfo(domain, NULL, &hints, &res);
    if (status != 0)
    {
        fprintf(stderr, "[!] Could not resolve %s: %s\n", domain, gai_strerror(status));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        void *addr;

        if (p->ai_family == AF_INET)
        {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        }
        else
        {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        inet_ntop(p->ai_family, addr, ip_str, sizeof(ip_str));
        
        snprintf(cmd, sizeof(cmd),
                 "iptables -I OUTPUT 1 -d %s -p tcp --dport 443 -j ACCEPT 2>/dev/null",
                 ip_str);
        run_cmd(cmd);
        
        snprintf(cmd, sizeof(cmd),
                 "iptables -I OUTPUT 1 -d %s -p tcp --dport 80 -j ACCEPT 2>/dev/null",
                 ip_str);
        run_cmd(cmd);

        printf("    -> Allowed: %s (%s)\n", ip_str, domain);
        resolved++;
    }

    freeaddrinfo(res);
    return resolved > 0 ? 0 : -1;
}

static int is_ip_address(const char *str)
{
    struct in_addr ipv4;
    struct in6_addr ipv6;
    
    return (inet_pton(AF_INET, str, &ipv4) == 1 ||
            inet_pton(AF_INET6, str, &ipv6) == 1);
}

static int whitelist_ip(const char *ip)
{
    char cmd[512];
    
    printf("[+] Whitelisting IP: %s\n", ip);
    
    snprintf(cmd, sizeof(cmd),
             "iptables -I OUTPUT 1 -d %s -p tcp --dport 443 -j ACCEPT",
             ip);
    run_cmd(cmd);
    
    snprintf(cmd, sizeof(cmd),
             "iptables -I OUTPUT 1 -d %s -p tcp --dport 80 -j ACCEPT",
             ip);
    run_cmd(cmd);
    
    return 0;
}

int setup_firewall_with_policy(const Policy *policy)
{
    printf("[+] Applying firewall rules from policy...\n");

    run_cmd("iptables -F 2>/dev/null");
    run_cmd("iptables -X 2>/dev/null");

    run_cmd("iptables -P INPUT DROP");
    run_cmd("iptables -P OUTPUT DROP");
    run_cmd("iptables -P FORWARD DROP");

    run_cmd("iptables -A INPUT -i lo -j ACCEPT");
    run_cmd("iptables -A OUTPUT -o lo -j ACCEPT");

    run_cmd("iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT");
    run_cmd("iptables -A OUTPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT");

    run_cmd("iptables -A OUTPUT -p udp --dport 53 -j ACCEPT");
    run_cmd("iptables -A OUTPUT -p tcp --dport 53 -j ACCEPT");

    run_cmd("iptables -A OUTPUT -p icmp -j ACCEPT");
    run_cmd("iptables -A INPUT -p icmp -j ACCEPT");

    if (policy->whitelist_count > 0)
    {
        printf("[+] Processing network whitelist (%d entries)...\n", policy->whitelist_count);
        
        for (int i = 0; i < policy->whitelist_count; i++)
        {
            const char *entry = policy->network_whitelist[i];
            
            if (is_ip_address(entry))
            {
                whitelist_ip(entry);
            }
            else
            {
                whitelist_domain(entry);
            }
        }
    }
    
    if (policy->allow_all_https || policy->whitelist_count == 0)
    {
        printf("[+] Allowing all HTTPS/HTTP traffic\n");
        run_cmd("iptables -A OUTPUT -p tcp --dport 443 -j ACCEPT");
        run_cmd("iptables -A OUTPUT -p tcp --dport 80 -j ACCEPT");
    }
    else
    {
        printf("[+] Adding REJECT rules for non-whitelisted traffic\n");
        run_cmd("iptables -A OUTPUT -p tcp --dport 443 -j REJECT --reject-with tcp-reset");
        run_cmd("iptables -A OUTPUT -p tcp --dport 80 -j REJECT --reject-with tcp-reset");
    }

    run_cmd("iptables -A OUTPUT -p tcp -j REJECT --reject-with tcp-reset");
    run_cmd("iptables -A OUTPUT -p udp -j REJECT --reject-with icmp-port-unreachable");

    printf("[+] Firewall configured:\n");
    printf("    - Default: REJECT (immediate failure)\n");
    printf("    - Allow: loopback, DNS, ICMP\n");
    if (policy->whitelist_count > 0)
    {
        printf("    - Whitelist: %d hosts configured\n", policy->whitelist_count);
    }
    if (policy->allow_all_https || policy->whitelist_count == 0)
    {
        printf("    - HTTP/HTTPS: all allowed\n");
    }
    else
    {
        printf("    - HTTP/HTTPS: whitelist only (others rejected)\n");
    }

    return 0;
}

int setup_firewall(void)
{
    Policy default_policy;
    default_policy.whitelist_count = 0;
    default_policy.allow_all_https = 1;
    default_policy.network_mode = NET_POLICY_DENY;
    
    return setup_firewall_with_policy(&default_policy);
}

int cleanup_firewall(void)
{
    run_cmd("iptables -F 2>/dev/null");
    run_cmd("iptables -X 2>/dev/null");
    run_cmd("iptables -P INPUT ACCEPT");
    run_cmd("iptables -P OUTPUT ACCEPT");
    run_cmd("iptables -P FORWARD ACCEPT");
    return 0;
}
