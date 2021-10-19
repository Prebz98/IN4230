#ifndef MIP_DAEMON
#define MIP_DAEMON
#include "general.h"
#include <linux/if_packet.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/poll.h>
#include <stdbool.h>

#define MAX_EVENTS 4
#define CACHE_TABLE_LEN 4
#define MAX_IF 4
#define DST_MAC_ADDR {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#define NUMBER_OF_UNIX_CONNECTIONS 2

struct ether_frame {
    uint8_t dst_addr[6];
    uint8_t src_addr[6];
    uint8_t eth_protocol[2];
    uint8_t contents[0];
}__attribute__((packed));

struct arp_sdu {
    uint8_t type : 1;
    uint16_t address : 8;
    uint32_t padding : 23;
}__attribute__((packed));

struct cache {
    uint8_t mip;
    struct sockaddr_ll iface;
    uint8_t mac[6];
};

struct update_pair {
    uint8_t mip_target;
    uint8_t distance;
};

// closing socket and free allocated memory
void clear_memory();
void print_cache();
void print_mac(uint8_t* mac);
struct mip_hdr create_mip_hdr(uint8_t dst, uint8_t src, uint8_t ttl, uint16_t sdu_len, uint8_t sdu_type);
int get_mac_from_interface(struct sockaddr_ll *senders_iface);
int send_raw_packet(int *raw_sock, struct sockaddr_ll *so_name, uint8_t *buf, size_t len, uint8_t dst_mac[6]);
void handle_routing_msg(struct pollfd *fds, uint8_t my_mip, struct cache *cache_table, struct raw_packet *waiting_message, int waiting_msg_len);
void send_to_router(char *msg, uint8_t msg_size, uint8_t mip_dst, int sock_server);
void write_to_unix_socket(char *msg, uint8_t msg_size, uint8_t mip_dst, int sock_server, uint8_t ttl);
int check_cache(uint8_t mip);
void error(int ret, char *msg);
void send_req_to_router(uint8_t mip_from, uint8_t mip_to, int router_socket);
struct arp_sdu create_arp_sdu(uint8_t mip, bool req);
#endif