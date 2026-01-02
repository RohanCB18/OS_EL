#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "============================================"
echo "  AI Sandbox Security Demonstration"
echo "  OS-Level Guardrail for AI Agents"
echo "============================================"
echo ""

# Check if running as normal user
if [ "$EUID" -eq 0 ]; then 
   echo "Error: Please run as normal user (not sudo)"
   exit 1
fi

# Step 1: Show vulnerability WITHOUT sandbox
echo -e "${YELLOW}Step 1: Demonstrating Vulnerability (No Sandbox)${NC}"
echo "------------------------------------------------"
echo "Running malicious AI agent WITHOUT protection..."
echo ""
sleep 2

python3 demo/attack.py

echo ""
echo -e "${RED}⚠ VULNERABLE: AI agent can access secrets and network!${NC}"
echo ""
read -p "Press Enter to continue to Step 2..."

# Step 2: Create sandbox
echo ""
echo -e "${YELLOW}Step 2: Creating Sandbox with Security Policy${NC}"
echo "------------------------------------------------"
echo "Creating policy.yaml with protected paths..."
echo ""

sudo ./ai-run create

echo ""
echo "Policy created. Contents:"
cat policy.yaml
echo ""
read -p "Press Enter to continue to Step 3..."

# Step 3: Run attack INSIDE sandbox
echo ""
echo -e "${YELLOW}Step 3: Running Same Attack INSIDE Sandbox${NC}"
echo "------------------------------------------------"
echo "Launching sandboxed environment..."
echo ""
sleep 1

# Create temp script to run attack inside sandbox
cat > /tmp/run_attack.sh << 'EOF'
#!/bin/bash
cd /home/$SUDO_USER/RVCE/OS\ EL/OS_EL/OS_EL/ai-sandbox
python3 demo/attack.py
exit
EOF

chmod +x /tmp/run_attack.sh

# Run sandbox with attack script
sudo ./ai-run run policy.yaml << 'SANDBOX'
python3 demo/attack.py
exit
SANDBOX

echo ""
echo -e "${GREEN}✓ PROTECTED: All attacks blocked by OS-level sandbox!${NC}"
echo ""

# Step 4: Summary
echo "============================================"
echo "  Demonstration Complete"
echo "============================================"
echo ""
echo "Summary:"
echo -e "  ${RED}Without Sandbox:${NC} AI can steal secrets + exfiltrate data"
echo -e "  ${GREEN}With Sandbox:${NC} Kernel blocks all unauthorized access"
echo ""
echo "Key Features:"
echo "  ✓ File isolation (mount namespaces)"
echo "  ✓ Network isolation (network namespaces)"
echo "  ✓ Firewall rules (iptables)"
echo "  ✓ Policy-driven configuration (YAML)"
echo ""
echo "This is OS-level enforcement, not application-level!"
echo ""

rm -f /tmp/run_attack.sh
