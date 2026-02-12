#ifndef FIREWALL_H
#define FIREWALL_H

#include "policy.h"

int setup_firewall_with_policy(const Policy *policy);


int setup_firewall(void);

/* Cleanup firewall rules */
int cleanup_firewall(void);

#endif
