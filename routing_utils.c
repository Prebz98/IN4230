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
    
    if (i->mip == mip){
        if (i->distance != 1){
            i->distance = 1;
            return 1;
        }
        return 0;
    }
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

void find_targets_for_update(int mip_caused_update){
    /*
    * finds all mip addresses to send update to
    * mip_caused_update: the node that sent us new data
    * we will not send the update back to that node.  
    */
}

void send_update(struct node *routing_list, int sock_server){
    /*
    * sends an update message with the entire routing table
    * routing_list: the routing table that we will send
    * sock_server: socket fd to send through
    */
    struct node *x = routing_list;
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    memcpy(buffer, ROUTING_UPDATE, 3);
    struct update_pair *packet_list = (struct update_pair *)&buffer[3];
    int index = 0;
    
    //routing list has one empty node at the beginning
    // so we skip that one
    while(x->next != NULL){
        x = x->next;
        struct update_pair packet;
        packet.mip_target = x->mip;
        packet.distance = x->distance;
        packet_list[index] = packet;
        index++;
    }

    //send the buffer with the data
    struct unix_packet packet;
    packet.mip = 10; //TODO
    packet.ttl = 1;
    strcpy(packet.msg, buffer);

    write(sock_server, &packet, sizeof(struct unix_packet));
    printf("Sent update\n");
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
    printf("Packet received with msg: %s from mip %d\n", packet->msg, packet->mip);
    if (strcmp(packet->msg, ROUTING_HELLO) == 0){
        int res = handle_hello(packet->mip, routing_list);
        if (res == 1){
            send_update(routing_list, sock_server);
        }
    }
}


