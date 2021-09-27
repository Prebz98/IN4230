
#include <stdint.h>

struct node {
    struct node *next;
    uint8_t mip;
    int distance;
    // mac or interface
};