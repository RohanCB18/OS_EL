"""
AI Sandbox Dashboard - Streamlit Application
A modern, user-friendly interface for managing AI sandboxes
"""

import streamlit as st
import json
import os
import subprocess
import yaml
from pathlib import Path
from datetime import datetime

# Configuration
STATE_FILE = "/var/lib/ai-sandbox/sessions.json"
DEFAULT_POLICY_PATH = "/etc/ai-sandbox/default-policy.yaml"

# Page configuration
st.set_page_config(
    page_title="AI Sandbox Dashboard",
    page_icon="🔒",
    layout="wide",
    initial_sidebar_state="expanded"
)

# Custom CSS for modern dark theme
st.markdown("""
<style>
    /* Dark theme with gradient accents */
    .stApp {
        background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
    }
    
    /* Card styling */
    .sandbox-card {
        background: linear-gradient(145deg, #1e2a4a, #182035);
        border-radius: 16px;
        padding: 24px;
        margin: 12px 0;
        border: 1px solid rgba(99, 102, 241, 0.2);
        box-shadow: 0 4px 20px rgba(0, 0, 0, 0.3);
    }
    
    .sandbox-card:hover {
        border-color: rgba(99, 102, 241, 0.5);
        transform: translateY(-2px);
        transition: all 0.3s ease;
    }
    
    /* Status indicators */
    .status-running {
        color: #10b981;
        font-weight: 600;
    }
    
    .status-stopped {
        color: #ef4444;
        font-weight: 600;
    }
    
    /* Headers */
    .main-header {
        background: linear-gradient(90deg, #6366f1, #8b5cf6);
        -webkit-background-clip: text;
        -webkit-text-fill-color: transparent;
        font-size: 2.5rem;
        font-weight: 700;
        margin-bottom: 0.5rem;
    }
    
    /* Metric cards */
    .metric-card {
        background: rgba(99, 102, 241, 0.1);
        border-radius: 12px;
        padding: 20px;
        text-align: center;
        border: 1px solid rgba(99, 102, 241, 0.2);
    }
    
    .metric-value {
        font-size: 2rem;
        font-weight: 700;
        color: #6366f1;
    }
    
    .metric-label {
        color: #94a3b8;
        font-size: 0.875rem;
    }
    
    /* Button styling */
    .stButton > button {
        background: linear-gradient(90deg, #6366f1, #8b5cf6);
        color: white;
        border: none;
        border-radius: 8px;
        padding: 0.5rem 1.5rem;
        font-weight: 600;
        transition: all 0.3s ease;
    }
    
    .stButton > button:hover {
        transform: translateY(-2px);
        box-shadow: 0 4px 12px rgba(99, 102, 241, 0.4);
    }
    
    /* Sidebar styling */
    section[data-testid="stSidebar"] {
        background: linear-gradient(180deg, #1e2a4a 0%, #182035 100%);
        border-right: 1px solid rgba(99, 102, 241, 0.2);
    }
    
    /* Code blocks */
    .stCodeBlock {
        background: #0d1117 !important;
        border-radius: 8px;
    }
    
    /* Hide Streamlit branding */
    #MainMenu {visibility: hidden;}
    footer {visibility: hidden;}
</style>
""", unsafe_allow_html=True)


def load_sessions():
    """Load active sessions from state file"""
    try:
        if os.path.exists(STATE_FILE):
            with open(STATE_FILE, 'r') as f:
                data = json.load(f)
                return data.get('sessions', [])
    except (json.JSONDecodeError, PermissionError) as e:
        st.warning(f"Could not read sessions: {e}")
    return []


def load_policy(policy_path):
    """Load policy YAML file"""
    try:
        if os.path.exists(policy_path):
            with open(policy_path, 'r') as f:
                return yaml.safe_load(f)
    except Exception as e:
        st.error(f"Error loading policy: {e}")
    return None


def save_policy(policy_path, content):
    """Save policy YAML file"""
    try:
        with open(policy_path, 'w') as f:
            yaml.dump(content, f, default_flow_style=False, sort_keys=False)
        return True
    except Exception as e:
        st.error(f"Error saving policy: {e}")
        return False


