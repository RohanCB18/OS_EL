# AI Sandbox - Demonstration Guide

This guide walks through a live demo of AI Sandbox showing all three security layers:
1. **Network Blocking** - Prevent unauthorized git push
2. **File Access Blocking** - Hide sensitive files
3. **Syscall Blocking** - Prevent permission changes

---

## Prerequisites

```bash
# Build and ensure ai-run is available
cd ~/OS_EL/ai-sandbox
make clean && make

# Prepare a test directory with a git repo
cd ~/OS_EL/tmp
git init
git remote set-url origin https://github.com/RaghottamNadgoudar/ForTest.git
echo "test content" > test.txt
```

**Policy files needed** (already created in `~/OS_EL/ai-sandbox/`):
- `policy_deny.yaml` - GitHub NOT whitelisted
- `policy_allow.yaml` - GitHub IS whitelisted
- `policy.yaml` - Full policy with syscall blocking

---

## Demo 1: Network Blocking (git push)

### Step 1A: Show git push BLOCKED

```bash
cd ~/OS_EL/tmp
sudo ai-run run ../ai-sandbox/policy_deny.yaml
```

Output shows:
```
Whitelisted hosts (2):
    - pypi.org
    - registry.npmjs.org
```
GitHub is NOT in the whitelist.

```bash
# Configure git identity (required inside sandbox)
git config --global user.email "raghottam.nadgoudar2006p@gmail.com"
git config --global user.name "Raghottam Nadgoudar"

# Try to push
git add .
git commit -m "Test commit"
git push origin main
```

**Expected output:**
```
fatal: unable to access 'https://github.com/...': 
Failed to connect to github.com port 443: Connection refused
```

The AI agent cannot push code to GitHub. Connection fails fast (not a timeout).

```bash
exit
```

### Step 1B: Show git push ALLOWED

```bash
sudo ai-run run ../ai-sandbox/policy_allow.yaml
```

Output shows:
```
Whitelisted hosts (4):
    - github.com         <-- NOW INCLUDED
    - api.github.com     <-- NOW INCLUDED
    - pypi.org
    - registry.npmjs.org
```

```bash
git config --global user.email "raghottam.nadgoudar2006p@gmail.com"
git config --global user.name "Raghottam Nadgoudar"

git push origin main
```

**Expected output:**
```
Writing objects: 100% (143/143), 98.31 KiB | 7.02 MiB/s, done.
To https://github.com/RaghottamNadgoudar/ForTest.git
 * [new branch]      main -> main
```

Same command, different policy = different result. The sandbox allows controlled access.

```bash
exit
```

### Key Talking Point
> "Same git push command, but the outcome depends entirely on the policy. An AI agent can only access services we explicitly whitelist."

---

## Demo 2: File Access Blocking

```bash
sudo ai-run run ../ai-sandbox/policy_allow.yaml
```

Output shows:
```
[+] Hiding: /home/raghottam/.ssh
[+] Successfully hidden: /home/raghottam/.ssh
[+] Hiding: /home/raghottam/.aws
[+] Successfully hidden: /home/raghottam/.aws
```

```bash
# Try to access sensitive files
ls ~/.ssh
# Output: (empty - directory appears empty)

cat ~/.ssh/id_rsa
# Output: No such file or directory

ls ~/.aws
# Output: (empty)

# Regular files still work fine
echo "hello" > test.txt
cat test.txt
# Output: hello
```

```bash
exit
```

### Key Talking Point
> "SSH keys, AWS credentials, and other sensitive files are completely invisible inside the sandbox. An AI agent cannot read or exfiltrate them."

---

## Demo 3: Syscall Blocking (chmod)

```bash
sudo ai-run run ../ai-sandbox/policy.yaml
```

Output shows:
```
[+] Setting up seccomp filter (4 syscalls to block)...
    -> Blocked: ptrace (syscall #101)
    -> Blocked: chmod (syscall #90)
    -> Blocked: fchmod (syscall #91)
    -> Blocked: fchmodat (syscall #268)
[+] Seccomp filter loaded: 4 syscalls blocked
```

```bash
# Try to change file permissions
chmod 777 test.txt
# Expected: Operation not permitted

# Try to debug/trace a process
strace ls
# Expected: strace: ptrace(PTRACE_TRACEME, ...): Operation not permitted

# Regular operations still work fine
echo "hello" > test.txt
cat test.txt
# Output: hello
```

```bash
exit
```

### Key Talking Point
> "The sandbox blocks dangerous system calls at the kernel level using seccomp. The AI cannot change file permissions or debug processes, even though it runs as root."

---

## Full Demo Flow (5 minutes)

| Step | Time | Action | Shows |
|------|------|--------|-------|
| 1 | 0:00 | Start with `policy_deny.yaml` | Sandbox setup output |
| 2 | 0:30 | `git push origin main` → FAILS | Network blocking |
| 3 | 1:00 | `exit`, start `policy_allow.yaml` | Policy swap |
| 4 | 1:30 | `git push origin main` → WORKS | Whitelist control |
| 5 | 2:00 | `ls ~/.ssh` → empty | File protection |
| 6 | 2:30 | `cat ~/.ssh/id_rsa` → not found | Credential hiding |
| 7 | 3:00 | `exit`, start `policy.yaml` | Syscall demo |
| 8 | 3:30 | `chmod 777 test.txt` → denied | Syscall blocking |
| 9 | 4:00 | `strace ls` → denied | ptrace blocking |
| 10 | 4:30 | Show `policy.yaml` file | Policy configuration |

---

## Summary Slide Points

1. **Three layers of protection:**
   - Network isolation (iptables + namespaces)
   - File system hiding (mount namespaces)
   - Syscall filtering (seccomp-BPF)

2. **Policy-driven** - Simple YAML configuration
3. **Fast failure** - REJECT, not DROP (instant feedback)
4. **Real-world scenario** - Prevents code exfiltration via git
