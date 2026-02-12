

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <seccomp.h>
#include "seccomp.h"

static int get_syscall_number(const char *name)
{
    int syscall_nr = seccomp_syscall_resolve_name(name);
    if (syscall_nr == __NR_SCMP_ERROR)
    {
        fprintf(stderr, "[!] Unknown syscall: %s\n", name);
        return -1;
    }
    return syscall_nr;
}


int setup_seccomp_filter(const Policy *policy)
{
    if (policy->blocked_syscalls_count == 0)
    {
        printf("[+] Seccomp: No syscalls blocked (none specified)\n");
        return 0;
    }

    printf("[+] Setting up seccomp filter (%d syscalls to block)...\n",
           policy->blocked_syscalls_count);

    /* Create filter context - default action is ALLOW */
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (ctx == NULL)
    {
        fprintf(stderr, "[!] Failed to initialize seccomp context\n");
        return -1;
    }

    /* Add rules for each blocked syscall */
    int blocked = 0;
    for (int i = 0; i < policy->blocked_syscalls_count; i++)
    {
        const char *syscall_name = policy->blocked_syscalls[i];
        int syscall_nr = get_syscall_number(syscall_name);

        if (syscall_nr < 0)
        {
            /* Unknown syscall, skip it */
            continue;
        }

        //return EPERM
        int rc = seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), syscall_nr, 0);
        if (rc < 0)
        {
            fprintf(stderr, "[!] Failed to add rule for %s: %s\n",
                    syscall_name, strerror(-rc));
            continue;
        }

        printf("    -> Blocked: %s (syscall #%d)\n", syscall_name, syscall_nr);
        blocked++;
    }

    if (blocked == 0)
    {
        printf("[+] Seccomp: No valid syscalls to block\n");
        seccomp_release(ctx);
        return 0;
    }

    /* Load the filter into the kernel */
    int rc = seccomp_load(ctx);
    if (rc < 0)
    {
        fprintf(stderr, "[!] Failed to load seccomp filter: %s\n", strerror(-rc));
        seccomp_release(ctx);
        return -1;
    }

    printf("[+] Seccomp filter loaded: %d syscalls blocked\n", blocked);


    seccomp_release(ctx);

    return 0;
}
