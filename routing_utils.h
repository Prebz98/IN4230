#include <sys/un.h>

void error(int ret, char* msg);
void socket_setup(char *path, int *sock_server,struct sockaddr_un *serv_addr);
void write_identifying_msg(int sock_server);
void write_hello(int sock_server);