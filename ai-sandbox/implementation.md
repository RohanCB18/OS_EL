# AI Sandbox - Technical Implementation Details

A deep dive into the OS-level concepts and libraries used to create an isolated, secure execution environment for AI agents.

---

## 1. Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                        HOST SYSTEM                          │
│                                                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │                    SANDBOX (Child Process)            │  │
│  │                                                       │  │
│  │   [Mount Namespace]        [Network Namespace]        │  │
│  │   - Hidden: ~/.ssh        - Isolated network stack    │  │
│  │   - Hidden: ~/.aws        - veth-sandbox (10.200.1.2) │  │
│  │   - Hidden: ~/.gnupg      - Local iptables firewall   │  │
│  │                                                       │  │
│  └───────────────────────────────────────────────────────┘  │
│           │                            │                    │
│           │ tmpfs mount                │ veth pair          │
│           ▼                            ▼                    │
│   [Hidden Files]              veth-host (10.200.1.1)       │
│                                        │                    │
│                                   NAT (iptables)           │
│                                        │                    │
│                                   Internet                 │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Operating System Concepts Used

### 2.1 Linux Namespaces

Namespaces are a Linux kernel feature that provide **isolation** of global system resources. Each namespace creates a separate, independent instance of that resource for the process.

#### Mount Namespace (`CLONE_NEWNS`)

- **Purpose**: Isolates the filesystem view for a process.
- **How we use it**: After calling `unshare(CLONE_NEWNS)`, mounts made inside the sandbox are invisible to the host.
- **Code**: `src/namespace.c`

```c
// Create a new mount namespace
unshare(CLONE_NEWNS);

// Make mounts private (don't propagate to host)
mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);
```

#### Network Namespace (`CLONE_NEWNET`)

- **Purpose**: Isolates the network stack (interfaces, routing tables, iptables rules).
- **How we use it**: A new namespace has **no network interfaces**. This means the AI process cannot make any network calls until we explicitly allow them.
- **Code**: `src/network.c`

```c
// Create isolated network stack
unshare(CLONE_NEWNET);
```

---

### 2.2 Bind Mounts & tmpfs

These are mechanisms to modify what the process sees on the filesystem.

#### Hiding Directories with `tmpfs`

- **Concept**: `tmpfs` is a RAM-backed filesystem. By mounting an empty `tmpfs` over a sensitive directory, the original contents become invisible.
- **How we use it**: To hide `~/.ssh`, we mount an empty `tmpfs` on top of it. Any `ls ~/.ssh` inside the sandbox returns an empty directory.

```c
mount("tmpfs", "/home/user/.ssh", "tmpfs", 0, "size=1k");
```

#### Hiding Files with Bind Mounts

- **Concept**: A bind mount makes a file or directory from one location appear at another.
- **How we use it**: We bind-mount `/dev/null` over individual sensitive files, so any read returns empty.

```c
mount("/dev/null", "/home/user/.env", NULL, MS_BIND, NULL);
```

---

### 2.3 Virtual Ethernet (veth) Pairs

A `veth` pair is a virtual network cable. Packets sent to one end come out the other.

- **Purpose**: To give the sandbox controlled internet access without exposing the host's network.
- **How we use it**:
    1. Create a pair: `veth-host` and `veth-sandbox`.
    2. `veth-host` stays in the host namespace with IP `10.200.1.1`.
    3. `veth-sandbox` is moved into the sandbox's network namespace with IP `10.200.1.2`.
    4. Traffic from the sandbox goes through the veth to the host, which then NATs it to the internet.

```bash
# Equivalent commands
ip link add veth-host type veth peer name veth-sandbox
ip link set veth-sandbox netns <sandbox_pid>
ip addr add 10.200.1.1/24 dev veth-host
ip link set veth-host up
```

---

### 2.4 Network Address Translation (NAT)

- **Purpose**: The sandbox has a private IP (`10.200.1.2`) that is not routable on the internet. NAT translates it to the host's public IP.
- **How we use it**: We enable IP forwarding and add a `MASQUERADE` rule so outgoing packets appear to come from the host.

```bash
sysctl -w net.ipv4.ip_forward=1
iptables -t nat -A POSTROUTING -s 10.200.1.0/24 -j MASQUERADE
```

---

### 2.5 iptables Firewall

`iptables` is the Linux userspace utility to configure the kernel's packet filtering rules.

#### Default-Deny Policy

We set the default policy to `DROP`, blocking everything not explicitly allowed.

```bash
iptables -P INPUT DROP
iptables -P OUTPUT DROP
iptables -P FORWARD DROP
```

#### Whitelisting Domains

Since `iptables` works at the IP level, we resolve domain names to IPs at startup and add `ACCEPT` rules.

```c
getaddrinfo("github.com", NULL, &hints, &res);
// For each IP resolved:
iptables -I OUTPUT 1 -d <IP> -p tcp --dport 443 -j ACCEPT
```

#### Fail-Fast with `REJECT`

- **Problem**: `DROP` causes connections to hang for 60+ seconds (timeout).
- **Solution**: Use `REJECT --reject-with tcp-reset` to send an immediate "Connection Refused".

