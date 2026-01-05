#!/bin/bash
# AI Sandbox GUI Launcher
# Automatically sets up Python environment and launches dashboard

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Paths
DASHBOARD_DIR="/usr/share/ai-sandbox/dashboard"
VENV_DIR="/var/lib/ai-sandbox/venv"

# Check if dashboard exists
if [ ! -f "${DASHBOARD_DIR}/app.py" ]; then
    echo -e "${YELLOW}Dashboard not found. Please run: sudo ./install.sh${NC}"
    exit 1
fi

# Setup Python virtual environment if needed
setup_venv() {
    if [ ! -d "${VENV_DIR}" ]; then
        echo -e "${CYAN}[1/3] Creating Python virtual environment...${NC}"
        python3 -m venv "${VENV_DIR}"
    fi
    
    # Activate venv
    source "${VENV_DIR}/bin/activate"
    
    # Check if streamlit is installed
    if ! python3 -c "import streamlit" 2>/dev/null; then
        echo -e "${CYAN}[2/3] Installing dependencies (first run only)...${NC}"
        pip install --quiet --upgrade pip
        pip install --quiet -r "${DASHBOARD_DIR}/requirements.txt"
        echo -e "${GREEN}✓ Dependencies installed${NC}"
    fi
}

# Main
echo -e "${GREEN}"
echo "╔═══════════════════════════════════════╗"
echo "║     🔒 AI Sandbox Dashboard           ║"
echo "╚═══════════════════════════════════════╝"
echo -e "${NC}"

# Need sudo for venv in /var/lib
if [ "$EUID" -ne 0 ] && [ ! -d "${VENV_DIR}" ]; then
    echo -e "${YELLOW}First run requires sudo to setup Python environment${NC}"
    echo "Run: sudo ai-run gui"
    exit 1
fi

# Setup venv (only first time needs sudo)
setup_venv

echo -e "${CYAN}[3/3] Launching dashboard...${NC}"
echo ""
echo -e "${GREEN}Dashboard URL: http://localhost:8501${NC}"
echo -e "${YELLOW}Press Ctrl+C to stop${NC}"
echo ""

# Launch streamlit
cd "${DASHBOARD_DIR}"
streamlit run app.py --server.headless true --browser.gatherUsageStats false
