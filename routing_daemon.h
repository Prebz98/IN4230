#include "general.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/un.h>
#include <stdbool.h>

#define MAX_DISTANCE 255

struct node {
    struct node* next;
    uint8_t mip;
    uint8_t distance;
    uint8_t next_mip;
    struct timeval last_rcv;
};

struct update_pair {
    uint8_t mip_target;
    uint8_t distance;
};

void error(int ret, char* msg);
void socket_setup(char *path, int *sock_server,struct sockaddr_un *serv_addr);
void write_identifying_msg(int sock_server);
void write_hello(int sock_server);
void read_from_socket(int sock_server, char* buffer, bool *done, struct node *routing_list);
void free_linked_list(struct node *list);
void identify_broken_path(struct node *routing_list, int sock_server);
int argparser(int argc, char **argv, char* path);
void send_update(struct node *routing_list, int sock_server);