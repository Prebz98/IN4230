#include <stdint.h>
#include "general.h"

struct miptp_pdu{
    uint8_t mip;
    uint8_t port;
    char sdu[BUFSIZE-2];
}__attribute__((packed));

int argparser(int argc, char **argv, char *path_to_file, char *path_to_miptp, uint8_t *dst_mip, uint8_t *dst_port);
int connect_to_miptp(char* path_to_mip);
void write_identifying_msg(int miptp_fd, uint8_t my_port);
