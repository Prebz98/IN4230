#include "general.h"
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdbool.h>
#include "routing_daemon.h"

void error(int ret, char *msg) {
    //program error
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

void socket_setup(char *path, int *sock_server, struct sockaddr_un *serv_addr) {
    /*
    * setting up unix socket, and connecting to it
    * path: path to unix socket fd
    * sock_server: socket fd
    * serv_addr: socket address
    */

    if ((*sock_server = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0){
        error(*sock_server, "socket setup failed\n");
    }
    //fcntl(*sock_server, F_SETFL, O_NONBLOCK);
    memset(serv_addr, 0, sizeof(struct sockaddr_un));
    serv_addr->sun_family = AF_UNIX;
    strcpy(serv_addr->sun_path, path);
    if ((strlen(path)) < strlen(serv_addr->sun_path)){
        error(-1, "Path is too long");
    }
    if (0 != (connect(*sock_server, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_un)))){
        error(-1, "Failed to connect to server");
    }
}

void write_identifying_msg(int sock_server){
    /*
    * writing an identifying message to the daemon
    * sock_server: socket fd
    */

    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    uint8_t sdu_type = 0x04; //routing sdu type
    memset(buffer, sdu_type, 1);
    write(sock_server, buffer, 1);
    printf("Identify myself for daemon\n");
}

void write_hello(int sock_server){
    /*
    * write hello to all neighbors
    * sock_server: socket fd
    */

    char *message = ROUTING_HELLO;
    struct unix_packet down;
    memset(&down, 0, sizeof(struct unix_packet));
    down.mip = 255;
    down.ttl = 1;
    strcpy(down.msg, message);
    write(sock_server, &down, sizeof(struct unix_packet));
    printf("Sent HELLO\n");
}

int handle_hello(uint8_t mip, struct node *routing_list){
    /*
    * checks if the mip stored already, then changes the distance to 1. Otherwise adds a new node at the end with distance 1. 
    * mip: mip to store
    * routing_list: startnode of linked list
    *
    * returns 1 if a change was made, else 0
    */

    struct node *i = routing_list;
    
    // if (i->mip == mip){
    //     if (i->distance != 1){
    //         i->distance = 1;
    //         return 1;
    //     }
    //     return 0;
    // }
    while (i->next != NULL) {
        i = i->next;
        if (i->mip == mip){
            if (i->distance != 1){
                i->distance = 1;
                return 1;
            }
            return 0;
        }
    }

    struct node *new = malloc(sizeof(struct node));
    new->distance = 1;
    new->mip = mip;
    new->next_mip = mip;
    i->next = new;
    return 1;
}

void free_linked_list(struct node *list){
    /*
    * freeing the linked list with nodes
    * list: startnode of the list
    */

    
    struct node *x = list;
    struct node *y;

    while (x->next != NULL){
        y = x->next;
        free(x);
        x = y;
    }
    free(x);
}

void print_routing_list(struct node *list){
    printf("\nRouting list:\n");
    struct node *x = list;

    while (x->next != NULL) {
        x = x->next;
        printf("{MIP: %d - Distance: %d - Next hop: %d}\n", x->mip, x->distance, x->next_mip);
    }
    printf("\n");
}

void send_update(struct node *routing_list, int sock_server, int mip_caused_update){
    /*
    * sends an update message with the entire routing table
    * routing_list: the routing table that we will send
    * sock_server: socket fd to send through
    * mip_caused_update: mip that the packet will not be sent to
    */
    struct node *x = routing_list;
    char update_buffer[BUFSIZE];
    memset(update_buffer, 0, BUFSIZE);
    memcpy(update_buffer, ROUTING_UPDATE, 3);
    //save one slot for number of pairs
    struct update_pair *pair_list = (struct update_pair *)&update_buffer[4];
    int index = 0;
    //routing list has one empty node at the beginning
    // so we skip that one
    // here we are createing the response to be sent
    while(x->next != NULL){
        x = x->next;
        struct update_pair pair;
        pair.mip_target = x->mip;
        pair.distance = x->distance;
        memcpy(&pair_list[index], &pair, sizeof(struct update_pair));
        index++;
    }
    
    //number of packets
    update_buffer[3] = index;
    int message_size = 4+(index*sizeof(struct update_pair));

    //create a unix packet
    char* buffer_to_send[BUFSIZE];
    memset(buffer_to_send, 0, BUFSIZE);
    struct unix_packet *packet = (struct unix_packet*)buffer_to_send;
    packet->ttl = 1;
    memcpy(packet->msg, update_buffer, message_size);

    //sending to neighbors, except the one that caused the update
    x = routing_list;
    while (x->next != NULL) {
        x = x->next;
        if (x->distance == 1 && x->mip != mip_caused_update) {
            packet->mip = x->mip;

            write(sock_server, packet, message_size);
            printf("Sent update to %d, %d\n", packet->mip, *(packet->msg+4));
        }
    }
    print_routing_list(routing_list);
}

void update_routing_list(struct unix_packet *packet, struct node *routing_list){
    /*
    * updating the routing table
    * packet: the update packet received
    * routing_list: current routing list
    */


    struct update_pair *pair = (struct update_pair*)&(packet->msg[4]);
    printf("First pair %d %d\n", pair[0].mip_target, pair[0].distance);
}

void read_from_socket(int sock_server, char* buffer, bool *done, struct node *routing_list){
    /*
    * reads a message from unix socket, stops if connection is closed or an error occurs
    * sock_server: socket fd
    * buffer: to store message
    * done: boolean to stop the program
    */
    int x;
    if ((x = read(sock_server, buffer, sizeof(buffer))) == -1 || x == 0){
        *done = true;
        return;
    }

    struct unix_packet *packet = (struct unix_packet*)buffer;
    printf("Packet received with msg: \"%s\" from mip %d\n", packet->msg, packet->mip);

    //its a hello message
    if (memcmp(packet->msg, ROUTING_HELLO, 3) == 0){
        int res = handle_hello(packet->mip, routing_list);
        if (res == 1){
            send_update(routing_list, sock_server, packet->mip);
        }
    }
    //its an update
    else if (memcmp(packet->msg, ROUTING_UPDATE, 3) == 0) {
         update_routing_list(packet, routing_list);
    }
}


