# 🚀 Low-Latency Kernel-Bypass Networking with DPDK

This project is a deep dive into **high-performance, low-latency networking**, the core principles of **Low Latency Netowrking infrastructure**. infrastructure.  
It demonstrates how to bypass the operating system kernel to process **raw Ethernet packets at nanosecond speeds** using **DPDK (Data Plane Development Kit)**.

---

## 🧩 Applications Included

### 1. `l2_echo`
A minimal **single-core** DPDK “Hello World” app that:
- Receives raw Ethernet frames.
- Swaps source and destination MAC addresses.
- Sends them back (L2 echo).

This verifies that your DPDK environment, NIC binding, and mempool setup are correct.

---

### 2. `l2_multi_echo`
A **multi-core**, high-performance “share-nothing” DPDK echo server designed for scalability.  
Each core runs its own isolated pipeline: receive → process → transmit.

#### Key features:
- **Run-to-completion** model per lcore (no shared state).
- **Receive Side Scaling (RSS)** for load balancing.
- **Dedicated Rx/Tx queues per core**.
- **Promiscuous mode** for packet visibility.
- Clean shutdown and per-core stats reporting.

---

## 🧠 Concepts Demonstrated

- Kernel-bypass user-space networking with DPDK.
- Multi-core scaling with `rte_eal_mp_remote_launch()`.
- RSS-based packet distribution.
- Memory management with `rte_mempool`.
- Ethernet frame manipulation using `rte_ether_hdr`.

---

## 🧰 Project Structure

```
dpdk-l2echo/
│
├── src/
│   ├── l2_echo.cpp
│   └── l2_multi_echo.cpp
│
├── Makefile
└── README.md
```

---

## ⚙️ Build Instructions

### Prerequisites
- DPDK >= 23.11 installed under `/usr/local`.
- GCC or Clang with C++17.
- Hugepages configured.
- NIC bound to DPDK driver (`vfio-pci` or `igb_uio`).

### Build
```bash
make
```

### Run (Single-Core)
```bash
sudo ./build/l2_echo -l 0 -n 4
```

### Run (Multi-Core)
```bash
sudo ./build/l2_multi_echo -l 0-3 -n 4
```

---

## 🍎 Running in UTM on macOS (Apple Silicon / M1–M4)

> ⚠️ Note: UTM virtualization on macOS **only supports a single Rx/Tx queue** (no multiqueue or RSS).  
> Your code will **run correctly**, but not scale across cores.

### Steps:
1. Download and install **UTM**.
2. Create a new ARM virtual machine and import:
   ```
   ubuntu-24.04-live-server-arm64.iso
   ```
3. During setup, assign:
   - 4 CPU cores  
   - 4 GB RAM  
   - Networking: Virtio (default)
4. After boot:
   ```bash
   sudo apt update && sudo apt install -y build-essential meson ninja-build libnuma-dev
   ```
5. Download and build DPDK:
   ```bash
   wget https://fast.dpdk.org/rel/dpdk-24.03.tar.xz
   tar xf dpdk-24.03.tar.xz
   cd dpdk-24.03
   meson setup build
   ninja -C build
   sudo ninja -C build install
   sudo ldconfig
   ```
6. Clone and build this project:
   ```bash
   git clone https://github.com/yourname/dpdk-l2echo.git
   cd dpdk-l2echo
   make
   ```
7. Run:
   ```bash
   sudo ./build/l2_multi_echo -l 0 -n 4
   ```

You’ll see output like:
```
Running on 4 lcores (1 main + 3 workers)
Device supports max_rx_queues=1, limiting nb_rx_queues to that
Core 1 entering main loop...
```

Even though only 1 queue is used, all cores execute their main loops for testing.

---

## 🧪 Verifying Packet Echo
You can test with `tcpdump` or `pktgen-dpdk`:
```bash
sudo tcpdump -i <iface> ether proto 0x0800
```

---

## 💡 Real Parallelism (Recommended Setup)

To achieve true multi-core parallelism and RSS distribution:
- Run this on a **Linux host** with a **multi-queue NIC** (e.g., Intel X520, Mellanox ConnectX).
- Bind the NIC to DPDK:
  ```bash
  sudo dpdk-devbind.py --bind=vfio-pci 0000:01:00.0
  ```
- Then:
  ```bash
  sudo ./build/l2_multi_echo -l 0-3 -n 4
  ```

---

## 🧾 License
MIT License © 2025 Sahil Shaikh

---

## 🧠 Summary
| Environment | Works | Parallelism | Notes |
|--------------|--------|--------------|-------|
| macOS (UTM, M1–M4) | ✅ | ❌ | Single queue only |
| Linux VM (QEMU+KVM) | ✅ | ✅ | Use `mq=on,vhost=on` |
| Bare Metal NIC (Intel/Mellanox) | ✅ | ✅✅✅ | Full RSS parallelism |
