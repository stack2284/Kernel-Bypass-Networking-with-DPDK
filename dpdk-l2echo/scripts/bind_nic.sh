#!/bin/bash
# This script finds the first kernel-bound NIC and binds it to DPDK.
# Must be run as root (or with sudo).

# Path to your dpdk-devbind.py script
# This path is correct for the dpdk-24.03 source directory
DPDK_TOOLS_PATH="/home/sahil/dpdk-24.03/usertools"
DEV_BIND_SCRIPT="$DPDK_TOOLS_PATH/dpdk-devbind.py"

# Find the *first* NIC that is using a kernel driver
echo "--- Finding first available kernel NIC ---"
# We look for virtio-pci, the default for most VMs
NIC_PCI_ADDR=$($DEV_BIND_SCRIPT --status | grep 'drv=virtio-pci' | head -n 1 | awk '{print $1}')
NIC_IF_NAME=$($DEV_BIND_SCRIPT --status | grep 'drv=virtio-pci' | head -n 1 | grep -o 'if=[^ ]*' | cut -d'=' -f2)

if [ -z "$NIC_PCI_ADDR" ] || [ -z "$NIC_IF_NAME" ]; then
    echo "--- No kernel-bound NIC found. Checking DPDK drivers... ---"
    $DEV_BIND_SCRIPT --status
    exit 0
fi

echo "Found NIC: $NIC_IF_NAME at PCI: $NIC_PCI_ADDR"

# Load the uio_pci_generic module (we installed this)
echo "--- Loading uio_pci_generic module ---"
modprobe uio_pci_generic

# Take the interface down
echo "--- Taking interface $NIC_IF_NAME down ---"
ip link set dev $NIC_IF_NAME down

# Bind the NIC to DPDK
echo "--- Binding $NIC_PCI_ADDR to uio_pci_generic ---"
$DEV_BIND_SCRIPT --bind=uio_pci_generic $NIC_PCI_ADDR

# Show final status
echo "--- Bind Complete. Final Status: ---"
$DEV_BIND_SCRIPT --status
