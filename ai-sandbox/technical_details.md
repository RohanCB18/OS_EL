# AI Sandbox - Technical Details

An in-depth look at how each component of AI Sandbox is implemented.

---

## Architecture Overview

```
                    ┌──────────────────────────────────────────────┐
                    │              ai-run (main.c)                 │
                    │                                              │
                    │  1. Parse CLI args                           │
                    │  2. Load policy.yaml                         │
                    │  3. fork()                                   │
                    │         │                                    │
                    │    ┌────┴────┐                               │
                    │    ▼         ▼                               │
                    │  PARENT    CHILD                             │
                    │  (host)   (sandbox)                          │
                    └────┬────────┬────────────────────────────────┘
                         │        │
              ┌──────────┘        └──────────────┐
              ▼                                  ▼
    ┌───────────────────┐            ┌───────────────────────┐
    │  Host Namespace   │            │  Sandbox Namespace    │
    │                   │            │                       │
    │  - Setup veth-host│            │  - Mount namespace    │
    │  - Configure NAT  │◄──veth──► │  - Network namespace  │
    │  - Wait for child │            │  - Setup veth-sandbox │
    │  - Cleanup on exit│            │  - Configure DNS      │
    │                   │            │  - Apply firewall     │
    └───────────────────┘            │  - Hide files         │
                                     │  - Setup seccomp      │
                                     │  - Launch /bin/bash   │
                                     └───────────────────────┘
```

---

## 1. Namespace Isolation

### 1.1 Mount Namespace (namespace.c)

**Purpose:** Create an isolated filesystem view so files can be hidden without affecting the host.

**Implementation:**
```c
// Create new mount namespace
unshare(CLONE_NEWNS);

// Make all mounts private (changes don't propagate to host)
mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL);
```

**File Hiding:**
```c
// For directories: overlay with empty tmpfs
mount("tmpfs", path, "tmpfs", 0, "size=0");

// For files: bind-mount /dev/null over the file
mount("/dev/null", path, NULL, MS_BIND, NULL);
```

**Why it works:** Mount namespaces isolate the filesystem hierarchy. Changes to mounts inside the namespace are invisible to the host, and vice versa.

### 1.2 Network Namespace (network.c)

**Purpose:** Create an isolated network stack with controlled internet access.

**Implementation:**
```c
// In child process: create new network namespace
unshare(CLONE_NEWNET);
// Result: completely blank network stack (no interfaces)
```

**Why fork() is needed:** The parent must stay in the host namespace to create the veth pair and configure NAT. The child enters the new namespace.

**Signal synchronization:**
```
CHILD                           PARENT
  │                               │
  ├── unshare(CLONE_NEWNET)       │
  ├── signal parent (SIGUSR1) ──►│
  ├── pause() [wait]              ├── setup_veth_from_host()
  │                               ├── setup_nat()
  │◄── signal child (SIGUSR1) ────┤
  ├── configure_sandbox_network() │
  ├── setup_dns()                 │
  ├── launch shell                ├── waitpid() [wait]
  │                               │
```

---

## 2. Network Connectivity

### 2.1 Veth Pair (network.c)

A virtual ethernet pair creates a tunnel between host and sandbox namespaces.

```
Host Namespace                    Sandbox Namespace
┌──────────────┐                 ┌──────────────┐
│  veth-host   │◄───connected───►│ veth-sandbox │
│  10.200.1.1  │                 │  10.200.1.2  │
└──────┬───────┘                 └──────┬───────┘
       │                                │
       ▼                                ▼
  Host network                    Sandbox apps
  (internet)                      (curl, git, etc.)
```

**Commands executed:**
```bash
# Create the pair
ip link add veth-host type veth peer name veth-sandbox

# Move one end into sandbox namespace
ip link set veth-sandbox netns $CHILD_PID

# Host side
ip addr add 10.200.1.1/24 dev veth-host
ip link set veth-host up

# Sandbox side (run inside namespace)
ip addr add 10.200.1.2/24 dev veth-sandbox
ip link set veth-sandbox up
ip route add default via 10.200.1.1
```

### 2.2 NAT / Masquerade (network.c)

Allows sandbox traffic to reach the internet by translating addresses.

```bash
# Enable IP forwarding on host
echo 1 > /proc/sys/net/ipv4/ip_forward

# Setup NAT masquerade
iptables -t nat -A POSTROUTING -s 10.200.1.0/24 -j MASQUERADE
```

**How it works:**
1. Sandbox sends packet: `src=10.200.1.2 → dst=20.207.73.82 (github.com)`
2. Host NAT rewrites: `src=<host_ip> → dst=20.207.73.82`
3. Reply arrives at host: `src=20.207.73.82 → dst=<host_ip>`
4. Host NAT rewrites back: `src=20.207.73.82 → dst=10.200.1.2`
5. Packet delivered to sandbox

