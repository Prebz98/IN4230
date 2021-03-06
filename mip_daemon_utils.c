#include "general.h"
#include <linux/if_packet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

void handle_routing_msg(struct pollfd *fds, uint8_t my_mip, struct cache *cache_table, struct waiting_queue *routing_queue, struct waiting_queue *arp_queue){
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
        int message_size = 4+(number_of_pairs*sizeof(struct update_pair));

        struct update_pair *list = (struct update_pair*)&packet->msg[4];

        int total_size = 4+message_size;
        int rest = total_size % 4;
        total_size += rest ? 4-rest : 0;

        struct mip_hdr hdr = create_mip_hdr(packet->mip, my_mip, 1, message_size, 0x04);
        memcpy(raw_buffer, &hdr, sizeof(struct mip_hdr));
        memcpy(&raw_buffer[sizeof(struct mip_hdr)], packet->msg, message_size);

        int cache_index = check_cache(packet->mip);
        struct sockaddr_ll interface;

        //target mip not found
        if (cache_index == -1){
            //arp req?
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
            printf("Dont know the path\n");
            return;
        }

        int cache_index = check_cache(next_mip);
        if (cache_index == -1){
            //mip not in cache -> arp-req
            //save message
            struct raw_packet packet_to_send = pop_waiting_queue(routing_queue);
            add_to_waiting_queue(arp_queue, packet_to_send);

            //create arp request
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
            struct raw_packet packet = pop_waiting_queue(routing_queue);

            int size = (packet.hdr.sdu_len*4) + 4;
            int rest = size % 4;
            size += rest ? 4-rest : 0;

            memcpy(raw_buffer, &packet, size);
            memcpy(&interface, &cache_table[cache_index].iface, sizeof(struct sockaddr_ll));

            send_raw_packet(&fds[1].fd, &interface, raw_buffer, size, cache_table[cache_index].mac);
        }
    }
}

void add_to_waiting_queue(struct waiting_queue *routing_queue, struct raw_packet packet){
    struct waiting_queue *current_node = routing_queue;

    while (current_node->next != NULL) {
        current_node = current_node->next;
    }

    struct waiting_queue *new = malloc(sizeof(struct waiting_queue));
    new->packet = packet;
    new->next = NULL;

    current_node->next = new;
}

struct raw_packet pop_waiting_queue(struct waiting_queue *routing_queue){
    /*
    * gets the first element of the queue if there is one, and removes it from the list

    * routing_queue: routing queue linked list

    * returns: (struct raw_packet) packet of the first node
    */
    struct waiting_queue *current_node = routing_queue;

    // if queue is empty, return empty packet
    if (current_node->next == NULL){
        printf("Queue is empty\n");
        return routing_queue->packet;
    }

    current_node = current_node->next;
    routing_queue->next = current_node->next;
    struct raw_packet to_return = current_node->packet;
    free(current_node);
    return to_return;
}

void free_waiting_queue(struct waiting_queue *routing_queue){
    /*
    * free the routing_queue

    * routing_queue: routing_queue
    */
    while (routing_queue->next != NULL) {
        pop_waiting_queue(routing_queue);
    }
    free(routing_queue);
}

void send_arp_queue(struct waiting_queue *arp_queue, uint8_t mip, struct pollfd *fds, struct cache *cache_table){
    /*
    * sending all messages targeted a specified mip

    * arp_queue: the arp_queue linked list
    * mip: the mip we will send to
    */

    uint8_t raw_buffer[BUFSIZE];

    struct waiting_queue *current_node = arp_queue;
    struct sockaddr_ll interface;
    while (current_node->next != NULL) {
        current_node = current_node->next;
        //if the destination to the waiting message is equal to the one we now have -> send it
        if (current_node->packet.hdr.dst == mip){
            memset(raw_buffer, 0, BUFSIZE);
            int size = sizeof(struct mip_hdr) + (4*current_node->packet.hdr.sdu_len);
            memcpy(raw_buffer, &current_node->packet, size);
            int index = check_cache(mip);

            if (index == -1){
                error(index, "Did not find mip in cache that should be there");
            }
            memcpy(&interface, &cache_table[index].iface, sizeof(struct sockaddr_ll));
            send_raw_packet(&fds[1].fd, &interface, raw_buffer, size, cache_table[index].mac);
        }
    }
}