# AI Sandbox - Key Terms and Concepts

A glossary of important technical terms used in this project.

---

## Linux Namespaces

**Namespaces** are a Linux kernel feature that partitions system resources so that one set of processes sees one set of resources while another set of processes sees a different set. They are the foundation of container technologies like Docker.

AI Sandbox uses two types of namespaces:

### Mount Namespace (`CLONE_NEWNS`)

Isolates the filesystem mount table. When a process enters a new mount namespace, it gets its own copy of the mount table. Mounting or unmounting filesystems inside the namespace does not affect the host.

**In AI Sandbox:** We create a mount namespace and then overlay sensitive directories (like `~/.ssh`) with empty filesystems. The host filesystem is unchanged, but the sandbox sees those directories as empty.

```
Host:                    Sandbox:
~/.ssh/                  ~/.ssh/
  ├── id_rsa               (empty - hidden by tmpfs overlay)
  ├── id_rsa.pub
  └── known_hosts
```

### Network Namespace (`CLONE_NEWNET`)

Isolates the entire network stack — interfaces, IP addresses, routing tables, firewall rules, and ports. A new network namespace starts completely empty with no interfaces at all.

**In AI Sandbox:** We create a new network namespace for the sandbox. This gives us a blank network slate that we then populate with controlled interfaces (veth pair) and strict firewall rules.

---

## Veth Pair (Virtual Ethernet Pair)

A **veth pair** is a pair of virtual network interfaces that act like two ends of a cable. Whatever goes into one end comes out the other. They are used to connect different network namespaces.

```
┌─────────────────────┐         ┌─────────────────────┐
│   Host Namespace     │         │  Sandbox Namespace   │
│                     │         │                     │
│   veth-host  ◄──────┼─────────┼──────►  veth-sandbox │
│   10.200.1.1        │  (pipe) │        10.200.1.2   │
│                     │         │                     │
└─────────────────────┘         └─────────────────────┘
```

**Analogy:** Think of a veth pair like a physical ethernet cable. One end plugs into the host, the other plugs into the sandbox. Data flows through this cable.

**In AI Sandbox:**
- `veth-host` (IP: 10.200.1.1) stays in the host namespace
- `veth-sandbox` (IP: 10.200.1.2) is moved into the sandbox namespace
- The sandbox routes all traffic through `veth-sandbox` → `veth-host` → internet

---

## NAT (Network Address Translation)

**NAT** is a technique where a router modifies the source or destination IP addresses of packets as they pass through. The most common form is **MASQUERADE**, which replaces the source IP with the router's own IP.

**Why needed:** The sandbox uses a private IP (10.200.1.2) that isn't routable on the internet. NAT translates this to the host's real IP so responses can find their way back.

```
Sandbox sends:          Host NAT rewrites:         Server sees:
┌──────────┐           ┌──────────┐               ┌──────────┐
│src: 10.200.1.2│ ──► │src: 192.168.1.5│ ──────► │src: 192.168.1.5│
│dst: github.com│      │dst: github.com │          │dst: github.com │
└──────────┘           └──────────┘               └──────────┘

Server replies:         Host NAT rewrites back:    Sandbox receives:
┌──────────┐           ┌──────────┐               ┌──────────┐
│src: github.com│ ──► │src: github.com │ ──────► │src: github.com │
│dst: 192.168.1.5│     │dst: 10.200.1.2 │         │dst: 10.200.1.2 │
└──────────┘           └──────────┘               └──────────┘
```

**In AI Sandbox:**
```bash
iptables -t nat -A POSTROUTING -s 10.200.1.0/24 -j MASQUERADE
```
This tells the host: "For any packet coming from 10.200.1.x, replace the source IP with my own IP before sending it out."

---

## iptables

**iptables** is the traditional Linux firewall tool. It defines rules that determine what happens to network packets — allow, reject, drop, or modify them.

### Key Concepts

| Term | Meaning |
|------|---------|
| **Chain** | A list of rules checked in order (INPUT, OUTPUT, FORWARD) |
| **INPUT** | Rules for packets coming INTO the machine |
| **OUTPUT** | Rules for packets going OUT from the machine |
| **FORWARD** | Rules for packets being ROUTED through the machine |
| **Target** | What to do when a rule matches (ACCEPT, REJECT, DROP) |

