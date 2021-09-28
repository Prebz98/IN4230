#include "general.h"
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <unistd.h>
#include "routing_utils.h"

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

    char *message = "HLO";
    write(sock_server, message, strlen(message));
    printf("Sent HELLO\n");
}


