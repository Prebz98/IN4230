#ifndef GENERAL
#define GENERAL
#include <stdint.h>
#include <sys/types.h>

#define BUFSIZE 4096
#define LINE "\n------------------------------\n"
struct unix_packet {
    uint8_t mip_dst;
    char msg[BUFSIZE];
};
#endif