#include <stdint.h>
#include <stdio.h>
#include "general.h"

struct miptp_pdu{
    uint8_t mip;
    uint8_t port;
    char sdu[BUFSIZE-2];
}__attribute__((packed));

struct link{
    uint8_t port;
    uint8_t mip;
    FILE *file;
};

int connect_to_miptp(char* path_to_mip);
void write_identifying_msg(int miptp_fd, uint8_t my_port);
int read_from_socket(int miptp_fd, int *done, char *buffer);
int check_link(uint8_t port, uint8_t mip, struct link *links, int link_len);
void create_new_link(uint8_t port, uint8_t mip, struct link *links, int *link_len);
FILE* get_file(uint8_t port, uint8_t mip, struct link *links, int link_len);
