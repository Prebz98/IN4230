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
    /*
    * sends a request to the routing_daemon

    * mip_from: my own mip address
    * mip_to: mip address to send to
    * router_socket: the fd of the routing_daemon
    */
    char buffer[BUFSIZE];
    struct unix_packet *packet = (struct unix_packet*)buffer;
    memset(packet->msg, 0, BUFSIZE);

    packet->mip = mip_from;
    packet->ttl = 0;
    memcpy(packet->msg, "REQ", 3);
    memcpy(&packet->msg[3], &mip_to, 1);

    write(router_socket, packet, 8);
}

void handle_routing_msg(struct pollfd *fds, uint8_t my_mip, struct cache *cache_table, struct raw_packet *waiting_message, int waiting_sdu_len){
    /*
    * takes one message from the routing_daemon and acts based on the type of message. (hello, update, response)

    * fds: all saved fds
    * my_mip: my own mip address
    * cache_table: the arp-cache table
    * waiting_message: unsent message
    * waiting_sdu_len: length of the sdu in the waiting message
    */
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    int rc;
    rc = read(fds[3].fd, buffer, BUFSIZE);
    if (rc == -1){
        error(rc, "read from socket");
    }
    uint8_t sdu_len = rc/4;
    struct unix_packet *packet = (struct unix_packet*)buffer;

    //its a hello message
    if (0 == memcmp(packet->msg, ROUTING_HELLO, 3)){
        uint8_t raw_buffer[BUFSIZE];
        memset(raw_buffer, 0, BUFSIZE);

        struct mip_hdr hdr = create_mip_hdr(255, my_mip, 1, sdu_len, 0x04);
        memcpy(raw_buffer, &hdr, sizeof(struct mip_hdr));
        memcpy(&raw_buffer[4], ROUTING_HELLO, 3);

        struct sockaddr_ll interfaces[MAX_IF];
        int number_of_if = 0;
        number_of_if = get_mac_from_interface(interfaces);
        uint8_t broadcast_mac[6] = DST_MAC_ADDR;

        //broadcast the hello
        for (int i = 0; i<number_of_if; i++){
            send_raw_packet(&fds[1].fd, &interfaces[i], raw_buffer, 7, broadcast_mac);
        }
        
    }
    //its an update message
    else if (0 == memcmp(packet->msg, ROUTING_UPDATE, 3)) {
        uint8_t raw_buffer[BUFSIZE];
        memset(raw_buffer, 0, BUFSIZE);

        uint8_t number_of_pairs = packet->msg[3];
        uint8_t message_size = 4+(number_of_pairs*sizeof(struct update_pair));

        struct update_pair *list = (struct update_pair*)&packet->msg[4];

        uint8_t total_size = 4+message_size;
        uint8_t rest = total_size % 4;
        total_size += rest ? 4-rest : 0;

        struct mip_hdr hdr = create_mip_hdr(packet->mip, my_mip, 1, message_size, 0x04);
        memcpy(raw_buffer, &hdr, sizeof(struct mip_hdr));
        memcpy(&raw_buffer[sizeof(struct mip_hdr)], packet->msg, message_size);

        int cache_index = check_cache(packet->mip);
        struct sockaddr_ll interface;

        //target mip not found
        if (cache_index == -1){
            //broadcast?
            error(cache_index, "cant find the requested MIP address\n");
        }else { //mip found in cache -> send
            interface = cache_table[cache_index].iface;
            send_raw_packet(&fds[1].fd, &interface, raw_buffer, total_size, cache_table[cache_index].mac);
        }

    }
    // its a response, if cached -> send, else arp broadcast req
    else if (0 == memcmp(packet->msg, ROUTING_RESPONSE, 3)) {
        uint8_t raw_buffer[BUFSIZE];
        memset(raw_buffer, 0, BUFSIZE);

        uint8_t next_mip = packet->msg[3];
        if (next_mip == 255){ //unknown path
            printf("Dont know the path to %d\n", packet->msg[0]);
            return;
        }
        printf("HEHE %d\n", next_mip);

        int cache_index = check_cache(next_mip);
        if (cache_index == -1){
            //mip not in cache -> arp-req
            struct arp_sdu arp_req_sdu = create_arp_sdu(next_mip, true);
            struct mip_hdr mip_hdr = create_mip_hdr(255, my_mip, 1, sizeof(struct arp_sdu), 0x01);
            memset(raw_buffer, 0, BUFSIZE);
            memcpy(raw_buffer, &mip_hdr, sizeof(struct mip_hdr));
            memcpy(&raw_buffer[4], &arp_req_sdu, sizeof(struct arp_sdu));

            struct sockaddr_ll senders_iface[MAX_IF];
            int num_if = get_mac_from_interface(senders_iface);
            uint8_t broadcast_mac[6] = DST_MAC_ADDR;
            for (int i=0; i<num_if; i++){
                send_raw_packet(&fds[1].fd, &senders_iface[i], raw_buffer, 8, broadcast_mac);
            }
        }else { //mip in cache
            struct sockaddr_ll interface;

            uint8_t size = sizeof(struct mip_hdr)+waiting_sdu_len;
            uint8_t rest = size % 4;
            size += rest ? 4-rest : 0;

            memcpy(raw_buffer, waiting_message, size);
            memcpy(&interface, &cache_table[cache_index].iface, sizeof(struct sockaddr_ll));

            send_raw_packet(&fds[1].fd, &interface, raw_buffer, size, cache_table[cache_index].mac);
        }
    }
}