### 2.3 DNS Configuration (network.c)

```c
// Create temporary resolv.conf
FILE *f = fopen("/tmp/sandbox_resolv.conf", "w");
fprintf(f, "nameserver 8.8.8.8\n");

// Bind-mount over /etc/resolv.conf inside sandbox
mount("/tmp/sandbox_resolv.conf", "/etc/resolv.conf", NULL, MS_BIND, NULL);
```

**Why needed:** The new network namespace has no DNS configuration. Without this, domain names cannot be resolved.

---

## 3. Firewall / Network Whitelist (firewall.c)

### 3.1 Strategy

```
┌─────────────────────────────────────────────────────┐
│                  iptables Rules                      │
│                                                      │
│  1. ACCEPT loopback (lo)          ← always           │
│  2. ACCEPT ESTABLISHED,RELATED    ← reply traffic    │
│  3. ACCEPT DNS (port 53)          ← domain resolve   │
│  4. ACCEPT ICMP                   ← ping             │
│  5. ACCEPT github.com:443         ← whitelisted      │
│  6. ACCEPT pypi.org:443           ← whitelisted      │
│  7. REJECT tcp port 443           ← everything else  │
│  8. REJECT tcp port 80            ← everything else  │
│  9. REJECT all other              ← catch-all        │
│                                                      │
│  Default policy: DROP                                │
└─────────────────────────────────────────────────────┘
```

### 3.2 Domain Resolution at Startup

iptables operates on IP addresses, not domain names. Domains are resolved at sandbox startup:

```c
struct addrinfo *res;
getaddrinfo("github.com", NULL, &hints, &res);
// Returns: 20.207.73.82

// Create iptables rule for the resolved IP
// Use INSERT (not APPEND) to place before REJECT rules
iptables -I OUTPUT 1 -d 20.207.73.82 -p tcp --dport 443 -j ACCEPT
```

**Limitation:** If a domain's IP changes during the sandbox session, the new IP won't be allowed.

### 3.3 REJECT vs DROP

```
DROP:   Packet silently discarded → client waits 60-130 seconds for timeout
REJECT: Sends RST/ICMP error      → client gets "Connection refused" instantly
```

**Implementation:**
```bash
# For HTTP/HTTPS (TCP): send TCP RST
iptables -A OUTPUT -p tcp --dport 443 -j REJECT --reject-with tcp-reset

# For other protocols: send ICMP unreachable
iptables -A OUTPUT -p udp -j REJECT --reject-with icmp-port-unreachable
```

---

## 4. Policy Engine (policy.c, policy.h)

### 4.1 Data Structure

```c
typedef struct {
    // File protection
    char protected_files[MAX_PATHS][MAX_LEN];
    int protected_count;

    // Network whitelist
    char network_whitelist[MAX_PATHS][MAX_LEN];
    int whitelist_count;
    NetworkPolicyMode network_mode;  // NET_POLICY_DENY or NET_POLICY_ALLOW
    int allow_all_https;

    // Syscall blocking
    char blocked_syscalls[MAX_PATHS][MAX_LEN];
    int blocked_syscall_count;
} Policy;
```

### 4.2 YAML Parsing

Uses `libyaml` to parse `policy.yaml`. The parser handles:
- `protected_files:` - Array of file/directory paths
- `network_whitelist:` - Array of domains/IPs
- `default_network_policy:` - "DENY" or "ALLOW"
- `allow_all_https:` - boolean
- `blocked_syscalls:` - Array of syscall names

### 4.3 Path Resolution

`~` is expanded to the real user's home directory:
```c
// ~ → /home/raghottam
if (path[0] == '~') {
    snprintf(resolved, size, "/home/%s%s", user, path + 1);
}
```

---

## 5. Syscall Filtering (seccomp-BPF)

### 5.1 What is seccomp?

seccomp (Secure Computing Mode) is a Linux kernel feature that restricts which system calls a process can make. We use seccomp-BPF (Berkeley Packet Filter) which allows flexible rules.

### 5.2 Implementation

```c
// BPF program structure:
// 1. Load syscall number from seccomp_data
// 2. Compare against each blocked syscall
// 3. If match → return ERRNO (Operation not permitted)
// 4. If no match → ALLOW

struct sock_filter filter[] = {
    // Load architecture
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch)),
    // Verify x86_64
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),

    // Load syscall number
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),

    // For each blocked syscall:
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_chmod, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

    // Default: allow
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
};

// Apply filter
prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
```

### 5.3 Syscall Name to Number Mapping