### Rule Processing

```
Packet arrives → Check Rule 1 → No match
                 Check Rule 2 → No match
                 Check Rule 3 → MATCH → Apply target (ACCEPT/REJECT/DROP)
```

Rules are checked top-to-bottom. First match wins. This is why AI Sandbox uses `INSERT` (not `APPEND`) for whitelist rules — they must appear before the REJECT rules.

### REJECT vs DROP

| Action | Behavior | User Experience |
|--------|----------|-----------------|
| **DROP** | Silently discard packet | Tool hangs for 60+ seconds waiting for timeout |
| **REJECT** | Send error response back | Tool gets "Connection refused" immediately |

**In AI Sandbox:** We use REJECT with `tcp-reset` for TCP connections. This gives instant feedback instead of frustrating timeouts.

---

## seccomp (Secure Computing Mode)

**seccomp** is a Linux kernel security feature that restricts which **system calls** a process can make. It acts as a filter between the application and the kernel.

### What is a System Call (Syscall)?

A **syscall** is a request from a program to the kernel to perform a privileged operation. Programs cannot directly access hardware or kernel resources — they must ask the kernel through syscalls.

```
Application          Kernel
    │                  │
    ├── read()  ──────►│  Read from file
    ├── write() ──────►│  Write to file
    ├── socket()──────►│  Create network socket
    ├── chmod() ──────►│  Change file permissions
    ├── ptrace()──────►│  Debug another process
    │                  │
```

Every operation a program performs eventually becomes one or more syscalls.

### seccomp-BPF

**BPF (Berkeley Packet Filter)** is a small in-kernel virtual machine. seccomp-BPF uses BPF programs to create flexible syscall filtering rules.

```
Application calls chmod()
        │
        ▼
┌─────────────────────┐
│  seccomp-BPF Filter │
│                     │
│  Is syscall #268?   │──► No  ──► Check next rule
│  (fchmodat)         │
│         │           │
│        Yes          │
│         │           │
│  Return EPERM       │──► "Operation not permitted"
│  (block it)         │
└─────────────────────┘
```

**In AI Sandbox:** We build a BPF program that checks every syscall against the `blocked_syscalls` list from policy.yaml. Blocked syscalls return "Operation not permitted" without ever reaching the kernel.

---

## DNS (Domain Name System)

**DNS** translates human-readable domain names (like `github.com`) into IP addresses (like `20.207.73.82`) that computers use to communicate.

```
Application: "Connect to github.com"
     │
     ▼
DNS Resolver (8.8.8.8)
     │
     ▼
Answer: "github.com = 20.207.73.82"
     │
     ▼
Application connects to 20.207.73.82
```

**In AI Sandbox:** A new network namespace has no DNS configuration. We create a custom `/etc/resolv.conf` pointing to Google's DNS (8.8.8.8) and bind-mount it inside the sandbox. Without this, no domain names could be resolved.

---

## IP Forwarding

**IP forwarding** allows a Linux machine to act as a router, passing packets between network interfaces. Normally disabled — a machine only processes packets addressed to itself.

```bash
echo 1 > /proc/sys/net/ipv4/ip_forward
```

**In AI Sandbox:** The host machine must forward packets between `veth-host` (connected to sandbox) and the real network interface (connected to internet). Without IP forwarding, sandbox traffic would be stuck at the host.

---

## tmpfs (Temporary Filesystem)

**tmpfs** is a filesystem that lives entirely in RAM. It appears as a normal directory but has no persistent storage. Contents are lost when unmounted.

**In AI Sandbox:** We mount empty tmpfs filesystems over sensitive directories:
```c
mount("tmpfs", "/home/user/.ssh", "tmpfs", 0, "size=0");
```
This makes the directory appear empty inside the sandbox. The original files on disk are untouched and invisible to the sandbox.

---

## Bind Mount

A **bind mount** makes a file or directory visible at a different location in the filesystem. Unlike symbolic links, bind mounts work at the kernel level and are transparent to applications.

**In AI Sandbox:** We bind-mount `/dev/null` over sensitive files:
```c
mount("/dev/null", "/home/user/.env", NULL, MS_BIND, NULL);
```
Any read of the file returns nothing (since `/dev/null` is empty). The original file is safe on disk.

