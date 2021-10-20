#include "general.h"
#include <bits/types/struct_timeval.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "routing_daemon.h"
#include <sys/time.h>


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
    write(sock_server, buffer, 4);
    printf("Identify myself for daemon\n");
}

void write_hello(int sock_server){
    /*
    * write hello to all neighbors
    * sock_server: socket fd
    */


    char buffer[BUFSIZE];
    struct unix_packet *down = (struct unix_packet*)buffer;
    memset(buffer, 0, BUFSIZE);
    down->mip = 255;
    down->ttl = 1;
    strcpy(down->msg, ROUTING_HELLO);

    //32 bit align
    uint8_t message_len = strlen(down->msg) + 2;
    uint8_t rest = message_len % 4;
    message_len += rest ? 4-rest : 0;
    
    int rc;
    rc = write(sock_server, buffer, message_len);
    printf("Sent HLO %d bytes\n", rc);
}

void add_to_linked_list(uint8_t distance, uint8_t mip, uint8_t next_mip, struct node *routing_list){
    /*
    * pushes one element in the back of the linked list
    * distance: distance to mip
    * mip: target mip
    * next_mip: next node on the path to target
    * routing_list: linked routing list
    */

    struct node *current_node = routing_list;
    while (current_node->next != NULL) {
        current_node = current_node->next;
    }
    struct node *end_node = current_node;
    struct timeval time;
    gettimeofday(&time, NULL);

    struct node *new = malloc(sizeof(struct node));
    new->distance = distance;
    new->mip = mip;
    new->next_mip = next_mip;
    new->last_rcv = time;
    new->next = NULL;
    end_node->next = new;
}

int handle_hello(uint8_t mip, struct node *routing_list){
    /*
    * checks if the mip stored already, then changes the distance to 1. Otherwise adds a new node at the end with distance 1. 
    * mip: mip to store
    * routing_list: startnode of linked list
    *
    * returns 1 if a change was made, else 0
    */
    struct timeval time;
    struct node *i = routing_list;
    //checks if the address is saved with higher distance
    while (i->next != NULL) {
        i = i->next;
        if (i->mip == mip){
            gettimeofday(&time, NULL);
            i->last_rcv = time;
            if (i->distance != 1){
                i->distance = 1;
                return 1;
            }
            return 0;
        }
    }
    //it is not saved => add new node to list
    add_to_linked_list(1, mip, mip, routing_list);
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
    /*
    * prints the routing list

    * list: the first node of the routing list
    */
    printf("\nRouting list:\n");
    struct node *x = list;

    while (x->next != NULL) {
        x = x->next;
        printf("{MIP: %d - Distance: %d - Next hop: %d}\n", x->mip, x->distance, x->next_mip);
    }
    printf("\n");
}

void send_update(struct node *routing_list, int sock_server){
    /*
    * sends an update message with the entire routing table
    
    * routing_list: the routing table that we will send
    * sock_server: socket fd to send through
    */
    struct node *current_node_to_send = routing_list;
    
    //routing list has one empty node at the beginning
    // so we skip that one
    // here we are createing the response to be sent
    //sending to neighbors
    current_node_to_send = routing_list;
    while (current_node_to_send->next != NULL) {
        current_node_to_send = current_node_to_send->next;
        char update_buffer[BUFSIZE];
        memset(update_buffer, 0, BUFSIZE);
        memcpy(update_buffer, ROUTING_UPDATE, 3);
        //save one slot for number of pairs
        struct update_pair *pair_list = (struct update_pair *)&update_buffer[4];
        uint8_t index = 0;

        //dont send update back to the one that caused it
        // if (current_node_to_send->mip == mip_caused_update) {
        //     continue;
        // }

        if (current_node_to_send->distance == 1) {
            
            //building up a message list
            struct node *current_message_node = routing_list;
            while(current_message_node->next != NULL){
                current_message_node = current_message_node->next;
                struct update_pair pair;
                pair.mip_target = current_message_node->mip;

                if (current_message_node->mip == current_node_to_send->mip){
                    // dont send its own adress to a node
                    continue;
                }

                //sending MAX_DISTANCE if next hop is equal to the one we are sending to
                else if (current_message_node->next_mip == current_node_to_send->mip){
                    pair.distance = MAX_DISTANCE;
                    pair_list[index] = pair;
                    index++;
                }else {
                    pair.distance = current_message_node->distance;
                    pair_list[index] = pair;
                    index++;
                }
            }

            //number of pairs
            update_buffer[3] = index;
            uint8_t message_size = 4+(index*sizeof(struct update_pair));

            //create a unix packet
            char buffer_to_send[BUFSIZE];
            memset(buffer_to_send, 0, BUFSIZE);
            struct unix_packet *packet = (struct unix_packet*)buffer_to_send;
            packet->ttl = 1;
            memcpy(packet->msg, update_buffer, message_size);

            uint8_t total_size = 2+message_size;
            uint8_t rest = total_size % 4;
            total_size += rest ? 4-rest : 0;

            packet->mip = current_node_to_send->mip;
            int rc;
            rc = write(sock_server, buffer_to_send, total_size);
            printf("Sent update to %d, %d bytes\n", packet->mip, rc);

        }
    }
}

