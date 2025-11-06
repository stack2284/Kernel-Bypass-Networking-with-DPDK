# Low-Latency Kernel-Bypass Networking with DPDK

This project is a deep dive into high-performance, low-latency networking that implements the core principles of HFT (High-Frequency Trading) infrastructure. It demonstrates how to bypass the operating system kernel to process raw Ethernet packets at nanosecond speeds.

The repository contains two applications:

1. **`l2_echo`** — a simple, single-core "Hello World" application that proves the kernel-bypass concept.
2. **`l2_multi_echo`** — a high-performance, multi-core, "share-nothing" server that scales linearly by using hardware RSS (Receive Side Scaling) and dedicated queues per core.

> The *real* project is not only the ~200 lines of C++ code — it’s the complex environment, hardware bindings, and systems-level engineering required to make it run.

---

## 🎯 Core Concepts Demonstrated

This project shows a deep understanding of the full networking stack, from the C++ application down to hardware registers:

- **Kernel-Bypass Networking**: Avoid the OS kernel for packet I/O.
- **DPDK (Data Plane Development Kit)**: Industry-standard C library for user-space networking.
- **User-space drivers (UIO/VFIO)**: Bind the NIC away from the kernel (e.g. `virtio-pci`) and control it from user-space via `uio_pci_generic` or VFIO.
- **Polling vs. Interrupts**: Replace unpredictable IRQs with a 100% CPU busy-poll loop for predictable, low latency.
- **Zero-Copy & DMA**: Use `rte_mempool`/`rte_mbuf` so the NIC writes packets directly to user-space buffers via DMA.
- **Multi-core "Share-Nothing" Architecture**: Give each core dedicated, lock-free Rx/Tx queues for near-linear scaling.
- **Hardware RSS**: Configure the NIC to hash and distribute incoming traffic across multiple Rx queues.
- **Systems engineering & virtualization**: Troubleshoot virtualization limitations (e.g., virtio multiqueue support) and kernel module dependencies.

---

## 1. Why the Kernel is the Bottleneck

A typical networking call like `recv()` goes through the kernel — which introduces latency and jitter due to:

1. **System call overhead** (`syscall`) — slow.
2. **Context switches** — save/restore state and mode switches.
3. **Data copies** — kernel memory → user memory (double-copy).
4. **Interrupts** — unpredictable latency (jitter).

**DPDK fixes these** by:

- Removing system calls (NIC is bound to user-space driver).
- Avoiding context switches (user-space busy-polling loop).
- Eliminating copies (DMA into `rte_mbuf`s).
- Avoiding interrupts (busy polling).

This yields the lowest, most predictable latency possible for packet processing.

---

## 2. Environment (the 90% of the work)

This project requires careful environment setup. I used a **MacBook Pro (M4)** running an **Ubuntu 24.04 (ARM64)** VM in **UTM**. The VM configuration and virtualization backend are critical for DPDK to talk to a NIC correctly.

### VM setup (UTM)
1. **Host:** macOS (Apple Silicon M4)
2. **VM software:** UTM
3. **VM type:** **Virtualize** (Apple Virtualization Framework) or **Emulate** (QEMU) depending on multiqueue requirements
4. **Guest OS:** Ubuntu Server 24.04 (ARM64)
5. **Network:** Use **Bridged (Advanced)** mode so the VM gets its own IP and the virtual NIC is exposed as a PCI device.

> **Important:** The choice of Virtualize vs Emulate affects virtio-net features. Apple's Virtualize backend often exposes only a single Rx/Tx queue; to run `l2_multi_echo` with multiple queues you may need a QEMU-based VM configured with multiqueue support.

### Install DPDK & dependencies (inside the VM)

```bash
# 1. Update and install build tools
sudo apt-get update
sudo apt-get install -y build-essential meson ninja-build python3-pyelftools libnuma-dev pkg-config

# 2. Install critical kernel extras (required for some UIO drivers)
sudo apt-get install -y linux-modules-extra-$(uname -r)

# 3. Download and build DPDK from source
cd ~
wget https://fast.dpdk.org/rel/dpdk-24.03.tar.xz
tar -xf dpdk-24.03.tar.xz
cd dpdk-24.03
meson build -Dplatform=generic
cd build
ninja
sudo ninja install
sudo ldconfig
```

---

## 3. The Multiqueue Virtualization Challenge

When attempting to run `l2_multi_echo` with multiple Rx/Tx queues the application failed with:

```
ETHDEV: Ethdev port_id=0 nb_rx_queues=4 > 1
EAL: Error - exiting with code: 1
Cause: Port configuration failed: Unknown error -22 (Invalid Argument)
```

**Root cause:** the VM’s virtual NIC only exposed a single Rx/Tx queue (common with UTM's `Virtualize` backend using virtio-net). Requesting 4 queues is an invalid argument for that device.

**Solution:** use a VM configured with a multiqueue-capable virtual NIC (e.g., QEMU `-netdev`/`-device virtio-net-pci,mq=on` or UTM "Emulate" with multiqueue enabled), or run the single-core `l2_echo` on the current VM.

**Conclusion:** `l2_echo` (single-core) is the correct target for the default UTM/Virtualize setup. `l2_multi_echo` is included as a demonstration of a scalable architecture and will run correctly only on hardware or a VM that exposes multiple Rx/Tx queues.

---

## Project structure

```
├── Makefile                # builds l2_echo and l2_multi_echo via pkg-config
├── src/
│   ├── l2_echo.cpp         # single-core example: one port, one queue, one mempool
│   └── l2_multi_echo.cpp   # multi-core, RSS-enabled, one Rx/Tx queue per core
├── scripts/
│   ├── setup_env.sh        # allocate hugepages (e.g., 1024 x 2MB)
│   └── bind_nic.sh         # find NIC PCI address, load uio_pci_generic, bind NIC to user-space driver
└── README.md               # this file
```

**Notes on important scripts:**
- `scripts/setup_env.sh` — configures hugepages required by DPDK (`/dev/hugepages`) and may set `ulimit`/sysctl values.
- `scripts/bind_nic.sh` — finds the kernel interface (e.g. `enp0s1`) and its PCI address (e.g. `0000:00:01.0`), brings the interface down, loads `uio_pci_generic`, and binds the NIC to the user-space driver (this will drop existing network connectivity including SSH).

---

## How to run (from the VM console)

> **Run these commands from the VM console** (not via SSH), because `bind_nic.sh` will unbind the NIC and disconnect SSH sessions.

```bash
# 1. Change to the project directory
cd ~/dpdk-l2echo

# 2. Build the apps
make

# --- One-time setup per VM reboot ---
# 3. Setup hugepages
sudo ./scripts/setup_env.sh

# 4. Bind NIC to DPDK (this will disconnect SSH if you are using it)
sudo ./scripts/bind_nic.sh

# --- Run the application ---
# 5. Run single-core server
sudo ./build/l2_echo -l 1

# OR (multiqueue VM required):
# sudo ./build/l2_multi_echo -l 0-3
```

---

## How to test (the proof of success)

The proof that the kernel is bypassed is that the kernel’s networking stack will not respond to pings while `l2_echo` owns the NIC. With `l2_echo` running in the VM console:

1. From your Mac (host), find the VM IP (e.g., `192.170.1.194`).
2. Ping the VM:

```bash
ping 192.170.1.194
```

**Expected result:** 100% packet loss — e.g.:

```
PING 192.170.1.194 (192.170.1.194): 56 data bytes
Request timeout for icmp_seq 0
Request timeout for icmp_seq 1
...
100% packet loss
```

This demonstrates the kernel never sees the packets — your user-space DPDK app intercepts them at Layer 2, flips MACs, and sends replies directly.

---

## Troubleshooting & tips

- Use the VM console for `bind_nic.sh` and related steps to avoid losing access.
- If you need multiple Rx/Tx queues, ensure your virtual NIC and hypervisor expose multiqueue support (QEMU/Emulate + `mq=on` or real NIC).
- Make sure `linux-modules-extra-$(uname -r)` is installed when using `uio_pci_generic`.
- Check `dmesg` and DPDK's EAL error messages for port configuration failures.

---

## Summary

This repository demonstrates the architecture, code, and environment required to achieve kernel-bypass networking using DPDK. The included `l2_echo` and `l2_multi_echo` examples show how to build single-core and multi-core, low-latency packet processing applications. The real engineering work is in the VM configuration, driver binding, hugepages, and ensuring the virtual/hardware NIC supports the features you need.

If you want, I can also:
- produce a printer-friendly `README.pdf` or `README.md` update in-place,
- add a troubleshooting checklist, or
- create a condensed quick-start `QUICKSTART.md`.

---

*Last updated:* November 7, 2025