---

## fork()

**fork()** creates a copy of the current process. The original becomes the **parent** and the copy becomes the **child**. They run independently but share the same code.

```
Before fork():        After fork():
┌──────────┐         ┌──────────┐  ┌──────────┐
│  Process  │   ──►  │  Parent   │  │  Child    │
│  (ai-run) │        │  (host)   │  │ (sandbox) │
└──────────┘         └──────────┘  └──────────┘
```

**In AI Sandbox:** We fork because we need two processes in different namespaces:
- **Parent** stays in the host namespace to set up veth pair and NAT
- **Child** enters new namespaces to become the sandbox

---

## unshare()

**unshare()** is a Linux syscall that disassociates parts of the calling process's execution context. It creates new namespaces for the calling process.

```c
unshare(CLONE_NEWNS);   // New mount namespace
unshare(CLONE_NEWNET);  // New network namespace
```

After `unshare()`, the process has its own isolated view of the specified resource (filesystem or network).

---

## MASQUERADE

**MASQUERADE** is an iptables NAT target that automatically replaces the source IP of outgoing packets with the IP of the outgoing interface. Similar to SNAT but automatically detects the correct IP.

```bash
iptables -t nat -A POSTROUTING -s 10.200.1.0/24 -j MASQUERADE
```

**Analogy:** Like a receptionist forwarding your mail. Your internal address (10.200.1.2) is replaced with the building's public address. Replies come back to the building, and the receptionist routes them back to you.

---

## ICMP (Internet Control Message Protocol)

**ICMP** is a network protocol used for diagnostic and error reporting. The most common ICMP tool is `ping`.

**In AI Sandbox:** We allow ICMP traffic so that:
- `ping` works for basic network diagnostics
- Error messages (like "port unreachable") are delivered properly
- Network troubleshooting is possible inside the sandbox

---

## BPF (Berkeley Packet Filter)

**BPF** is a tiny virtual machine inside the Linux kernel. It was originally designed for network packet filtering but is now used for many purposes including seccomp.

A BPF program is a series of simple instructions:
```
LOAD  syscall_number        # Load the syscall number
CMP   268                   # Compare with fchmodat (268)
JEQ   block_it              # If equal, jump to block
ALLOW                       # Otherwise, allow

block_it:
RETURN ERRNO(EPERM)          # Return "Operation not permitted"
```

**In AI Sandbox:** We construct BPF programs dynamically based on the `blocked_syscalls` in policy.yaml. Each blocked syscall adds a compare-and-jump instruction to the program.

---

## conntrack (Connection Tracking)

**conntrack** is a Linux kernel module that tracks the state of network connections. It remembers established connections so that reply packets are automatically allowed.

```bash
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
```

**In AI Sandbox:** We allow ESTABLISHED and RELATED connections so that:
- Reply packets from whitelisted servers get through
- We only need to filter outgoing (OUTPUT) connections
- Once a connection is approved, the response is automatically allowed

---

## Quick Reference Table

| Term | One-Line Definition |
|------|-------------------|
| **Namespace** | Kernel feature that isolates system resources per process |
| **Mount Namespace** | Isolates the filesystem mount table |
| **Network Namespace** | Isolates the entire network stack |
| **Veth Pair** | Virtual ethernet cable connecting two namespaces |
| **NAT** | Translates private IPs to public IPs for internet access |
| **MASQUERADE** | Automatic NAT using the outgoing interface's IP |
| **iptables** | Linux firewall tool for packet filtering |
| **REJECT** | Firewall action that sends back an error (fast failure) |
| **DROP** | Firewall action that silently discards (causes timeout) |
| **seccomp** | Kernel feature to restrict allowed system calls |
| **BPF** | In-kernel virtual machine for packet/syscall filtering |
| **Syscall** | Request from program to kernel for privileged operations |
| **DNS** | Translates domain names to IP addresses |
| **tmpfs** | RAM-based temporary filesystem |
| **Bind Mount** | Makes a file visible at a different path |
| **fork()** | Creates a copy of the current process |
| **unshare()** | Creates new isolated namespaces for a process |
| **conntrack** | Tracks network connection states |
| **IP Forwarding** | Allows a machine to route packets between interfaces |
| **ICMP** | Protocol for network diagnostics (ping) |
