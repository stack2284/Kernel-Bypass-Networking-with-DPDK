#!/bin/bash
# This script configures the Linux environment for DPDK.
# It must be run as root (or with sudo).

NUM_HUGEPAGES=1024
NODE_PATH="/sys/devices/system/node/node0"

echo "--- Unmounting old hugepages mount (if any) ---"
umount /mnt/huge || true

echo "--- Creating /mnt/huge mount point ---"
mkdir -p /mnt/huge

echo "--- Mounting hugetlbfs ---"
mount -t hugetlbfs nodev /mnt/huge

echo "--- Allocating $NUM_HUGEPAGES pages ---"
echo $NUM_HUGEPAGES > $NODE_PATH/hugepages/hugepages-2048kB/nr_hugepages

echo "--- Hugepage Allocation Complete ---"
cat /proc/meminfo | grep -i HugePages
