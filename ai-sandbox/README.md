# AI Sandbox - OS-Level Guardrail for AI Agents

A lightweight Linux sandbox that enforces least-privilege isolation for AI tools at the kernel level, preventing data exfiltration even if the AI is compromised.

## Problem

AI agents today run with full user permissions. If compromised via prompt injection, they can:
- Read sensitive files (SSH keys, `.env`, AWS credentials)
- Exfiltrate data to unauthorized servers
- Execute arbitrary commands

Current defenses (like `.cursorignore`) are application-level and easily bypassed.

## Solution

This project provides **OS-level isolation** using Linux kernel features:
- **Mount Namespaces**: Hide sensitive files/directories
- **Network Namespaces**: Isolate network stack
- **iptables Firewall**: Block unauthorized domains
- **YAML Policies**: User-configurable without recompiling

## Quick Start

### 1. Build
```bash
make
