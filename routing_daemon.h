
#include <stdint.h>
#include <sys/un.h>

struct node {
    uint8_t mip;
    int distance;
    struct sockaddr_un next_hop;
};