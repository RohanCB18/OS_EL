#ifndef NETWORK_H
#define NETWORK_H

#include <sys/types.h>

/* Network namespace management */
int create_network_namespace(void);
int setup_loopback(void);

/* Veth pair setup (for external connectivity) */
int cleanup_veth(void);
int setup_veth_from_host(pid_t sandbox_pid);
int setup_veth_in_sandbox(void);

/* NAT for internet access */
int setup_nat(void);

/* DNS configuration */
int setup_dns(void);

/* Main network setup function (called from inside sandbox) */
int setup_sandbox_network(void);

#endif
