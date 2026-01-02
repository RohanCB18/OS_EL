#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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
        "protected_files:\n"
        "  - ~/.ssh\n"
        "  - ~/.env\n"
        "  - ~/.aws\n"
        "  - ~/.gnupg\n"
        "  - ~/.config/gh\n"
        "\n"
        "default_network_policy: DENY\n");

    fclose(f);
    printf("[+] Default policy.yaml created\n");
}

void run_sandbox(const char *policy_file)
{
    check_root();

    /* 1. Filesystem isolation */
    create_mount_namespace();

    /* 2. Network isolation */
    create_network_namespace();
    setup_loopback();
    setup_firewall();

    /* 3. Load policy */
    Policy policy;
    if (load_policy(policy_file, &policy) != 0)
    {
        fprintf(stderr, "Failed to load policy\n");
        exit(EXIT_FAILURE);
    }

    print_policy(&policy);

    /* 4. Enforce file restrictions */
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

    /* 5. Launch sandbox shell */
    printf("[+] Launching sandboxed shell...\n");
    execl("/bin/bash", "/bin/bash", NULL);
    perror("execl");
    exit(EXIT_FAILURE);
}

void destroy_sandbox(void)
{
    printf("[+] Cleanup complete (nothing to destroy yet)\n");
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
