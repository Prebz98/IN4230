#include "general.h"
#include <sys/poll.h>

#define APPROVED 1
#define DENIED 0

int argparser(int argc, char **argv,int *timeout_msecs, char* mip_daemon, char* path_to_higher);
int connect_to_mip_daemon(char* path_to_mip, struct pollfd *fds);
void write_identifying_msg(int mip_fd);
void setup_unix_socket(char *path, struct pollfd *fds);
int available_port(uint8_t *port_numbers, uint8_t port);
void send_port_response(int fd, uint8_t port, int approved);
void forward_to_mip(int mip_daemon, int application);