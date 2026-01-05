#ifndef FIREWALL_H
#define FIREWALL_H

#include "policy.h"

/* Setup firewall rules inside sandbox namespace using policy */
int setup_firewall_with_policy(const Policy *policy);

/* Legacy function - sets up basic firewall (allows all HTTPS) */
int setup_firewall(void);

/* Cleanup firewall rules */
int cleanup_firewall(void);

#endif
