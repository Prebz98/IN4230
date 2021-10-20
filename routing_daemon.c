#include "general.h"
#include "routing_daemon.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char* argv[]){
    /*
    * 
    */

    struct timeval time_sent_hlo, current_time;
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
    gettimeofday(&time_sent_hlo, NULL);

    while (!done) {
        poll(&pollfd, 1, 1);
        //write hello every 5th second 
        gettimeofday(&current_time, NULL);
        if (((current_time.tv_sec-time_sent_hlo.tv_sec)) > 5){
            write_hello(sock_server);
            gettimeofday(&time_sent_hlo, NULL);
        }
        identify_broken_path(routing_list, sock_server);

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