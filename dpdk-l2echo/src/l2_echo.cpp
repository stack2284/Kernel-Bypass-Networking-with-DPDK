// dpdk_echo.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>    // memset
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>

/* DPDK headers */
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_mbuf.h>

/* --- Configuration Constants --- */
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32



/* port id */
static const uint16_t PORT_ID = 0;

int main(int argc, char *argv[])
{
    int ret;

    /* 1) Initialize EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "EAL initialization failed (ret=%d)\n", ret);
    }
    argc -= ret;
    argv += ret;

    /* 2) Check port */
    if (!rte_eth_dev_is_valid_port(PORT_ID)) {
        rte_exit(EXIT_FAILURE, "Port %u isn't a valid port\n", (unsigned)PORT_ID);
    }

    /* 3) Create mbuf pool */
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL",
        NUM_MBUFS,
        MBUF_CACHE_SIZE,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id()
    );
    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "Mempool creation failed\n");
    }

    /* 4) Configure port */
    struct rte_eth_conf port_conf;
    memset(&port_conf, 0, sizeof(port_conf));

    ret = rte_eth_dev_configure(PORT_ID, 1, 1, &port_conf);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Port configuration failed (ret=%d)\n", ret);
    }

    /* 5) Setup Rx queue */
    ret = rte_eth_rx_queue_setup(
        PORT_ID, 0, RX_RING_SIZE,
        rte_eth_dev_socket_id(PORT_ID),
        NULL,
        mbuf_pool
    );
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Rx queue setup failed (ret=%d)\n", ret);
    }

    /* 6) Setup Tx queue */
    ret = rte_eth_tx_queue_setup(
        PORT_ID, 0, TX_RING_SIZE,
        rte_eth_dev_socket_id(PORT_ID),
        NULL
    );
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Tx queue setup failed (ret=%d)\n", ret);
    }

    /* 7) Start port */
    ret = rte_eth_dev_start(PORT_ID);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Port start failed (ret=%d)\n", ret);
    }

    /* 8) Get MAC address (use DPDK types) */
    struct rte_ether_addr my_mac;
    rte_eth_macaddr_get(PORT_ID, &my_mac);

    /* Note: space between literal and PRIx8 prevents C++ "invalid suffix" warnings
       (still good practice even when compiling as C). */
    printf("Port %u MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8
           ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 "\n",
           (unsigned)PORT_ID,
           my_mac.addr_bytes[0], my_mac.addr_bytes[1],
           my_mac.addr_bytes[2], my_mac.addr_bytes[3],
           my_mac.addr_bytes[4], my_mac.addr_bytes[5]);

    rte_eth_promiscuous_enable(PORT_ID);
    printf("Promiscuous mode enabled.\n");
    printf("\nCore %u is entering the main processing loop. Press Ctrl+C to quit.\n",
           (unsigned)rte_lcore_id());

    /* 9) Main packet loop */
    for (;;) {
        struct rte_mbuf *bufs[BURST_SIZE];
        const uint16_t num_rx = rte_eth_rx_burst(PORT_ID, 0, bufs, BURST_SIZE);

        if (num_rx == 0)
            continue;

        for (uint16_t i = 0; i < num_rx; ++i) {
            struct rte_mbuf *buf = bufs[i];

            /* Use the proper DPDK ethernet header type */
            struct rte_ether_hdr *eth_hdr =
                rte_pktmbuf_mtod(buf, struct rte_ether_hdr *);

            struct rte_ether_addr temp_mac;

            /* swap / replace MACs as you intended:
               save src -> temp, write my_mac into src, copy temp -> dst */
            rte_ether_addr_copy(&eth_hdr->src_addr, &temp_mac);
            rte_ether_addr_copy(&my_mac, &eth_hdr->src_addr);
            rte_ether_addr_copy(&temp_mac, &eth_hdr->src_addr);
        }

        const uint16_t num_tx = rte_eth_tx_burst(PORT_ID, 0, bufs, num_rx);

        if (num_tx < num_rx) {
            for (uint16_t i = num_tx; i < num_rx; ++i) {
                rte_pktmbuf_free(bufs[i]);
            }
        }
    }

    /* unreachable in this example */
    rte_eth_dev_stop(PORT_ID);
    rte_eth_dev_close(PORT_ID);
    return 0;
}

