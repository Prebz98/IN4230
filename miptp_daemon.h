#include "general.h"
#include <stdint.h>
#include <sys/poll.h>

#define APPROVED 1
#define DENIED 0

struct app_pdu{
    uint8_t mip;
    uint8_t port;
    char sdu[BUFSIZE-2];
}__attribute__((packed));

struct miptp_pdu{
    uint8_t src_port;
    uint8_t dst_port;
    uint16_t seq : 14;
    uint8_t padding : 2;
    char sdu[BUFSIZE-4];
}__attribute__((packed));

int argparser(int argc, char **argv,int *timeout_msecs, char* mip_daemon, char* path_to_higher);
int connect_to_mip_daemon(char* path_to_mip, struct pollfd *fds);
void write_identifying_msg(int mip_fd);
void setup_unix_socket(char *path, struct pollfd *fds);
int available_port(uint8_t *port_numbers, uint8_t port);
void send_port_response(int fd, uint8_t port, int approved);
void forward_to_mip(int mip_daemon, int application, uint8_t app_port);
int index_of_port(uint8_t port, uint8_t *port_numbers, int number_of_ports);