```bash
iptables -A OUTPUT -p tcp --dport 443 -j REJECT --reject-with tcp-reset
```

---

### 2.6 DNS Resolution

- **Problem**: The sandbox needs to resolve domain names (like `github.com`) to IPs.
- **Solution**: We bind-mount a custom `/etc/resolv.conf` pointing to Google's DNS (`8.8.8.8`).

```c
// Create /tmp/sandbox_resolv.conf with:
// nameserver 8.8.8.8
mount("/tmp/sandbox_resolv.conf", "/etc/resolv.conf", NULL, MS_BIND, NULL);
```

---

### 2.7 Process Control

#### `fork()`

Creates a child process. The child becomes the sandbox; the parent manages host-side configuration (veth, NAT).

#### `unshare()`

Places the calling process into a new namespace.

#### Signal Synchronization (`SIGUSR1`)

Since the parent needs to configure the veth pair *after* the child enters the network namespace, we use `SIGUSR1` for coordination.

1. Child enters namespace, sends `SIGUSR1` to parent.
2. Parent configures veth, sends `SIGUSR1` to child.
3. Child proceeds with internal network setup.

---

### 2.8 Session State Management

Active sandbox sessions are tracked in `/var/lib/ai-sandbox/sessions.json`. This allows the dashboard and CLI to list running sandboxes.

```json
{
  "sessions": [
    {
      "pid": 12345,
      "user": "raghottam",
      "policy": "policy.yaml",
      "cwd": "/home/raghottam/project",
      "started": "2026-01-08 17:00:00",
      "status": "running"
    }
  ]
}
```

---

## 3. Libraries Used

| Library | Purpose | Where Used in Project |
|---------|---------|----------------------|
| **libyaml** | Parsing YAML policy configuration files | `src/policy.c` - Parses `policy.yaml` to extract protected files, network whitelist, and settings. |
| **glibc (POSIX)** | Standard C library providing system call wrappers (`unshare`, `mount`, `fork`, `signal`) | `src/main.c`, `src/namespace.c`, `src/network.c` - Core sandbox creation logic. |
| **netdb.h / arpa/inet.h** | DNS resolution (`getaddrinfo`) and IP address conversion (`inet_ntop`) | `src/firewall.c` - Resolves domain names to IP addresses for iptables rules. |
| **iptables** (external command) | Kernel packet filtering for network whitelisting and REJECT rules | `src/firewall.c` - Configures DROP/ACCEPT/REJECT rules via `system()` calls. |
| **iproute2** (external command) | Network interface configuration (`ip link`, `ip addr`, `ip route`) | `src/network.c` - Creates veth pairs, assigns IPs, configures routing. |
| **Streamlit** | Python web framework for the interactive dashboard | `dashboard/app.py` - Renders the web UI with session monitoring and policy editing. |
| **PyYAML** | Python library for reading/writing YAML files | `dashboard/app.py` - Loads and saves policy files in the dashboard. |
| **watchdog** | Python library for filesystem event monitoring | `dashboard/requirements.txt` - Potentially for live-reloading policy file changes. |

---

## 4. File Structure

```
ai-sandbox/
├── src/
│   ├── main.c           # CLI entry point, fork logic, session tracking
│   ├── namespace.c      # Mount namespace, file hiding (tmpfs, bind mounts)
│   ├── network.c        # Network namespace, veth, NAT, DNS configuration
│   ├── firewall.c       # iptables rules, domain whitelisting, REJECT logic
│   ├── policy.c         # YAML policy parsing with libyaml
│   ├── policy.h         # Policy struct definition
│   ├── namespace.h      # Namespace function declarations
│   ├── network.h        # Network function declarations
│   └── firewall.h       # Firewall function declarations
├── dashboard/
│   ├── app.py           # Streamlit web dashboard
│   └── requirements.txt # Python dependencies
├── Makefile             # Build configuration (gcc, libyaml)
├── install.sh           # System-wide installation script
└── policy.yaml          # Default policy template
```

---

## 5. Security Model Summary

| Layer | Mechanism | Protection Provided |
|-------|-----------|---------------------|
| **Filesystem** | Mount Namespace + tmpfs/bind-mounts | Hides sensitive files from AI process |
| **Network** | Network Namespace + veth | Isolates sandbox from host network |
| **Firewall** | iptables (REJECT rules) | Enforces domain whitelist, blocks unauthorized traffic immediately |
| **DNS** | Custom resolv.conf | Ensures DNS works within sandbox isolation |
| **Process** | fork + unshare | Creates isolated child process |

---

## 6. References

- [Linux Namespaces](https://man7.org/linux/man-pages/man7/namespaces.7.html)
- [iptables Manual](https://linux.die.net/man/8/iptables)
- [veth (Virtual Ethernet Device)](https://man7.org/linux/man-pages/man4/veth.4.html)
- [libyaml Documentation](https://pyyaml.org/wiki/LibYAML)
- [Streamlit Documentation](https://docs.streamlit.io/)
