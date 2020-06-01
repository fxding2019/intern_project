/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <sqlite3.h>
#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <time.h>

#include "datainterface.h"
#define clear() printf("\033[H\033[J")

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

//index for basic stat :)
#define TCP 0
#define UDP 1
#define ICMP4 2
#define ICMP6 3
#define IPv4 4
#define IPv6 5
sqlite3 *db;
static const char usage[] =
	"%s EAL_ARGS -- [-t]\n";

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
	},
};

static struct {
	uint64_t total_cycles;
	uint64_t total_queue_cycles;
	uint64_t total_pkts;
} latency_numbers;
struct ipv4_data
{
	char *ip_addr;
	int rep;
};

int hw_timestamping;
void initHandler(int);

unsigned long int basic_stat[4];//for counting number of protocol in l4
#define TICKS_PER_CYCLE_SHIFT 16
static uint64_t ticks_per_cycle_mult;
void
initHandler(int sig){
	char c;
	signal(sig, SIG_IGN);
	printf("Are you sure to quit? [y/N] ");
	c = getchar();
	if (c == 'y' || c == 'Y')
	{
		clear();
		printf("There are %d IPv4 packets and %d IPv6 packets......\n",basic_stat[IPv4],basic_stat[IPv6]);
		printf("These are the number of packet type(layer 4 protocol) which has recorded....\n");
		printf("\t- TCP: %d\n",basic_stat[TCP]);
		printf("\t- UDP: %d\n",basic_stat[UDP]);
		printf("\t- ICMPv4: %d\n",basic_stat[ICMP4]);
		printf("\t- ICMPv6: %d\n",basic_stat[ICMP6]);
		printf("List of most use ip address...\n");
		conclude_stat(db);
		printf("Bye.....\n");
		sqlite3_close(db);
		exit(0);
	}
	else
		signal(SIGINT,initHandler);
	
}
static inline void
decode_ipv6(const uint8_t ip_addr_src[],const uint8_t ip_addr_dst[],char p,sqlite3 *db)
{
	char ipv6_addr_src[40];
	char ipv6_addr_dst[40];
	char tmp_src[4];
	char tmp_dst[4];
	ipv6_addr_dst[0] = 0;
	ipv6_addr_src[0] = 0;
	for (int i = 0; i < 16; i++)
	{
		uint16_t tmp = ip_addr_src[i];
		uint16_t tmp2 = ip_addr_dst[i];
		sprintf(tmp_src,"%02x",tmp);
		sprintf(tmp_dst,"%02x",tmp2);
		strcat(ipv6_addr_src,tmp_src);
		strcat(ipv6_addr_dst,tmp_dst);
		if(i%2 == 1 && i < 15){
			strcat(ipv6_addr_src,":");
			strcat(ipv6_addr_dst,":");
		}
	}
	if(data_choice(db,ipv6_addr_src)){
		update_data(db,ipv6_addr_src);
	}
	else{
		insert_data(db,ipv6_addr_src);
	}
	if(p == 'y' || p == 'Y'){
		printf("%s ----> %s \n",ipv6_addr_src,ipv6_addr_dst);
	}
}
//for printing ipv4 addr.....
static inline void
decode_ip(const uint32_t ip_addr_src,const uint32_t ip_addr_dst,char p,sqlite3 *db){
	char ipv4_addr_src[16];//perpare for sending to sql
	char ipv4_addr_dst[16];
	sprintf(ipv4_addr_src,"%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8,
			(uint8_t)(ip_addr_src & 0xff),
			(uint8_t)((ip_addr_src >> 8)&0xff),
			(uint8_t)((ip_addr_src >> 16)&0xff),
			(uint8_t)((ip_addr_src >> 24) & 0xff)
	);
	sprintf(ipv4_addr_dst,"%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8,
			(uint8_t)(ip_addr_dst & 0xff),
			(uint8_t)((ip_addr_dst >> 8)&0xff),
			(uint8_t)((ip_addr_dst >> 16)&0xff),
			(uint8_t)((ip_addr_dst >> 24) & 0xff)
	);
	//printf("choice ---->  %d\n",data_choice(db,ipv4_addr_src));
	if(data_choice(db,ipv4_addr_src)){
		update_data(db,ipv4_addr_src);
	}
	else{
		insert_data(db,ipv4_addr_src);
	}
	if(p == 'Y' || p == 'y'){
		printf("%s ----> %s\n",ipv4_addr_src,ipv4_addr_dst);
	}
}
void
print_decode_packet(struct rte_mbuf *m,char p,sqlite3 *db)
{
	uint16_t eth_type;
	int l2_len;
	int l3_len;
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	//may be it can solve "Segmentation fault?"
	struct rte_tcp_hdr *tcp_hdr_v4;
	struct rte_tcp_hdr *tcp_hdr_v6;

	struct rte_udp_hdr *udp_hdr;
	struct rte_udp_hdr *udp_hdr_v6;

	eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);
	l2_len = sizeof(struct rte_ether_hdr);
	//make sure that it is a right packet type to decode
	switch (eth_type)
	{
	case RTE_ETHER_TYPE_IPV4:
		basic_stat[IPv4]++;
		l3_len = sizeof(struct rte_ipv4_hdr);
		ipv4_hdr = (struct rte_ipv4_hdr *)((char *)eth_hdr + l2_len);
		decode_ip(ipv4_hdr->src_addr,ipv4_hdr->dst_addr,p,db);
		if(p == 'y' || p == 'Y'){
			printf("\t--> ");
		}
		if(ipv4_hdr->next_proto_id == 0x01){
			if(p == 'y' || p == 'Y'){
				printf("protocol(next layer): ICMP\n");
			}
			basic_stat[ICMP4]++;
		}
		else if(ipv4_hdr->next_proto_id == 0x02){
			if(p == 'y' || p == 'Y'){
				printf("protocol(next layer): IGMP\n");
			}
		}
		else if(ipv4_hdr->next_proto_id == 0x11){
			basic_stat[UDP]++;
			udp_hdr = (struct rte_udp_hdr *)((char*)ipv4_hdr + l3_len);
			if(p == 'y' || p == 'Y'){
				printf("protocol(next layer): UDP\n");
				printf(" \t\t%ld ---> %ld :port travel\n",udp_hdr->src_port,udp_hdr->dst_port);
			}
		}
		else if(ipv4_hdr->next_proto_id == 0x06){
			basic_stat[TCP]++;
			tcp_hdr_v4 = (struct rte_tcp_hdr *)((char *)ipv4_hdr + l3_len);
			if(p == 'y' || p == 'Y'){
				printf("protocol(next layer): TCP\n");
				printf(" \t\t%ld ---> %ld :port travel\n",tcp_hdr_v4->src_port,tcp_hdr_v4->dst_port);
			}
		}
		else{
			if(p == 'y' || p == 'Y'){
				printf("protocol(next layer): %d (Will add into data base later.....)\n",ipv4_hdr->next_proto_id);
			}
		}
		break;
	case RTE_ETHER_TYPE_IPV6:
		basic_stat[IPv6]++;
		l3_len = sizeof(struct rte_ipv6_hdr);
		ipv6_hdr = (struct rte_ipv6_hdr *)((char *)eth_hdr + l2_len);
		decode_ipv6(ipv6_hdr->src_addr,ipv6_hdr->dst_addr,p,db);
		if(p == 'y' || p == 'Y'){
			printf("\t--> next protocol: ");
		}
		switch (ipv6_hdr->proto)
		{
		case 0x06:
			basic_stat[TCP]++;
			tcp_hdr_v6 = (struct rte_tcp_hdr *)((char *)ipv6_hdr + l3_len);
			if(p == 'y' || p == 'Y'){
				printf("TCP\n");
				printf("\t\t%ld ---> %ld :port travel\n",tcp_hdr_v6->src_port,tcp_hdr_v6->dst_port);
			}
			break;
		case 0x11:
			basic_stat[UDP]++;
			udp_hdr_v6 = (struct rte_udp_hdr *)((char*)ipv6_hdr + l3_len);
			if(p == 'y' || p == 'Y'){
			printf("UDP\n");
			printf("\t\t%ld ---> %ld :port travel\n",udp_hdr_v6->src_port,udp_hdr_v6->dst_port);
			}
			break;
		case 0x3A:
			basic_stat[ICMP6]++;
			if(p == 'y' || p == 'Y'){
			printf("ICMP for ipV6\n");
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}
/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rxconf rxconf;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0) {
		printf("Error during getting device (port %u) info: %s\n",
				port, strerror(-retval));

		return retval;
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	if (hw_timestamping) {
		if (!(dev_info.rx_offload_capa & DEV_RX_OFFLOAD_TIMESTAMP)) {
			printf("\nERROR: Port %u does not support hardware timestamping\n"
					, port);
			return -1;
		}
		port_conf.rxmode.offloads |= DEV_RX_OFFLOAD_TIMESTAMP;
	}

	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	rxconf = dev_info.default_rxconf;

	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
			rte_eth_dev_socket_id(port), &rxconf, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	retval  = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	if (hw_timestamping && ticks_per_cycle_mult  == 0) {
		uint64_t cycles_base = rte_rdtsc();
		uint64_t ticks_base;
		retval = rte_eth_read_clock(port, &ticks_base);
		if (retval != 0)
			return retval;
		rte_delay_ms(100);
		uint64_t cycles = rte_rdtsc();
		uint64_t ticks;
		rte_eth_read_clock(port, &ticks);
		uint64_t c_freq = cycles - cycles_base;
		uint64_t t_freq = ticks - ticks_base;
		double freq_mult = (double)c_freq / t_freq;
		printf("TSC Freq ~= %" PRIu64
				"\nHW Freq ~= %" PRIu64
				"\nRatio : %f\n",
				c_freq * 10, t_freq * 10, freq_mult);
		/* TSC will be faster than internal ticks so freq_mult is > 0
		 * We convert the multiplication to an integer shift & mult
		 */
		ticks_per_cycle_mult = (1 << TICKS_PER_CYCLE_SHIFT) / freq_mult;
	}

	struct rte_ether_addr addr;

	retval = rte_eth_macaddr_get(port, &addr);
	if (retval < 0) {
		printf("Failed to get MAC address on port %u: %s\n",
			port, rte_strerror(-retval));
		return retval;
	}
	printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;
	return 0;
}

/*
 * Main thread that does the work, reading from INPUT_PORT
 * and writing to OUTPUT_PORT
 */
static  __attribute__((noreturn)) void
lcore_main(void)
{
	int stat_db;
	uint16_t port;
	char is_debug;
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv4_hdr *ipv4_hdr;
	int32_t i;
	clock_t t;
	RTE_ETH_FOREACH_DEV(port)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
						(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());
	for (int i = 0; i < 6; i++)
	{
		basic_stat[i] = 0;//init value for stat
	}
	stat_db = sqlite3_open("ip_stat.db",&db);
	if(stat_db){
		printf("ERROR OCCUR DURING OPEN DATABASE......\n");
		exit(0);
	}
	create_tbl(db);
	printf("Do you want to print realtime packet detail?[y/N]: ");
	is_debug = getchar();
	for (;;) {
		//Maybe I have to work around here.
		
		RTE_ETH_FOREACH_DEV(port) {
			struct rte_mbuf *bufs[BURST_SIZE];
			uint32_t size = 0;
			const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
					bufs, BURST_SIZE);
			t = clock();
			for(i=0;i<nb_rx;i++){
				print_decode_packet(bufs[i],is_debug,db);
				//printf("packet len is %d bytes\n",bufs[i]->pkt_len);
				size += bufs[i]->pkt_len;
			}
			t = clock() -t;
			double time_taken = ((double)t)/CLOCKS_PER_SEC;
			if(size > 0){
				//printf("total size: %ld\n",size);
				//printf("fun() took %f seconds to execute \n", time_taken);
				printf("Throughput %f bytes per seconds\n",((float)size)/time_taken);
			}
			if (unlikely(nb_rx == 0))
				continue;
			const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0,
					bufs, nb_rx);
			if (unlikely(nb_tx < nb_rx)) {
				uint16_t buf;
				for (buf = nb_tx; buf < nb_rx; buf++)
					rte_pktmbuf_free(bufs[buf]);
			}
		}
	}
}

/* Main function, does initialisation and calls the per-lcore functions */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	uint16_t nb_ports;
	uint16_t portid;
	struct option lgopts[] = {
		{ NULL,  0, 0, 0 }
	};
	int opt, option_index;


	/* init EAL */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	while ((opt = getopt_long(argc, argv, "t", lgopts, &option_index))
			!= EOF)
		switch (opt) {
		case 't':
			hw_timestamping = 1;
			break;
		default:
			printf(usage, argv[0]);
			return -1;
		}
	optind = 1; /* reset getopt lib */

	nb_ports = rte_eth_dev_count_avail();
	printf("number of availabkle port: %d\n",nb_ports);
	if (nb_ports < 2 || (nb_ports & 1))
		rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
		NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
		RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(portid)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too much enabled lcores - "
			"App uses only 1 lcore\n");

	/* call lcore_main on master core only */
	signal(SIGINT,initHandler);
	lcore_main();
	return 0;
}
