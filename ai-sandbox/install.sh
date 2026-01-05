#!/bin/bash
# AI Sandbox Installation Script
# Installs ai-run system-wide so it can be used from any directory

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${GREEN}"
echo "╔═══════════════════════════════════════╗"
echo "║     🔒 AI Sandbox Installer           ║"
echo "╚═══════════════════════════════════════╝"
echo -e "${NC}"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Error: Please run as root (sudo ./install.sh)${NC}"
    exit 1
fi

# Directories
INSTALL_DIR="/usr/local/bin"
STATE_DIR="/var/lib/ai-sandbox"
CONFIG_DIR="/etc/ai-sandbox"
DASHBOARD_DIR="/usr/share/ai-sandbox/dashboard"
SCRIPTS_DIR="/usr/share/ai-sandbox/scripts"

# Get source directory
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Build first
echo -e "${CYAN}[1/7] Building ai-run...${NC}"
cd "${SOURCE_DIR}"
make clean
make

if [ ! -f "ai-run" ]; then
    echo -e "${RED}Error: Build failed, ai-run binary not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Build complete${NC}"

# Install binary
echo -e "${CYAN}[2/7] Installing ai-run to ${INSTALL_DIR}...${NC}"
cp ai-run "${INSTALL_DIR}/ai-run"
chmod 755 "${INSTALL_DIR}/ai-run"
echo -e "${GREEN}✓ Binary installed${NC}"

# Create state directory
echo -e "${CYAN}[3/7] Creating state directory ${STATE_DIR}...${NC}"
mkdir -p "${STATE_DIR}"
chmod 755 "${STATE_DIR}"

# Initialize empty sessions file
if [ ! -f "${STATE_DIR}/sessions.json" ]; then
    echo '{"sessions":[]}' > "${STATE_DIR}/sessions.json"
    chmod 644 "${STATE_DIR}/sessions.json"
fi
echo -e "${GREEN}✓ State directory created${NC}"

# Create config directory with default policy
echo -e "${CYAN}[4/7] Creating config directory ${CONFIG_DIR}...${NC}"
mkdir -p "${CONFIG_DIR}"

# Create default policy template
cat > "${CONFIG_DIR}/default-policy.yaml" << 'EOF'
# AI Sandbox Default Security Policy
# Copy this file to your project directory and customize

# Files/directories to hide from the sandbox
protected_files:
  - ~/.ssh
  - ~/.env
  - ~/.aws
  - ~/.gnupg
  - ~/.config/gh

# Network policy: DENY (whitelist only) or ALLOW (all)
default_network_policy: DENY

# Whitelisted domains/IPs (when policy is DENY)
network_whitelist:
  - github.com
  - api.github.com
  - pypi.org
  - registry.npmjs.org

# Set to true to allow all HTTPS regardless of whitelist
allow_all_https: false
EOF

chmod 644 "${CONFIG_DIR}/default-policy.yaml"
echo -e "${GREEN}✓ Config directory created${NC}"

# Install dashboard
echo -e "${CYAN}[5/7] Installing dashboard to ${DASHBOARD_DIR}...${NC}"
mkdir -p "${DASHBOARD_DIR}"
cp -r "${SOURCE_DIR}/dashboard/"* "${DASHBOARD_DIR}/"
chmod -R 755 "${DASHBOARD_DIR}"
echo -e "${GREEN}✓ Dashboard installed${NC}"

# Install GUI launcher script
echo -e "${CYAN}[6/7] Installing GUI launcher...${NC}"
mkdir -p "${SCRIPTS_DIR}"

# Copy and make executable
if [ -f "${SOURCE_DIR}/scripts/ai-sandbox-gui.sh" ]; then
    cp "${SOURCE_DIR}/scripts/ai-sandbox-gui.sh" "${SCRIPTS_DIR}/"
fi

# Create the ai-sandbox-gui command
cat > "${INSTALL_DIR}/ai-sandbox-gui" << 'LAUNCHER'
#!/bin/bash
# AI Sandbox GUI Launcher

DASHBOARD_DIR="/usr/share/ai-sandbox/dashboard"
VENV_DIR="/var/lib/ai-sandbox/venv"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${GREEN}"
echo "╔═══════════════════════════════════════╗"
echo "║     🔒 AI Sandbox Dashboard           ║"
echo "╚═══════════════════════════════════════╝"
echo -e "${NC}"

# Check dashboard exists
if [ ! -f "${DASHBOARD_DIR}/app.py" ]; then
    echo -e "${YELLOW}Dashboard not found. Please reinstall: sudo ./install.sh${NC}"
    exit 1
fi

# Setup venv if needed
if [ ! -d "${VENV_DIR}" ]; then
    echo -e "${CYAN}[1/3] Creating Python virtual environment...${NC}"
    if [ "$EUID" -ne 0 ]; then
        echo -e "${YELLOW}First run requires sudo: sudo ai-run gui${NC}"
        exit 1
    fi
    python3 -m venv "${VENV_DIR}"
    chown -R ${SUDO_USER:-$USER}:${SUDO_USER:-$USER} "${VENV_DIR}" 2>/dev/null || true
fi

# Activate and check deps
source "${VENV_DIR}/bin/activate"

if ! python3 -c "import streamlit" 2>/dev/null; then
    echo -e "${CYAN}[2/3] Installing dependencies (first run)...${NC}"
    pip install --quiet --upgrade pip
    pip install --quiet -r "${DASHBOARD_DIR}/requirements.txt"
    echo -e "${GREEN}✓ Dependencies installed${NC}"
fi

echo -e "${CYAN}[3/3] Launching dashboard...${NC}"
echo ""
echo -e "${GREEN}🌐 Dashboard URL: http://localhost:8501${NC}"
echo -e "${YELLOW}Press Ctrl+C to stop${NC}"
echo ""

cd "${DASHBOARD_DIR}"
exec streamlit run app.py --server.headless true --browser.gatherUsageStats false
LAUNCHER

chmod 755 "${INSTALL_DIR}/ai-sandbox-gui"
echo -e "${GREEN}✓ GUI launcher installed${NC}"

# Verify installation
echo -e "${CYAN}[7/7] Verifying installation...${NC}"
if command -v ai-run &> /dev/null && command -v ai-sandbox-gui &> /dev/null; then
    echo ""
    echo -e "${GREEN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}✓ Installation complete!${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}"
    echo ""
    echo -e "${CYAN}Quick Start:${NC}"
    echo ""
    echo "  ${YELLOW}1. Create a sandbox in any folder:${NC}"
    echo "     cd ~/my-project"
    echo "     ai-run create"
    echo ""
    echo "  ${YELLOW}2. Edit policy.yaml to customize${NC}"
    echo ""
    echo "  ${YELLOW}3. Start sandbox:${NC}"
    echo "     sudo ai-run run policy.yaml"
    echo ""
    echo "  ${YELLOW}4. Open dashboard (GUI):${NC}"
    echo "     sudo ai-run gui"
    echo "     # or: ai-sandbox-gui"
    echo ""
    echo -e "${CYAN}Installed files:${NC}"
    echo "  Binary:     ${INSTALL_DIR}/ai-run"
    echo "  GUI:        ${INSTALL_DIR}/ai-sandbox-gui"
    echo "  Dashboard:  ${DASHBOARD_DIR}/"
    echo "  State:      ${STATE_DIR}/"
    echo "  Config:     ${CONFIG_DIR}/"
    echo ""
else
    echo -e "${RED}Error: Installation verification failed${NC}"
    exit 1
fi
