#include "general.h"
#include "routing_utils.h"
#include <string.h>
#include <sys/un.h>

int main(int argc, char* argv[]){
    int sock_server;
    struct sockaddr_un serv_addr;
    char path[BUFSIZE];

    strcpy(path, argv[1]);
    sock_server = 0;
    socket_setup(path, &sock_server, &serv_addr);

    write_identifying_msg(sock_server);
    write_hello(sock_server);
}