#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>

void error(int ret, char *msg) {
    //program error
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

void socket_setup(char *path, int *sock_server, struct sockaddr_un *serv_addr) {

    if ((*sock_server = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0){
        error(*sock_server, "socket setup failed\n");
    }
    fcntl(*sock_server, F_SETFL, O_NONBLOCK);
    memset(serv_addr, 0, sizeof(struct sockaddr_un));
}