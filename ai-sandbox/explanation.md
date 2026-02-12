# AI Sandbox - Output Explanation

This document explains every line of output you see when running `sudo ai-run run policy.yaml`.

---

## 1. Security Policy Summary

```
========== Security Policy ==========
```
This banner is printed immediately after loading and parsing the `policy.yaml` file. It shows a summary of all configured protections.

### File Protection
```
[File Protection]
  Protected paths (5):
    - ~/.ssh
    - ~/.env
    - ~/.aws
    - ~/.gnupg
    - ~/.config/gh
```
Lists every file and directory that will be hidden from the sandbox. These paths are read from the `protected_files:` section in `policy.yaml`. The `~` is later expanded to the real user's home directory (e.g., `/home/raghottam`).

### Network Policy
```
[Network Policy]
  Default mode: DENY
  Whitelisted hosts (3):
    - github.com
    - api.github.com
    - pypi.org
  Allow all HTTPS: no
```
- **Default mode: DENY** means all outbound connections are blocked unless explicitly whitelisted.
- **Whitelisted hosts** are the only domains the sandbox can reach.
- **Allow all HTTPS: no** means even HTTPS traffic is restricted to the whitelist only.

### Syscall Restrictions
```
[Syscall Restrictions]
  Blocked syscalls (4):
    - ptrace
    - chmod
    - fchmod
    - fchmodat
```
Lists system calls that will be blocked using seccomp-BPF. Any program trying to make these calls will get "Operation not permitted".

---

## 2. Process Setup

```
[*] Parent: waiting for child to create namespace...
```
After `fork()`, the parent process waits for the child to signal that it has created new network and mount namespaces. The parent needs the child's PID and namespace to set up networking.

---

## 3. Mount Namespace

```
[+] Creating mount namespace...
[+] Mount namespace created successfully
[+] Making all mounts private...
```
- **Creating mount namespace:** The child calls `unshare(CLONE_NEWNS)` to create a new mount namespace. This gives the sandbox its own filesystem view.
- **Making all mounts private:** Calls `mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL)` to prevent mount changes from propagating between the sandbox and host.

---

## 4. Network Namespace

```
[+] Creating network namespace...
[+] Network namespace created successfully
```
The child calls `unshare(CLONE_NEWNET)` to create a completely isolated network stack. At this point, the sandbox has **zero** network interfaces — not even loopback.

```
[*] Waiting for network configuration...
```
The child signals the parent (SIGUSR1) and waits. The parent now sets up networking from the host side.

---

## 5. Host-Side Network Setup

```
[+] Setting up veth pair from host namespace...
[+] Host side veth configured (IP: 10.200.1.1)
```
The parent creates a **virtual ethernet (veth) pair** — two virtual network interfaces connected like a pipe. One end (`veth-host`, IP `10.200.1.1`) stays in the host namespace. The other end (`veth-sandbox`) is moved into the child's namespace.

```
[+] Setting up NAT for sandbox internet access...
[+] NAT configured - sandbox can access internet
```
The parent enables **Network Address Translation (NAT)** using iptables MASQUERADE. This rewrites the sandbox's IP address (10.200.1.2) to the host's real IP for outbound traffic, allowing the sandbox to reach the internet through the host.

---

## 6. Sandbox-Side Network Setup

```
[+] Enabling loopback interface...
[+] Loopback interface enabled
```
Brings up the `lo` (loopback) interface (127.0.0.1) inside the sandbox. Many applications require loopback to function.

```
[+] Configuring veth inside sandbox...
[+] Sandbox veth configured (IP: 10.200.1.2, Gateway: 10.200.1.1)
```
Configures the sandbox end of the veth pair:
- **IP: 10.200.1.2** — the sandbox's network address
- **Gateway: 10.200.1.1** — routes all traffic through the host

```
[+] Configuring DNS resolver...
[+] DNS configured (using 8.8.8.8)
```
Creates a custom `/etc/resolv.conf` pointing to Google's public DNS (8.8.8.8). This is bind-mounted over the real `/etc/resolv.conf` inside the sandbox. Without this, domain name resolution would fail.

