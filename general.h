#ifndef GENERAL
#define GENERAL
#include <stdint.h>
#include <sys/types.h>

#define BUFSIZE 4096
#define LINE "\n------------------------------\n"
#define ROUTING_HELLO "HLO"
#define ROUTING_UPDATE "UPD"

struct unix_packet {
    uint16_t mip : 8;
    uint16_t ttl : 8;
    char msg[BUFSIZE];
}__attribute__((packed));
#endif