def get_policy_files():
    """Find policy files in common locations"""
    policies = []
    
    # Check current directory and common locations
    search_paths = [
        Path.cwd(),
        Path.home(),
        Path("/etc/ai-sandbox"),
        Path.home() / "OS_EL" / "ai-sandbox"
    ]
    
    for path in search_paths:
        if path.exists():
            for policy_file in path.glob("**/policy.yaml"):
                policies.append(str(policy_file))
    
    return list(set(policies))[:10]  # Limit to 10


def render_sidebar():
    """Render sidebar navigation"""
    with st.sidebar:
        st.markdown("## 🔒 AI Sandbox")
        st.markdown("---")
        
        page = st.radio(
            "Navigation",
            ["📊 Dashboard", "📝 Policy Editor", "🚀 Quick Actions", "ℹ️ Help"],
            label_visibility="collapsed"
        )
        
        st.markdown("---")
        
        # Quick stats
        sessions = load_sessions()
        active_count = len([s for s in sessions if s.get('status') == 'running'])
        
        st.markdown("### Quick Stats")
        st.metric("Active Sandboxes", active_count)
        
        st.markdown("---")
        st.markdown("Made with ❤️ for AI Safety")
        
        return page


def render_dashboard():
    """Render main dashboard view"""
    st.markdown('<p class="main-header">🔒 AI Sandbox Dashboard</p>', unsafe_allow_html=True)
    st.markdown("Monitor and manage your isolated AI execution environments")
    
    # Metrics row
    sessions = load_sessions()
    active = len([s for s in sessions if s.get('status') == 'running'])
    
    col1, col2, col3, col4 = st.columns(4)
    
    with col1:
        st.markdown("""
        <div class="metric-card">
            <div class="metric-value">{}</div>
            <div class="metric-label">Active Sandboxes</div>
        </div>
        """.format(active), unsafe_allow_html=True)
    
    with col2:
        policies = get_policy_files()
        st.markdown("""
        <div class="metric-card">
            <div class="metric-value">{}</div>
            <div class="metric-label">Policy Files</div>
        </div>
        """.format(len(policies)), unsafe_allow_html=True)
    
    with col3:
        st.markdown("""
        <div class="metric-card">
            <div class="metric-value">✓</div>
            <div class="metric-label">System Status</div>
        </div>
        """, unsafe_allow_html=True)
    
    with col4:
        st.markdown("""
        <div class="metric-card">
            <div class="metric-value">DENY</div>
            <div class="metric-label">Default Policy</div>
        </div>
        """, unsafe_allow_html=True)
    
    st.markdown("---")
    
    # Active sandboxes
    st.markdown("### 🖥️ Active Sandbox Sessions")
    
    if not sessions or active == 0:
        st.info("No active sandboxes. Start one from the Quick Actions page!")
    else:
        for session in sessions:
            if session.get('status') == 'running':
                with st.container():
                    st.markdown(f"""
                    <div class="sandbox-card">
                        <h4>🔒 Sandbox PID: {session.get('pid', 'N/A')}</h4>
                        <p><strong>User:</strong> {session.get('user', 'unknown')}</p>
                        <p><strong>Policy:</strong> <code>{session.get('policy', 'N/A')}</code></p>
                        <p><strong>Directory:</strong> <code>{session.get('cwd', 'N/A')}</code></p>
                        <p><strong>Started:</strong> {session.get('started', 'N/A')}</p>
                        <p><span class="status-running">● Running</span></p>
                    </div>
                    """, unsafe_allow_html=True)
    
    # Refresh button
    if st.button("🔄 Refresh"):
        st.rerun()


