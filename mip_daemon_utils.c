#include "general.h"
#include <linux/if_packet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <unistd.h>
#include "mip_daemon.h"

void send_req_to_router(uint8_t mip_from, uint8_t mip_to, int router_socket){
    char buffer[BUFSIZE];
    struct unix_packet *packet = (struct unix_packet*)buffer;
    memset(packet->msg, 0, BUFSIZE);

    packet->mip = mip_from;
    packet->ttl = 0;
    memcpy(packet->msg, "REQ", 3);
    memcpy(&packet->msg[3], &mip_to, 1);

    write(router_socket, packet, 8);
}

void handle_routing_msg(struct pollfd *fds, uint8_t my_mip, struct cache *cache_table){
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    int rc;
    rc = read(fds[3].fd, buffer, BUFSIZE);
    if (rc == -1){
        error(rc, "read from socket");
    }
    uint8_t size = rc;
    struct unix_packet *packet = (struct unix_packet*)buffer;

    //its a hello message
    if (0 == memcmp(packet->msg, ROUTING_HELLO, 3)){
        printf("Received hello from router\n");
        uint8_t raw_buffer[BUFSIZE];
        memset(raw_buffer, 0, BUFSIZE);

        struct mip_hdr hdr = create_mip_hdr(255, my_mip, 1, size, 0x04);
        memcpy(raw_buffer, &hdr, sizeof(struct mip_hdr));
        memcpy(&raw_buffer[4], ROUTING_HELLO, 3);

        struct sockaddr_ll interfaces[MAX_IF];
        int number_of_if = 0;
        number_of_if = get_mac_from_interface(interfaces);
        uint8_t broadcast_mac[6] = DST_MAC_ADDR;

        for (int i = 0; i<number_of_if; i++){
            send_raw_packet(&fds[1].fd, &interfaces[i], raw_buffer, 7, broadcast_mac);
        }
        
    }
    //its an update message
    else if (0 == memcmp(packet->msg, ROUTING_UPDATE, 3)) {
        printf("Received an update\n");
        uint8_t raw_buffer[BUFSIZE];
        memset(raw_buffer, 0, BUFSIZE);

        uint8_t number_of_pairs = packet->msg[3];
        uint8_t message_size = 4+(number_of_pairs*sizeof(struct update_pair));

        struct update_pair *list = (struct update_pair*)&packet->msg[4];
        for (int i=0;i<number_of_pairs;i++){
            printf("MIP in packet %d\n", list[i].mip_target);
        }

        uint8_t total_size = 4+message_size;
        uint8_t rest = total_size % 4;
        total_size += rest ? 4-rest : 0;

        struct mip_hdr hdr = create_mip_hdr(packet->mip, my_mip, 1, message_size, 0x04);
        memcpy(raw_buffer, &hdr, sizeof(struct mip_hdr));
        memcpy(&raw_buffer[sizeof(struct mip_hdr)], packet->msg, message_size);

        int cache_index = check_cache(packet->mip);
        struct sockaddr_ll interface;

        if (cache_index == -1){
            //broadcast?
        }else {
            interface = cache_table[cache_index].iface;
            send_raw_packet(&fds[1].fd, &interface, raw_buffer, total_size, cache_table[cache_index].mac);
        }

    }
    else if (0 == memcmp(packet->msg, ROUTING_RESPONSE, 3)) {
        uint8_t raw_buffer[BUFSIZE];
        memset(raw_buffer, 0, BUFSIZE);

        uint8_t next_mip = packet->msg[5];
        
    }
}

void send_to_router(char *msg, uint8_t msg_size, uint8_t mip_dst, int sock_server){
    write_to_unix_socket(msg, msg_size, mip_dst, sock_server, 0);
}