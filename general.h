#ifndef GENERAL
#define GENERAL
// #include <cstdint>
#include <stdint.h>
#include <sys/types.h>

#define BUFSIZE 4096
#define LINE "\n------------------------------\n"
struct unix_packet {
    uint16_t mip : 8;
    uint16_t ttl : 8;
    char msg[BUFSIZE];
}__attribute__((packed));
#endif