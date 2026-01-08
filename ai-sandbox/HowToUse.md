# AI Sandbox - How To Use

A security tool that creates isolated execution environments for AI agents with file protection and network control.

---

## Quick Start

### Step 1: Install

```bash
cd /path/to/ai-sandbox

# Build the project
make

# Install system-wide (one-time setup)
sudo ./install.sh
```

### Step 2: Create a Sandbox in Your Project

```bash
# Go to any project folder
cd ~/my-ai-project

# Create a policy file
ai-run create
# Creates: policy.yaml
```

### Step 3: Configure Policy (Optional)

Edit `policy.yaml` to customize:

```yaml
# Files to hide from AI
protected_files:
  - ~/.ssh
  - ~/.aws
  - ~/.gnupg

# Allowed domains (others are blocked)
network_whitelist:
  - github.com
  - api.github.com
  - pypi.org

# DENY = whitelist only, ALLOW = no restrictions
default_network_policy: DENY

# Set true to allow all HTTPS (bypass whitelist)
allow_all_https: false

# System calls to block (advanced)
blocked_syscalls:
  - ptrace    # Prevents debugging/tracing
```

### Step 4: Start Sandbox

```bash
sudo ai-run run policy.yaml
```

You're now inside an isolated environment:

- 🔒 Protected files are hidden
- 🌐 Only whitelisted domains are accessible
- ⚡ Blocked connections fail instantly
- 🛡️ Blocked syscalls return "Operation not permitted"

### Step 5: Exit Sandbox

```bash
exit
```

---

## 📋 Command Reference

| Command               | Description                             | Requires sudo   |
| --------------------- | --------------------------------------- | --------------- |
| `ai-run create`       | Create policy.yaml in current directory | No              |
| `ai-run run <policy>` | Start sandbox with policy               | Yes             |
| `ai-run gui`          | Open web dashboard                      | Yes (first run) |
| `ai-run list`         | Show active sandbox sessions            | No              |
| `ai-run destroy`      | Cleanup leftover resources              | Yes             |

---

## 🖥️ Web Dashboard

Launch the graphical interface:

```bash
sudo ai-run gui
```

Open http://localhost:8501 in your browser.

**Features:**

- View active sandboxes
- Edit policy files visually
- Quick create/destroy actions
- Built-in documentation

---

## 📁 Example: Sandboxing an AI Agent

```bash
# 1. Create project folder
mkdir ~/ai-agent
cd ~/ai-agent

# 2. Initialize sandbox policy
ai-run create

# 3. Add your AI agent's required domains
nano policy.yaml
# Add: api.openai.com, huggingface.co, etc.

# 4. Start sandbox
sudo ai-run run policy.yaml

# 5. Run your AI agent (inside sandbox)
python agent.py

# 6. When done
exit
```

---

## 🔒 What Gets Protected

### Files (Hidden from sandbox)

| Default Protected | Description       |
| ----------------- | ----------------- |
| `~/.ssh`          | SSH keys          |
| `~/.aws`          | AWS credentials   |
| `~/.gnupg`        | GPG keys          |
| `~/.env`          | Environment files |
| `~/.config/gh`    | GitHub CLI tokens |

### Network (Blocked by default)

- All outbound connections except whitelisted domains
- Blocked connections fail immediately (no timeout)

### System Calls (Seccomp)

| Default Blocked | Description                    |
| --------------- | ------------------------------ |
| `ptrace`        | Prevents debugging/tracing     |

You can add more syscalls to block in `policy.yaml`:

```yaml
blocked_syscalls:
  - ptrace
  - reboot    # Prevent system reboot
  - mount     # Prevent mounting filesystems
```

---

## 🌐 Network Whitelist Examples

### For Python Development

```yaml
network_whitelist:
  - pypi.org
  - files.pythonhosted.org
  - github.com
```

### For Node.js Development

```yaml
network_whitelist:
  - registry.npmjs.org
  - github.com
```

### For AI/ML Agents

```yaml
network_whitelist:
  - api.openai.com
  - huggingface.co
  - github.com
```

### Allow Everything (Testing)

```yaml
allow_all_https: true
```

---

## 🛡️ Syscall Blocking Examples

### Block Debugging (Recommended)

```yaml
blocked_syscalls:
  - ptrace    # Blocks strace, gdb, etc.
```

### High Security Mode

```yaml
blocked_syscalls:
  - ptrace
  - mount
  - umount2
  - reboot
  - swapon
  - swapoff
```

---

## 🛠️ Troubleshooting

### "ai-run: command not found"

```bash
# Run install script
cd /path/to/ai-sandbox
sudo ./install.sh
```

### "Permission denied"

```bash
# Use sudo for sandbox operations
sudo ai-run run policy.yaml
```

### Connections hang instead of failing fast

```bash
# Rebuild and reinstall
cd /path/to/ai-sandbox
make clean && make
sudo ./install.sh
```

### Dashboard won't start

```bash
# Ensure Python 3 is installed
python3 --version

# Re-run with sudo (first time only)
sudo ai-run gui
```

---

## 📂 File Locations

| Path                               | Description              |
| ---------------------------------- | ------------------------ |
| `/usr/local/bin/ai-run`            | Main binary              |
| `/usr/local/bin/ai-sandbox-gui`    | Dashboard launcher       |
| `/etc/ai-sandbox/`                 | Default config templates |
| `/var/lib/ai-sandbox/`             | State and Python venv    |
| `/usr/share/ai-sandbox/dashboard/` | Web dashboard source     |

---

## 🔄 Updating

```bash
cd /path/to/ai-sandbox
git pull  # If using git
make clean && make
sudo ./install.sh
```

---

## 📖 How It Works

1. **Mount Namespace** - Creates isolated filesystem view
2. **Network Namespace** - Creates isolated network stack
3. **veth Pair** - Bridge between sandbox and host network
4. **NAT** - Translates sandbox IPs for internet access
5. **iptables** - Enforces network whitelist with fast REJECT
6. **DNS** - Configured to use 8.8.8.8 for resolution

---

## 🆘 Getting Help

```bash
ai-run --help
ai-run gui  # Then click "Help" in dashboard
```

---

Made with ❤️ for AI Safety
