#include "general.h"
#include "routing_daemon.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char* argv[]){
    /*
    * 
    */

    clock_t time_sent_hlo;
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
    struct pollfd pollfd;
    pollfd.fd = sock_server;
    pollfd.events = POLLIN | POLLHUP;

    write_identifying_msg(sock_server);
    write_hello(sock_server);
    time_sent_hlo = clock();

    while (!done) {
        poll(&pollfd, 1, 1);
        //write hello every 5th second 
        if (((clock()-time_sent_hlo)*100.0/CLOCKS_PER_SEC) > 5){
            write_hello(sock_server);
            time_sent_hlo = clock();
        }

        if (pollfd.revents & POLLHUP) {
            printf("MIP daemon disconnected\n");
            done = true;
        } 
        else if (pollfd.revents & POLLIN) {
            read_from_socket(sock_server, buffer, &done, routing_list);
        }
    }
    free_linked_list(routing_list);
}