---

## 7. Firewall Configuration

```
[+] Applying firewall rules from policy...
[+] Processing network whitelist (3 entries)...
```
Begins setting up iptables rules based on the policy.

```
[+] Resolving: github.com
    -> Allowed: 20.207.73.82 (github.com)
```
iptables works with IP addresses, not domain names. So each whitelisted domain is resolved to its IP address(es) using `getaddrinfo()`. An iptables ACCEPT rule is created for each resolved IP.

```
[+] Resolving: pypi.org
    -> Allowed: 151.101.64.223 (pypi.org)
    -> Allowed: 151.101.128.223 (pypi.org)
    -> Allowed: 151.101.0.223 (pypi.org)
    -> Allowed: 151.101.192.223 (pypi.org)
```
Some domains resolve to multiple IPs (CDN/load balancing). All IPs are whitelisted.

```
[+] Adding REJECT rules for non-whitelisted traffic
```
After all whitelist ACCEPT rules are added, REJECT rules are appended. Any traffic not matching the whitelist is immediately rejected with a TCP RST or ICMP error.

```
[+] Firewall configured:
    - Default: REJECT (immediate failure)
    - Allow: loopback, DNS, ICMP
    - Whitelist: 3 hosts configured
    - HTTP/HTTPS: whitelist only (others rejected)
```
Summary of the firewall state:
- **REJECT** — blocked connections get instant "Connection refused" (not a silent timeout)
- **loopback, DNS, ICMP** — always allowed for basic functionality
- **3 hosts configured** — only these can be reached
- **whitelist only** — all other HTTP/HTTPS traffic is rejected

---

## 8. File Protection

```
[+] Hiding: /home/raghottam/.ssh
[+] Successfully hidden: /home/raghottam/.ssh
```
For directories, an empty `tmpfs` filesystem is mounted over the path. This makes the directory appear empty inside the sandbox. The original contents are untouched on the host.

```
[+] Hiding file: /home/raghottam/.env
[+] Successfully hidden: /home/raghottam/.env
```
For individual files, `/dev/null` is bind-mounted over the file. Reading the file returns nothing; the original file is safe on the host.

---

## 9. Seccomp Filter

```
[+] Setting up seccomp filter (4 syscalls to block)...
    -> Blocked: ptrace (syscall #101)
    -> Blocked: chmod (syscall #90)
    -> Blocked: fchmod (syscall #91)
    -> Blocked: fchmodat (syscall #268)
[+] Seccomp filter loaded: 4 syscalls blocked
```
- Each syscall name from the policy is looked up in a table mapping names to numbers.
- A BPF (Berkeley Packet Filter) program is constructed that checks every syscall the sandbox makes.
- If a blocked syscall is attempted, the kernel returns `EPERM` (Operation not permitted).
- The number in parentheses (e.g., #101) is the Linux x86_64 syscall number.

```
[!] Unknown syscall: cat
```
If you see this warning, it means a name in `blocked_syscalls` is not a valid Linux system call. Commands like `cat`, `ls`, `rm` are **programs**, not syscalls. They use syscalls internally (`read`, `write`, `openat`, etc.).

---

## 10. Shell Launch

```
[+] Launching sandboxed shell...
===========================================
  AI SANDBOX ACTIVE
  Network: Enabled with DNS
  Protected files: Hidden
  Blocked syscalls: 4
  Type 'exit' to leave sandbox
===========================================
```
The sandbox is fully configured. `execl("/bin/bash")` replaces the child process with an interactive shell. All protections are active.

The prompt changes to `root@hostname:/path#` because the sandbox runs as root within its namespace (needed for mount operations). This root has **no real privileges** on the host due to namespace isolation.

---

## 11. Cleanup

```
[+] Cleaning up network...
[+] Sandbox session ended
```
When the user types `exit`, the shell terminates. The parent process:
1. Deletes the veth pair
2. Removes NAT rules
3. Unregisters the session from `sessions.json`
4. Exits cleanly

All mount namespace changes are automatically cleaned up by the kernel when the namespace is destroyed.
