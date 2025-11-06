
/*
 * l2_multi_echo.cpp
 *
 * This is a high-performance, multi-core DPDK "Layer 2 Echo" server.
 * It's designed using a "run-to-completion" and "share-nothing" model.
 *
 * 1. RSS: The NIC's hardware RSS is used to distribute incoming
 * packets across multiple Rx queues.
 * 2. Multi-Core: The app is launched on multiple cores (e.g., -l 0-3).
 * 3. Share-Nothing: Each lcore is given its own dedicated Rx queue,
 * Tx queue, and mempool. It polls, processes, and transmits
 * packets without ever needing to coordinate or lock with another core.
 */


#include<stdio.h>
#include<stdlib.h>
#include<string.h> 
#include<stdint.h> 
#include<inttypes.h> 
#include<unistd.h> 
#include<errno.h>

#include<rte_eal.h>
#include<rte_log.h>
#include<rte_ethdev.h>
#include<rte_mbuf.h>
#include<rte_lcore.h>
#include<rte_ether.h>
#include<rte_mempool.h>

#define RX_RING_SIZE 1024 
#define TX_RING_SIZE 1024 
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32 

const uint16_t PORT_ID = 0 ; 

// This is the data plane "hot loop" function
// It will be run on EVERY lcore that DPDK is launched on.

static int lcore_main_loop( __attribute__((unused)) void *arg){
	
	const uint16_t lcoreid = rte_lcore_id(); 
	const uint16_t queue_id = (uint16_t)lcoreid ; 
	// each core gets unique id as it l_core_id 

	struct rte_ether_addr my_device; 

	rte_eth_macaddr_get(PORT_ID , &my_device); 

	printf("Core %u entering main loop on queue %u.......\n" , lcoreid , queue_id);

	struct rte_mbuf * bufs[BURST_SIZE]; 

	while(1){
		

		// -------- RECIVING PACKETS -------------

		const uint16_t num_rx = rte_eth_rx_burst(PORT_ID, queue_id, bufs, BURST_SIZE);

		if (num_rx == 0) {
            		continue;
        	}

		for (uint16_t i = 0; i < num_rx; i++) {
            		struct rte_mbuf *buf = bufs[i];
            		struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(buf, struct rte_ether_hdr *);
            		struct rte_ether_addr temp_device;

			rte_ether_addr_copy(&eth_hdr->src_addr , &temp_device) ; 
			rte_ether_addr_copy(&my_device , &eth_hdr->src_addr); 
			rte_ether_addr_copy(&temp_device , &eth_hdr->dst_addr); 
		}
		
		const uint16_t num_tx = rte_eth_tx_burst(PORT_ID , queue_id , bufs , num_rx ); 
		if(num_tx < num_rx) {
			for(uint16_t i =num_tx ; i < num_rx ; i++){
				if (bufs[i])  rte_pktmbuf_free(bufs[i]);
			}
		}
	}
	return 0 ; 
}


// ========================================================================
// --- THIS IS THE main() FUNCTION (THE "CONTROL PLANE") --- :3 
// ========================================================================


int main(int argc ,char *argv[]){
	
	int ret ; 
	uint16_t nb_lcores , nb_rx_queues , nb_tx_queues; 

	ret = rte_eal_init(argc , argv); 

	if(ret <0 ){
		rte_exit(EXIT_FAILURE, "EAL initialization failed: %s\n", rte_strerror(errno));
	}
	argc -= ret; 
	argv += ret;  

	nb_lcores = rte_lcore_count(); 
	nb_rx_queues = nb_lcores; 
	nb_tx_queues = nb_lcores;

	printf("Running on %u lcores (1 main + %u workers ). \n", nb_lcores , nb_lcores - 1); 
	
	if (!rte_eth_dev_is_valid_port(PORT_ID))
        rte_exit(EXIT_FAILURE, "Port %u is not a valid port\n", PORT_ID);

	// ----------- CRATING MEMPOOOL -------------- 
	
	printf("creating mbuf pool.... \n"); 

	struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(
			"MBUF_POOL", 
			NUM_MBUFS * nb_lcores , 
			MBUF_CACHE_SIZE,
			0,
			RTE_MBUF_DEFAULT_BUF_SIZE, 
			rte_socket_id()
			);
	if(mbuf_pool == NULL) rte_exit(EXIT_FAILURE, "Mempool creation failed: %s\n", rte_strerror(rte_errno));

	// ------------ CONFIGURE the Ethernet Port -----------
	
	struct rte_eth_conf port_conf ; 

	memset(&port_conf , 0 , sizeof(struct rte_eth_conf)); 

	// Enabling receiver side scaling / rss 
	
	port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS ; 

	port_conf.rx_adv_conf.rss_conf.rss_hf = RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP | RTE_ETH_RSS_UDP;

	printf("Configuring port %u with %u Rx and %u Tx queues...\n", PORT_ID, nb_rx_queues, nb_tx_queues);

	ret = rte_eth_dev_configure(
        PORT_ID,
        nb_rx_queues, // Total number of Rx queues
        nb_tx_queues, // Total number of Tx queues
        &port_conf
    	);

	if (ret < 0) rte_exit(EXIT_FAILURE, "Port configuration failed: %s\n", rte_strerror(ret));
	
	uint16_t queue_id; 
	for(queue_id = 0; queue_id < nb_lcores ; queue_id++){
		
		printf("setting up RXqueu %u... \n" , queue_id); 

		ret = rte_eth_rx_queue_setup(
				PORT_ID, 
				queue_id , 
				RX_RING_SIZE, 
				rte_eth_dev_socket_id(PORT_ID), 
				NULL , 
				mbuf_pool
				);
		if(ret < 0) rte_exit(EXIT_FAILURE , "RX queue %u setup failed. %s \n" , queue_id , rte_strerror(errno));

		printf("Setting up Tx queue %u...\n", queue_id);

		ret = rte_eth_tx_queue_setup(
    		PORT_ID,
    		queue_id,
    		TX_RING_SIZE,
    		rte_eth_dev_socket_id(PORT_ID),
    		NULL
		);
		if (ret < 0) {
    			rte_exit(EXIT_FAILURE, "Tx queue %u setup failed: %s\n", queue_id, rte_strerror(ret));
		}		

	}

	// ----------- Starting ethernet portal ------------

	printf("Starting port %u...\n", PORT_ID);
	ret = rte_eth_dev_start(PORT_ID);
	if (ret < 0)
        rte_exit(EXIT_FAILURE, "Port start failed: %s\n", rte_strerror(ret));
	rte_eth_promiscuous_enable(PORT_ID);

	printf("Promiscuous mode enabled.\n");

	// ========================================================================
    	// --- 7. Launch Data Plane on all cores ---
    	// ========================================================================
	


    	printf("\nLaunching main loop on all %u cores...\n", nb_lcores);
	// Launch the 'lcore_main_loop' function on all WORKER cores
    	rte_eal_mp_remote_launch(lcore_main_loop, NULL, CALL_MAIN);
	lcore_main_loop(NULL);
	// Run the same loop on the MAIN core
	


	rte_eth_dev_stop(PORT_ID); 
	rte_eth_dev_close(PORT_ID); 
	return 0 ; 

}






