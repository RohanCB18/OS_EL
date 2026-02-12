#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <pwd.h>

#include "namespace.h"
#include "policy.h"
#include "network.h"
#include "firewall.h"
#include "seccomp.h"

#define STATE_FILE "/var/lib/ai-sandbox/sessions.json"

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
        "AI Sandbox - Isolated execution environment\n"
        "\n"
        "Usage:\n"
        "  ai-run create              Create policy.yaml in current directory\n"
        "  ai-run run <policy.yaml>   Start sandbox with given policy\n"
        "  ai-run gui                 Open web dashboard (auto-installs deps)\n"
        "  ai-run list                List active sandbox sessions\n"
        "  ai-run destroy             Cleanup sandbox resources\n"
        "\n"
        "Examples:\n"
        "  ai-run create\n"
        "  sudo ai-run run policy.yaml\n"
        "  sudo ai-run gui\n"
        "\n");
}

void register_session(pid_t pid, const char *policy_file, const char *user, const char *cwd)
{
    FILE *f = fopen(STATE_FILE, "r");
    char buffer[4096] = {0};
    
    if (f)
    {
        fread(buffer, 1, sizeof(buffer) - 1, f);
        fclose(f);
    }
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    f = fopen(STATE_FILE, "w");
    if (!f)
    {
        return;
    }
    
    char *sessions_end = strstr(buffer, "]}");
    if (sessions_end && strlen(buffer) > 20)
    {
        *sessions_end = '\0';
        fprintf(f, "%s,\n", buffer);
    }
    else
    {
        fprintf(f, "{\"sessions\":[\n");
    }
    
    fprintf(f, "  {\"pid\":%d,\"user\":\"%s\",\"policy\":\"%s\",\"cwd\":\"%s\",\"started\":\"%s\",\"status\":\"running\"}\n",
            pid, user, policy_file, cwd, timestamp);
    fprintf(f, "]}");
    fclose(f);
}

void unregister_session(pid_t pid)
{
    FILE *f = fopen(STATE_FILE, "r");
    if (!f) return;
    
    char buffer[4096] = {0};
    fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    char search[32];
    snprintf(search, sizeof(search), "\"pid\":%d", pid);
    
    if (strstr(buffer, search))
    {
        f = fopen(STATE_FILE, "w");
        if (f)
        {
            fprintf(f, "{\"sessions\":[]}\n");
            fclose(f);
        }
    }
}

void list_sessions(void)
{
    FILE *f = fopen(STATE_FILE, "r");
    if (!f)
    {
        printf("No active sessions (state file not found)\n");
        printf("Tip: Run 'sudo ./install.sh' to setup system directories\n");
        return;
    }
    
    char buffer[4096] = {0};
    fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    printf("\n=== Active Sandbox Sessions ===\n\n");
    
    if (strstr(buffer, "\"sessions\":[]"))
    {
        printf("No active sessions\n");
    }
    else
    {
        printf("%s\n", buffer);
    }
    printf("\n");
}

void create_default_policy(void)
{
    FILE *f = fopen("policy.yaml", "w");
    if (!f)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fprintf(f,
        "protected_files:\n"
        "  - ~/.ssh\n"
        "  - ~/.env\n"
        "  - ~/.aws\n"
        "  - ~/.gnupg\n"
        "  - ~/.config/gh\n"
        "\n"
        "default_network_policy: DENY\n"
        "\n"
        "network_whitelist:\n"
        "  - github.com\n"
        "  - api.github.com\n"
        "  - pypi.org\n"
        "\n"
        "allow_all_https: false\n"
        "\n"
        "blocked_syscalls:\n"
        "  - ptrace\n");

    fclose(f);
    printf("[+] Default policy.yaml created\n");
    printf("[+] Edit network_whitelist to add allowed domains\n");
    printf("[+] Edit blocked_syscalls to customize syscall restrictions\n");
}

static volatile sig_atomic_t veth_ready = 0;

void sigusr1_handler(int sig)
{
    (void)sig;
    veth_ready = 1;
}

void run_sandbox(const char *policy_file)
{
    check_root();
    
    Policy policy;
    if (load_policy(policy_file, &policy) != 0)
    {
        fprintf(stderr, "Failed to load policy\n");
        exit(EXIT_FAILURE);
    }
    
    print_policy(&policy);
    
    signal(SIGUSR1, sigusr1_handler);
    
    pid_t pid = fork();
    
    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0)
    {
        create_mount_namespace();
        
        create_network_namespace();
        
        kill(getppid(), SIGUSR1);
        
        printf("[*] Waiting for network configuration...\n");
        while (!veth_ready)
        {
            usleep(10000);
        }
        
        setup_sandbox_network();
        
        setup_firewall_with_policy(&policy);
        
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
        
        setup_seccomp_filter(&policy);
        
        printf("[+] Launching sandboxed shell...\n");
        printf("===========================================\n");
        printf("  AI SANDBOX ACTIVE\n");
        printf("  Network: Enabled with DNS\n");
        printf("  Protected files: Hidden\n");
        if (policy.blocked_syscalls_count > 0)
        {
            printf("  Blocked syscalls: %d\n", policy.blocked_syscalls_count);
        }
        printf("  Type 'exit' to leave sandbox\n");
        printf("===========================================\n");
        
        execl("/bin/bash", "/bin/bash", NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("[*] Parent: waiting for child to create namespace...\n");
        while (!veth_ready)
        {
            usleep(10000);
        }
        
        usleep(100000);
        
        if (setup_veth_from_host(pid) != 0)
        {
            fprintf(stderr, "[!] Failed to setup veth pair\n");
            kill(pid, SIGTERM);
            exit(EXIT_FAILURE);
        }
        
        setup_nat();
        
        char cwd[512];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            strcpy(cwd, "unknown");
        }
        register_session(pid, policy_file, get_real_user(), cwd);
        
        kill(pid, SIGUSR1);
        
        int status;
        waitpid(pid, &status, 0);
        
        printf("[+] Cleaning up network...\n");
        cleanup_veth();
        unregister_session(pid);
        
        printf("[+] Sandbox session ended\n");
    }
}

void destroy_sandbox(void)
{
    printf("[+] Cleaning up...\n");
    cleanup_veth();
    printf("[+] Cleanup complete\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
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
            fprintf(stderr, "Usage: ai-run run <policy.yaml>\n");
            exit(EXIT_FAILURE);
        }
        run_sandbox(argv[2]);
    }
    else if (strcmp(argv[1], "list") == 0)
    {
        list_sessions();
    }
    else if (strcmp(argv[1], "gui") == 0)
    {
        printf("[+] Launching AI Sandbox Dashboard...\n");
        int ret = system("ai-sandbox-gui");
        if (ret != 0)
        {
            fprintf(stderr, "[!] Failed to launch GUI. Make sure you ran: sudo ./install.sh\n");
            return 1;
        }
    }
    else if (strcmp(argv[1], "destroy") == 0)
    {
        destroy_sandbox();
    }
    else
    {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage();
        return 1;
    }

    return 0;
}