def render_policy_editor():
    """Render policy editor view"""
    st.markdown('<p class="main-header">📝 Policy Editor</p>', unsafe_allow_html=True)
    st.markdown("View and edit sandbox security policies")
    
    # Policy file selector
    policies = get_policy_files()
    
    col1, col2 = st.columns([3, 1])
    
    with col1:
        if policies:
            selected_policy = st.selectbox(
                "Select Policy File",
                policies,
                format_func=lambda x: os.path.basename(x) + f" ({os.path.dirname(x)})"
            )
        else:
            selected_policy = st.text_input(
                "Policy File Path",
                value=str(Path.cwd() / "policy.yaml")
            )
    
    with col2:
        st.markdown("<br>", unsafe_allow_html=True)
        if st.button("📂 Browse..."):
            st.info("Enter the path manually above")
    
    if selected_policy and os.path.exists(selected_policy):
        policy = load_policy(selected_policy)
        
        if policy:
            st.markdown("---")
            
            # Visual editor
            tab1, tab2 = st.tabs(["🎨 Visual Editor", "📄 Raw YAML"])
            
            with tab1:
                col1, col2 = st.columns(2)
                
                with col1:
                    st.markdown("#### 📁 Protected Files")
                    protected_files = policy.get('protected_files', [])
                    
                    for i, pf in enumerate(protected_files):
                        st.text_input(f"Path {i+1}", value=pf, key=f"pf_{i}", disabled=True)
                    
                    new_protected = st.text_input("Add new protected path", key="new_protected")
                    if st.button("➕ Add Path") and new_protected:
                        protected_files.append(new_protected)
                        policy['protected_files'] = protected_files
                        save_policy(selected_policy, policy)
                        st.rerun()
                
                with col2:
                    st.markdown("#### 🌐 Network Whitelist")
                    whitelist = policy.get('network_whitelist', [])
                    
                    for i, domain in enumerate(whitelist):
                        col_d, col_x = st.columns([4, 1])
                        with col_d:
                            st.text_input(f"Domain {i+1}", value=domain, key=f"wl_{i}", disabled=True)
                    
                    new_domain = st.text_input("Add new domain/IP", key="new_domain")
                    if st.button("➕ Add Domain") and new_domain:
                        whitelist.append(new_domain)
                        policy['network_whitelist'] = whitelist
                        save_policy(selected_policy, policy)
                        st.rerun()
                
                st.markdown("---")
                
                # Policy toggles
                col1, col2 = st.columns(2)
                
                with col1:
                    net_policy = st.selectbox(
                        "Default Network Policy",
                        ["DENY", "ALLOW"],
                        index=0 if policy.get('default_network_policy', 'DENY') == 'DENY' else 1
                    )
                
                with col2:
                    allow_https = st.checkbox(
                        "Allow All HTTPS",
                        value=policy.get('allow_all_https', False)
                    )
                
                if st.button("💾 Save Changes"):
                    policy['default_network_policy'] = net_policy
                    policy['allow_all_https'] = allow_https
                    if save_policy(selected_policy, policy):
                        st.success("Policy saved successfully!")
            
            with tab2:
                # Raw YAML editor
                with open(selected_policy, 'r') as f:
                    raw_yaml = f.read()
                
                edited_yaml = st.text_area(
                    "YAML Content",
                    value=raw_yaml,
                    height=400,
                    key="yaml_editor"
                )
                
                if st.button("💾 Save YAML"):
                    try:
                        # Validate YAML
                        yaml.safe_load(edited_yaml)
                        with open(selected_policy, 'w') as f:
                            f.write(edited_yaml)
                        st.success("Policy saved!")
                    except yaml.YAMLError as e:
                        st.error(f"Invalid YAML: {e}")
    else:
        st.warning("Policy file not found. Create one using the Quick Actions page.")


