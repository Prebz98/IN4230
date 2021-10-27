#include "general.h"
#include <sys/poll.h>

int argparser(int argc, char **argv,int *timeout_msecs, char* mip_daemon, char* path_to_higher);
int connect_to_mip_daemon(char* path_to_mip, struct pollfd *fds);
void write_identifying_msg(int mip_fd);
void setup_unix_socket(char *path, struct pollfd *fds);