bool update_routing_list(struct unix_packet *packet, struct node *routing_list){
    /*
    * updating the routing table
    * packet: the update packet received
    * routing_list: current routing list
    */

    //this boolean is true if a change where made, then we have to update others
    bool change = false;
    uint8_t num_pairs = packet->msg[3];
    struct update_pair *pairs = (struct update_pair*)&(packet->msg[4]);

    // go through the pairs
    int i = 0;
    while (i < num_pairs) {
        struct update_pair current_pair = pairs[i];
        bool known_address = false;

        struct node *current_node = routing_list;
        //go through the linked list for all pairs
        while (current_node->next != NULL) {
            current_node = current_node->next;

            //the address is known
            if (current_node->mip == current_pair.mip_target){
                known_address = true;
                // if current path is broken
                if (current_pair.distance == MAX_DISTANCE &&
                packet->mip == current_node->next_mip){
                    current_node->distance = MAX_DISTANCE;
                    change = true;
                }
                //if the prevous saved distance is bigger => swap
                else if (current_node->distance > current_pair.distance+1) {
                    current_node->distance = current_pair.distance+1;
                    change = true;   
                }
            }
        }
        //address is not known => add to linked list
        if (!known_address){
            add_to_linked_list(current_pair.distance+1, current_pair.mip_target, packet->mip, routing_list);
            change = true; 
        }
        i++;
    }
    return change;
}

uint8_t lookup(uint8_t mip_target, struct node *routing_list){
    /*
    * finds the next hop in the for messages targeted mip_target
    * mip_target: mip address the message is for
    * routing_list: the routing table

    * returns: (uint8_t) the mip address to send the packet to
    * returns 255 if the target is not found, or if the distance is MAX
    */
    struct node *current_node = routing_list;

    while (current_node->next != NULL) {
        current_node = current_node->next;

        if (current_node->mip == mip_target && current_node->distance != MAX_DISTANCE){
            return current_node->next_mip;
        }
    }
    return 255;
}

void read_from_socket(int sock_server, char* buffer, bool *done, struct node *routing_list){
    /*
    * reads a message from unix socket, stops if connection is closed or an error occurs
    
    * sock_server: socket fd
    * buffer: to store message
    * done: boolean to stop the program
    * routing_list: the routing table, linked list
    */
    int rc;
    if ((rc = read(sock_server, buffer, BUFSIZE)) == -1 || rc == 0){
        *done = true;
        return;
    }

    struct unix_packet *packet = (struct unix_packet*)buffer;
    printf("%d bytes received from mip %d\n", rc, packet->mip);

    //its a hello message
    if (memcmp(packet->msg, ROUTING_HELLO, 3) == 0){
        printf("It was a hello message\n");
        //update table, if a change was made, send an update
        int res = handle_hello(packet->mip, routing_list);
        if (res == 1){
            send_update(routing_list, sock_server);
        }
    }
    //its an update, update the table
    else if (memcmp(packet->msg, ROUTING_UPDATE, 3) == 0) {
        printf("It was an update message\n");
        //if a change was made, send an update
        if (update_routing_list(packet, routing_list)){
            send_update(routing_list, sock_server);
        }
    }
    //its a request, lookup the MIP and send a response
    else if (memcmp(packet->msg, ROUTING_REQUEST, 3) == 0) {
        printf("Its was a request message\n");
        uint8_t mip_to = packet->msg[3];
        uint8_t mip_from = packet->mip;
        uint8_t mip_next = lookup(mip_to, routing_list);

        char down_buf[BUFSIZE];
        memset(down_buf, 0, BUFSIZE);
        struct unix_packet *down = (struct unix_packet*)down_buf;
        down->mip = mip_from;
        down->ttl = 0;
        memcpy(down->msg, ROUTING_RESPONSE, 3);
        memcpy(&down->msg[3], &mip_next, 1);

        uint8_t total_size = 8;
        write(sock_server, down_buf, total_size);
        printf("Sent a response message\n");
    }
    print_routing_list(routing_list);
}

void identify_broken_path(struct node *routing_list, int sock_server){
    /*
    * if it is over 12 seconds since last hello from a neighbor -> max distance
    
    * routing_list: routing table as a linked list
    * sock_server: fd to mip daemon
    */
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    struct node *current_node = routing_list;
    while (current_node->next != NULL) {
        current_node = current_node->next;
        if (current_node->distance == 1){
            double diff = (double)(current_time.tv_sec - current_node->last_rcv.tv_sec);
            if (diff > 12){
                current_node->distance = MAX_DISTANCE;
                send_update(routing_list, sock_server);
            }
        }
    }
}

int argparser(int argc, char **argv, char* path) {
    /*
    * parses all arguments

    * path: path to unix socket fd
    */
    int index;
    int c;

    opterr = 0;

    while ((c = getopt (argc, argv, "h")) != -1)
        switch (c)
        {
        case 'h':
            printf("How to run\n./routing_daemon [-h] <socket lower>\nAlternative: Use the makefile commands.\n");
            printf("Optional args:\n\
            -h Help\n\
            Non-optional args:\n\
            Socket lower: filename for unix socket to lower layer\n\n");
            printf("Program description:\nThe program will identidy itself to the MIP_daemon through the unix socket. Then it will communicate with other routing daemons through the MIP_daemon. It will create a routing table and keep updating it. It shuts down when the connection to the MIP_daemon is lost.\n");
            exit(EXIT_SUCCESS);
            break;
        default:
            abort ();
        }

    index = optind;
    strcpy(path, argv[index]);
    return 0;
}

