#ifndef MIP_DAEMON
#define MIP_DAEMON
//#include <cstdint>
#include <linux/if_packet.h>
#include <stdint.h>

#define MAX_EVENTS 4
#define CACHE_TABLE_LEN 4
#define MAX_IF 4
#define DST_MAC_ADDR {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

struct ether_frame {
    uint8_t dst_addr[6];
    uint8_t src_addr[6];
    uint8_t eth_protocol[2];
    uint8_t contents[0];
}__attribute__((packed));

struct arp_sdu {
    uint8_t type : 1;
    uint16_t address : 8; //:8 eller ikke
    uint32_t padding : 23;
}__attribute__((packed));

struct mip_hdr {
    uint8_t dst;
    uint8_t src;
    uint8_t ttl : 4;
    uint16_t sdu_len : 9;
    uint8_t sdu_type : 3;
}__attribute__((packed));

struct cache {
    uint8_t mip;
    struct sockaddr_ll iface;
    uint8_t mac[6];
};

// closing socket and free allocated memory
void clear_memory();
void print_cache();
void print_mac(uint8_t* mac);

#endif