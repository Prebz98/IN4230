#ifndef GENERAL
#define GENERAL
#include <stdint.h>
#include <sys/types.h>

#define BUFSIZE 4096
#define LINE "\n------------------------------\n"
#define ROUTING_HELLO "HLO"
#define ROUTING_UPDATE "UPD"
#define ROUTING_REQUEST "REQ"
#define ROUTING_RESPONSE "RES"

struct unix_packet {
    uint16_t mip : 8;
    uint16_t ttl : 8;
    char msg[BUFSIZE];
}__attribute__((packed));

struct mip_hdr {
    uint8_t dst;
    uint8_t src;
    uint8_t ttl : 4;
    uint16_t sdu_len : 9;
    uint8_t sdu_type : 3;
}__attribute__((packed));

struct raw_packet {
    struct mip_hdr hdr;
    char sdu[BUFSIZE];
}__attribute__((packed));

#endif