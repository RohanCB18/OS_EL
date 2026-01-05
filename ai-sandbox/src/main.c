#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "namespace.h"
#include "policy.h"
#include "network.h"
#include "firewall.h"

/* ---------- Utility ---------- */

void check_root(void)
{
    if (geteuid() != 0)
    {
        fprintf(stderr, "Error: This program must be run as root\n");
        exit(EXIT_FAILURE);
    }
}

const char *get_real_user(void)
{
    const char *user = getenv("SUDO_USER");
    if (!user)
    {
        fprintf(stderr, "[!] Error: Run using sudo\n");
        exit(EXIT_FAILURE);
    }
    return user;
}

void print_usage(void)
{
    printf(
        "Usage:\n"
        "  ai-run create\n"
        "  ai-run run <policy.yaml>\n"
        "  ai-run destroy\n");
}

/* ---------- CLI Commands ---------- */

void create_default_policy(void)
{
    FILE *f = fopen("policy.yaml", "w");
    if (!f)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fprintf(f,
        "# AI Sandbox Security Policy\n"
        "\n"
        "# Files/directories to hide from the sandbox\n"
        "protected_files:\n"
        "  - ~/.ssh\n"
        "  - ~/.env\n"
        "  - ~/.aws\n"
        "  - ~/.gnupg\n"
        "  - ~/.config/gh\n"
        "\n"
        "# Network policy: DENY (whitelist only) or ALLOW (all)\n"
        "default_network_policy: DENY\n"
        "\n"
        "# Whitelisted domains/IPs (when policy is DENY)\n"
        "network_whitelist:\n"
        "  - github.com\n"
        "  - api.github.com\n"
        "  - pypi.org\n"
        "\n"
        "# Set to true to allow all HTTPS regardless of whitelist\n"
        "allow_all_https: false\n");

    fclose(f);
    printf("[+] Default policy.yaml created\n");
    printf("[+] Edit network_whitelist to add allowed domains\n");
}

/*
 * Signal synchronization between parent and child
 * Child waits for SIGUSR1 from parent after veth is configured
 */
static volatile sig_atomic_t veth_ready = 0;

void sigusr1_handler(int sig)
{
    (void)sig;
    veth_ready = 1;
}

void run_sandbox(const char *policy_file)
{
    check_root();
    
    /* Load policy first (before fork) */
    Policy policy;
    if (load_policy(policy_file, &policy) != 0)
    {
        fprintf(stderr, "Failed to load policy\n");
        exit(EXIT_FAILURE);
    }
    
    print_policy(&policy);
    
    /* Setup signal handler for synchronization */
    signal(SIGUSR1, sigusr1_handler);
    
    /* Fork: parent stays in host namespace, child enters sandbox */
    pid_t pid = fork();
    
    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0)
    {
        /* ======== CHILD PROCESS (becomes the sandbox) ======== */
        
        /* 1. Create mount namespace for filesystem isolation */
        create_mount_namespace();
        
        /* 2. Create network namespace */
        create_network_namespace();
        
        /* 3. Signal parent that we're in the new namespace */
        kill(getppid(), SIGUSR1);
        
        /* 4. Wait for parent to setup veth pair */
        printf("[*] Waiting for network configuration...\n");
        while (!veth_ready)
        {
            usleep(10000); /* 10ms */
        }
        
        /* 5. Configure network inside sandbox */
        setup_sandbox_network();
        
        /* 6. Apply firewall rules (inside sandbox namespace) */
        setup_firewall_with_policy(&policy);
        
        /* 7. Enforce file restrictions */
        const char *user = get_real_user();
        
        for (int i = 0; i < policy.protected_count; i++)
        {
            char resolved_path[512];
            struct stat st;
            
            if (policy.protected_files[i][0] == '~')
            {
                snprintf(resolved_path, sizeof(resolved_path),
                         "/home/%s%s", user,
                         policy.protected_files[i] + 1);
            }
            else
            {
                snprintf(resolved_path, sizeof(resolved_path),
                         "%s", policy.protected_files[i]);
            }
            
            if (stat(resolved_path, &st) == 0)
            {
                if (S_ISDIR(st.st_mode))
                    hide_directory(resolved_path);
                else if (S_ISREG(st.st_mode))
                    hide_file(resolved_path);
            }
        }
        
        /* 8. Launch sandbox shell */
        printf("[+] Launching sandboxed shell...\n");
        printf("===========================================\n");
        printf("  AI SANDBOX ACTIVE\n");
        printf("  Network: Enabled with DNS\n");
        printf("  Protected files: Hidden\n");
        printf("  Type 'exit' to leave sandbox\n");
        printf("===========================================\n");
        
        execl("/bin/bash", "/bin/bash", NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    }
    else
    {
        /* ======== PARENT PROCESS (stays in host namespace) ======== */
        
        /* Wait for child to enter new namespace */
        printf("[*] Parent: waiting for child to create namespace...\n");
        while (!veth_ready)
        {
            usleep(10000); /* 10ms */
        }
        
        /* Small delay to ensure namespace is fully established */
        usleep(100000); /* 100ms */
        
        /* Setup veth pair from host side */
        if (setup_veth_from_host(pid) != 0)
        {
            fprintf(stderr, "[!] Failed to setup veth pair\n");
            kill(pid, SIGTERM);
            exit(EXIT_FAILURE);
        }
        
        /* Setup NAT for internet access */
        setup_nat();
        
        /* Signal child that veth is ready */
        kill(pid, SIGUSR1);
        
        /* Wait for child (sandbox) to exit */
        int status;
        waitpid(pid, &status, 0);
        
        /* Cleanup veth pair */
        printf("[+] Cleaning up network...\n");
        cleanup_veth();
        
        printf("[+] Sandbox session ended\n");
    }
}

void destroy_sandbox(void)
{
    printf("[+] Cleaning up...\n");
    cleanup_veth();
    printf("[+] Cleanup complete\n");
}

/* ---------- MAIN ---------- */

int main(int argc, char *argv[])
{
    if (argc < 2 || strcmp(argv[1], "--help") == 0)
    {
        print_usage();
        return 0;
    }

    if (strcmp(argv[1], "create") == 0)
    {
        create_default_policy();
    }
    else if (strcmp(argv[1], "run") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Error: policy file required\n");
            exit(EXIT_FAILURE);
        }
        run_sandbox(argv[2]);
    }
    else if (strcmp(argv[1], "destroy") == 0)
    {
        destroy_sandbox();
    }
    else
    {
        print_usage();
    }

    return 0;
}
