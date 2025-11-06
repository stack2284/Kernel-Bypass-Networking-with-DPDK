#!/bin/bash
#
# This is a one-shot script to set up the entire DPDK environment
# on a fresh Ubuntu Server 24.04 VM (or similar Debian-based system).
#
# 1. Installs all dependencies (build tools, kernel modules, DPDK deps).
# 2. Downloads, compiles, and installs the DPDK library.
#
# USAGE:
# 1. After installing the VM, clone your project:
#    git clone ...
#    cd your-project-repo
# 2. Make this script executable: chmod +x setup_vm.sh
# 3. Run it with sudo:         sudo ./setup.sh

set -e # Exit immediately if any command fails

# --- 0. Sudo Check ---
if [ "$(id -u)" -ne 0 ]; then
  echo "This script must be run with sudo." >&2
  echo "Please run: sudo $0"
  exit 1
fi

# --- Configuration ---
DPDK_VERSION="dpdk-24.03"

# Find the real user's home directory, even when run with sudo
if [ -n "$SUDO_USER" ]; then
    USER_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
else
    # Fallback if not run with sudo (though we checked)
    USER_HOME=$(eval echo ~$USER)
fi

DPDK_SRC_DIR="$USER_HOME/$DPDK_VERSION"

echo "--- 🐧 Setting up Ubuntu VM for DPDK ---"
echo "DPDK source will be in: $DPDK_SRC_DIR"
echo "This will take several minutes..."

# --- 1. Install Dependencies ---
echo ""
echo "Updating apt and installing dependencies..."
apt-get update
apt-get install -y build-essential meson ninja-build python3-pyelftools \
                   libnuma-dev pkg-config linux-modules-extra-$(uname -r) wget

# --- 2. Download DPDK (as the user) ---
echo ""
echo "Downloading DPDK to $USER_HOME..."
cd $USER_HOME
if [ ! -f "$DPDK_VERSION.tar.xz" ]; then
    # Run wget as the original user, not root, to avoid permission issues
    sudo -u $SUDO_USER wget https://fast.dpdk.org/rel/$DPDK_VERSION.tar.xz
else
    echo "DPDK tarball already downloaded."
fi

# --- 3. Extract DPDK (as the user) ---
if [ -d "$DPDK_SRC_DIR" ]; then
    echo "DPDK directory already exists, skipping extraction."
else
    echo "Extracting DPDK..."
    sudo -u $SUDO_USER tar -xf $DPDK_VERSION.tar.xz
fi

# --- 4. Build and Install DPDK ---
echo ""
echo "Building DPDK..."
cd $DPDK_SRC_DIR
# Run meson/ninja as the user
sudo -u $SUDO_USER meson build -Dplatform=generic
cd build
sudo -u $SUDO_USER ninja

echo "Installing DPDK (this requires sudo)..."
# Install (as root)
ninja install
# Update the system's library cache
ldconfig

echo ""
echo "--------------------------------------------------------"
echo "✅ DPDK Environment is Set Up!"
echo ""
echo "--- ⬇️ How to Run Your Project (after every reboot) ---"
echo "1. Log in to this VM (at the console, not SSH)."
echo "2. cd ~/dpdk-l2echo  (or your project directory)"
echo "3. sudo ./scripts/setup_env.sh"
echo "4. sudo ./scripts/bind_nic.sh"
echo "5. make"
echo "6. sudo ./build/l2_echo -l 1"
echo "   (or sudo ./build/l2_multi_echo -l 0-3)"
echo "--------------------------------------------------------"