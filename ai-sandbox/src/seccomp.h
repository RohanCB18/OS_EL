#ifndef SECCOMP_H
#define SECCOMP_H

#include "policy.h"

/*
 * Setup seccomp filter to block specified system calls
 * Returns 0 on success, -1 on failure
 */
int setup_seccomp_filter(const Policy *policy);

#endif
