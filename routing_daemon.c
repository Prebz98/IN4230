#include "routing_utils.c"
#include <sys/un.h>

int main(int argc, char* argv[]){
    int sock_server;
    struct sockaddr_un serv_addr[1];
    char* path;

    path = argv[1];
    sock_server = 0;
    socket_setup(path, &sock_server, serv_addr);

    write_identifying_msg(sock_server);
}