def render_quick_actions():
    """Render quick actions view"""
    st.markdown('<p class="main-header">🚀 Quick Actions</p>', unsafe_allow_html=True)
    st.markdown("Common operations for managing AI sandboxes")
    
    st.markdown("---")
    
    col1, col2 = st.columns(2)
    
    with col1:
        st.markdown("### 📝 Create New Policy")
        st.markdown("Generate a default policy.yaml in the specified directory")
        
        create_dir = st.text_input(
            "Directory",
            value=str(Path.home()),
            key="create_dir"
        )
        
        if st.button("Create Policy", key="btn_create"):
            try:
                result = subprocess.run(
                    ["ai-run", "create"],
                    cwd=create_dir,
                    capture_output=True,
                    text=True,
                    timeout=10
                )
                if result.returncode == 0:
                    st.success(f"✅ Policy created in {create_dir}")
                    st.code(result.stdout)
                else:
                    st.error(f"Failed: {result.stderr}")
            except FileNotFoundError:
                st.error("ai-run not found. Please run install.sh first.")
            except Exception as e:
                st.error(f"Error: {e}")
    
    with col2:
        st.markdown("### 🧹 Cleanup Resources")
        st.markdown("Remove leftover network interfaces and resources")
        
        if st.button("🧹 Run Cleanup", key="btn_cleanup"):
            try:
                result = subprocess.run(
                    ["sudo", "ai-run", "destroy"],
                    capture_output=True,
                    text=True,
                    timeout=10
                )
                st.success("✅ Cleanup completed")
                st.code(result.stdout or "No output")
            except Exception as e:
                st.error(f"Error: {e}")
    
    st.markdown("---")
    
    st.markdown("### 🖥️ Start Sandbox")
    st.markdown("Launch a new sandbox session (opens in terminal)")
    
    policies = get_policy_files()
    
    if policies:
        selected = st.selectbox("Select Policy", policies, key="run_policy")
        
        st.code(f"sudo ai-run run {selected}", language="bash")
        
        st.info("💡 Run this command in your terminal to start the sandbox")
    else:
        st.warning("No policy files found. Create one first!")
    
    st.markdown("---")
    
    st.markdown("### 📋 Command Reference")
    
    st.code("""
# Create a new policy file
ai-run create

# Start sandbox with policy
sudo ai-run run policy.yaml

# List active sessions
ai-run list

# Cleanup resources
sudo ai-run destroy
    """, language="bash")


def render_help():
    """Render help view"""
    st.markdown('<p class="main-header">ℹ️ Help & Documentation</p>', unsafe_allow_html=True)
    
    st.markdown("""
    ## What is AI Sandbox?
    
    AI Sandbox is a security tool that creates isolated execution environments for AI agents.
    It provides:
    
    - **🔒 File Protection** - Hide sensitive files (SSH keys, AWS credentials, etc.)
    - **🌐 Network Isolation** - Control which domains AI can access
    - **📋 Policy-Based Control** - Configure via simple YAML files
    
    ---
    
    ## Quick Start
    
    1. **Install**: `sudo ./install.sh`
    2. **Create policy**: `ai-run create`
    3. **Edit policy.yaml** to customize
    4. **Start sandbox**: `sudo ai-run run policy.yaml`
    
    ---
    
    ## Policy Configuration
    
    ```yaml
    # Files to hide from sandbox
    protected_files:
      - ~/.ssh
      - ~/.aws
    
    # Allowed network destinations
    network_whitelist:
      - github.com
      - api.openai.com
    
    # Block everything except whitelist
    default_network_policy: DENY
    
    # Set to true to allow all HTTPS
    allow_all_https: false
    ```
    
    ---
    
    ## Network Behavior
    
    | Setting | Behavior |
    |---------|----------|
    | `DENY` + whitelist | Only whitelisted domains work |
    | `DENY` + `allow_all_https: true` | All HTTPS works |
    | `ALLOW` | No network restrictions |
    
    ---
    
    ## Troubleshooting
    
    **Q: Connections hang instead of failing fast**
    
    A: Make sure you're using the latest version with REJECT rules.
    
    **Q: DNS not working**
    
    A: Ensure the veth pair is configured correctly. Try `ai-run destroy` and restart.
    
    **Q: Permission denied**
    
    A: Run with `sudo` for sandbox operations.
    """)


def main():
    """Main application entry point"""
    page = render_sidebar()
    
    if page == "📊 Dashboard":
        render_dashboard()
    elif page == "📝 Policy Editor":
        render_policy_editor()
    elif page == "🚀 Quick Actions":
        render_quick_actions()
    elif page == "ℹ️ Help":
        render_help()


if __name__ == "__main__":
    main()