```c
// Maps human-readable names from policy.yaml to kernel numbers
struct syscall_entry {
    const char *name;
    int number;
};

static const struct syscall_entry syscall_table[] = {
    {"ptrace",   __NR_ptrace},     // 101
    {"chmod",    __NR_chmod},      // 90
    {"fchmod",   __NR_fchmod},     // 91
    {"fchmodat", __NR_fchmodat},   // 268
    {"kill",     __NR_kill},       // 62
    {"reboot",   __NR_reboot},     // 169
    // ... more entries
};
```

### 5.4 Modern vs Legacy Syscalls

Many commands use newer syscalls than expected:

| Command | Expected syscall | Actual syscall used |
|---------|-----------------|---------------------|
| `chmod file` | `chmod` (#90) | `fchmodat` (#268) |
| `chown file` | `chown` (#92) | `fchownat` (#260) |
| `open file` | `open` (#2) | `openat` (#257) |
| `stat file` | `stat` (#4) | `newfstatat` (#262) |

This is why we block multiple variants (chmod + fchmod + fchmodat).

---

## 6. Process Flow (main.c)

```
main()
  ├── parse_args()
  ├── check_root()
  ├── load_policy("policy.yaml")
  ├── print_policy()
  ├── fork()
  │
  ├── [CHILD - enters sandbox]
  │   ├── create_mount_namespace()     ← unshare(CLONE_NEWNS)
  │   ├── create_network_namespace()    ← unshare(CLONE_NEWNET)
  │   ├── signal parent (SIGUSR1)
  │   ├── pause() [wait for veth]
  │   ├── configure_sandbox_network()   ← IP, route, loopback
  │   ├── setup_dns()                   ← resolv.conf
  │   ├── setup_firewall_with_policy()  ← iptables rules
  │   ├── hide files (mount namespace)
  │   ├── setup_seccomp()               ← syscall filter
  │   ├── execl("/bin/bash")            ← become shell
  │   └── [user interacts with sandbox]
  │
  └── [PARENT - stays in host]
      ├── wait for SIGUSR1 from child
      ├── setup_veth_from_host()        ← create + configure veth
      ├── setup_nat()                   ← MASQUERADE rule
      ├── register_session()            ← track in sessions.json
      ├── signal child (SIGUSR1)
      ├── waitpid() [until child exits]
      ├── cleanup_veth()
      ├── unregister_session()
      └── exit
```

---

## 7. Session Tracking (main.c)

Sessions are stored in `/var/lib/ai-sandbox/sessions.json`:

```json
{
  "sessions": [
    {
      "pid": 12345,
      "user": "raghottam",
      "policy": "policy.yaml",
      "cwd": "/home/raghottam/OS_EL/tmp",
      "started": "2026-02-11 12:45:00",
      "status": "running"
    }
  ]
}
```

The Streamlit dashboard reads this file to display active sandboxes.

---

## 8. Build System (Makefile)

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -D_GNU_SOURCE
LDFLAGS = -lyaml      # libyaml for YAML parsing
SRCS = main.c namespace.c policy.c network.c firewall.c
TARGET = ai-run
```

**Dependencies:**
- `libyaml-dev` - YAML parsing
- `iptables` - Firewall rules
- `iproute2` - Network configuration
- Linux kernel 3.17+ - seccomp-BPF support

---

## 9. System-Wide Installation (install.sh)

The `install.sh` script deploys AI Sandbox system-wide, making `ai-run` and `ai-sandbox-gui` available from any directory.

### 9.1 Installation Steps

```bash
#!/bin/bash
# install.sh - System-wide installation script

# 1. Build the binary
make clean && make

# 2. Install ai-run to system PATH
sudo cp ai-run /usr/local/bin/
sudo chmod +x /usr/local/bin/ai-run

# 3. Create state directory
sudo mkdir -p /var/lib/ai-sandbox/
sudo touch /var/lib/ai-sandbox/sessions.json
echo '{"sessions":[]}' | sudo tee /var/lib/ai-sandbox/sessions.json

# 4. Create config directory
sudo mkdir -p /etc/ai-sandbox/
sudo cp policy.yaml /etc/ai-sandbox/default-policy.yaml

# 5. Install dashboard
sudo mkdir -p /usr/share/ai-sandbox/dashboard/
sudo cp dashboard/app.py /usr/share/ai-sandbox/dashboard/
sudo cp dashboard/requirements.txt /usr/share/ai-sandbox/dashboard/

# 6. Install GUI launcher
sudo cp scripts/ai-sandbox-gui.sh /usr/local/bin/ai-sandbox-gui
sudo chmod +x /usr/local/bin/ai-sandbox-gui
```

### 9.2 Directory Structure After Installation

```
/usr/local/bin/
├── ai-run                    # Main sandbox binary
└── ai-sandbox-gui            # Dashboard launcher script

/var/lib/ai-sandbox/
├── sessions.json             # Active sandbox tracking
└── venv/                     # Python virtual env (created on first gui run)
    └── ...

/etc/ai-sandbox/
└── default-policy.yaml       # Template policy file

/usr/share/ai-sandbox/
└── dashboard/
    ├── app.py                # Streamlit dashboard
    └── requirements.txt      # Python dependencies
```

### 9.3 How `ai-run` Finds Policies

When you run `ai-run run policy.yaml`, it searches for the policy file in this order:

1. **Current directory** - `./policy.yaml`
2. **Absolute path** - If you specify `/path/to/policy.yaml`
3. **Fallback** - `/etc/ai-sandbox/default-policy.yaml` (if implemented)

This allows you to:
- Use project-specific policies (in project directory)
- Share policies across projects (absolute path)
- Have a system-wide default

### 9.4 Dashboard Auto-Setup (ai-sandbox-gui)

The `ai-sandbox-gui` script handles Python environment setup automatically:

```bash
#!/bin/bash
# ai-sandbox-gui - Dashboard launcher

DASHBOARD_DIR="/usr/share/ai-sandbox/dashboard"
VENV_DIR="/var/lib/ai-sandbox/venv"

# Check if venv exists
if [ ! -d "$VENV_DIR" ]; then
    echo "Creating Python virtual environment..."
    python3 -m venv "$VENV_DIR"
fi

# Activate venv
source "$VENV_DIR/bin/activate"

# Install dependencies (only if not already installed)
if ! pip show streamlit &>/dev/null; then
    echo "Installing dashboard dependencies..."
    pip install -r "$DASHBOARD_DIR/requirements.txt"
fi

# Launch dashboard
cd "$DASHBOARD_DIR"
streamlit run app.py
```

**First run:**
1. Creates Python venv in `/var/lib/ai-sandbox/venv`
2. Installs Streamlit, PyYAML, watchdog
3. Launches dashboard

**Subsequent runs:**
1. Activates existing venv
2. Launches dashboard immediately (no installation)

### 9.5 Session Tracking Integration

The `ai-run` binary writes to `/var/lib/ai-sandbox/sessions.json` whenever a sandbox starts or stops:

```c
// In main.c - after fork() and network setup
register_session(child_pid, policy_file, real_user, cwd);

// Before cleanup
unregister_session(child_pid);
```

The dashboard reads this file to display active sandboxes in real-time.

### 9.6 Permissions

| Path | Owner | Permissions | Why |
|------|-------|-------------|-----|
| `/usr/local/bin/ai-run` | root | 755 (rwxr-xr-x) | Executable by all, needs root to run |
| `/var/lib/ai-sandbox/` | root | 755 | State directory, writable by root |
| `/var/lib/ai-sandbox/sessions.json` | root | 644 | Readable by all, writable by root |
| `/etc/ai-sandbox/` | root | 755 | Config directory |
| `/usr/share/ai-sandbox/` | root | 755 | Read-only application files |

**Why root?** Creating namespaces, mounting filesystems, and configuring iptables require root privileges. The `ai-run` binary must be run with `sudo`.

### 9.7 Uninstallation

To remove AI Sandbox:

```bash
# Remove binaries
sudo rm /usr/local/bin/ai-run
sudo rm /usr/local/bin/ai-sandbox-gui

# Remove state and config
sudo rm -rf /var/lib/ai-sandbox/
sudo rm -rf /etc/ai-sandbox/

# Remove dashboard
sudo rm -rf /usr/share/ai-sandbox/

# Clean up any leftover network interfaces
sudo ai-run destroy  # (before removing binary)
```

---

## 10. File Structure

```
ai-sandbox/
├── src/
│   ├── main.c          # Entry point, CLI, fork, session tracking
│   ├── namespace.c     # Mount namespace, file hiding
│   ├── namespace.h
│   ├── network.c       # Network namespace, veth, NAT, DNS
│   ├── network.h
│   ├── firewall.c      # iptables rules, whitelist, REJECT
│   ├── firewall.h
│   ├── policy.c        # YAML parser
│   └── policy.h        # Policy struct definition
├── dashboard/
│   ├── app.py          # Streamlit web UI
│   └── requirements.txt
├── scripts/
│   └── ai-sandbox-gui.sh
├── policy.yaml         # Default policy
├── policy_deny.yaml    # Demo: no GitHub
├── policy_allow.yaml   # Demo: with GitHub
├── install.sh          # System-wide installer
├── Makefile
├── HowToUse.md
├── DEMO.md
└── README.md
```
