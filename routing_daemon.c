#include "general.h"
#include "routing_daemon.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char* argv[]){
    /*
    * 
    */

    int sock_server;
    struct sockaddr_un serv_addr;
    char path[BUFSIZE];
    bool done = false;
    char buffer[BUFSIZE];
    struct node *routing_list = malloc(sizeof(struct node));
    routing_list->next = NULL;
    routing_list->mip = 0;

    strcpy(path, argv[1]);
    sock_server = 0;
    socket_setup(path, &sock_server, &serv_addr);

    write_identifying_msg(sock_server);
    write_hello(sock_server);

    while (!done) {
        read_from_socket(sock_server, buffer, &done, routing_list);
    }
    free_linked_list(routing_list);
}