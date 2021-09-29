#include "general.h"
#include <linux/if_packet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>
#include "mip_daemon.h"

void handle_routing_msg(struct pollfd *fds, uint8_t my_mip){
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    read(fds[3].fd, buffer, BUFSIZE);

    if (0 == strcmp(buffer, ROUTING_HELLO)){
        printf("Received hello from router\n");
        uint8_t raw_buffer[BUFSIZE];
        memset(raw_buffer, 0, BUFSIZE);

        struct mip_hdr hdr = create_mip_hdr(255, my_mip, 1, 3, 0x04);
        memcpy(raw_buffer, &hdr, sizeof(struct mip_hdr));
        memcpy(&raw_buffer[4], ROUTING_HELLO, 3);

        struct sockaddr_ll interfaces[MAX_IF];
        int number_of_if = 0;
        number_of_if = get_mac_from_interface(interfaces);
        uint8_t broadcast_mac[6] = DST_MAC_ADDR;

        for (int i = 0; i<number_of_if; i++){
            send_raw_packet(&fds[1].fd, &interfaces[i], raw_buffer, 7, broadcast_mac);
        }
        
    }
    //variabel her som er første del av bufferen
    else if (0 == strcmp(buffer[0:3], ROUTING_UPDATE)) {
        printf("Received an update\n");
    }
    printf("HERE %s\n", buffer);
    printf("%lu\n", strlen(buffer));
}

void send_to_router(char *msg, uint8_t mip_dst, int sock_server){
    write_to_unix_socket(msg, mip_dst, sock_server, 0);
}