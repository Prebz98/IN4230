#include "mip_daemon.h"
#include <bits/getopt_core.h>
#include <bits/types/struct_iovec.h>
#include <bits/types/struct_timeval.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include <poll.h>
#include "general.h"
#include <ifaddrs.h>
#include <linux/if_packet.h>
#include "general.h"

bool debug_mode;
struct cache cache_table[CACHE_TABLE_LEN];
int cache_current_len = 0;
// struct raw_packet waiting_message;
// int waiting_sdu_len = 0;
struct waiting_queue *routing_queue;
struct waiting_queue *arp_queue;

void argparser(int argc, char **argv, char* path, uint8_t *mip_addr) {
    /*
    * parses all arguments
    * argc: number of system arguments given
    * argv: system arguments
    * path: path to unix socket fd
    * mip_addr: mip address to connected host
    * 
    * global:
    * debug_mode is affected
    * termination is affected
    */
    int index;
    int c;
    debug_mode = false;

    opterr = 0;

    while ((c = getopt (argc, argv, "hd")) != -1)
        switch (c)
        {
        case 'h':
            printf("How to run\n./mip_daemon [-h] [-d] <socket_upper> <MIP address>\nAlternative: Use the makefile commands.\nMake all to compilate all files, then runDaemonA to run a daemon for node A. Likewise runDaemonB for B and runDaemonC for C\n");
            printf("Optional args:\n\
            -h Help\n\
            -d Debug mode\n\
            Non-optional args:\n\
            socket_upper: filename for unix socket to upper layer\n\
            MIP address: MIP address for the connected application.\n\n");
            printf("Program description:\nThe mip-daemon will open a unix socket for a ping_client or a ping_server to connect, as well as a unix socket for a routing daemon.\nIf it receives a ping-message from the PING-host, it will ask send a request to the routing daemon to find the best path.\nThe MIP-daemon forwards all traffic to the targets sat by the routing daemon. \nIf there is no activity for 60 seconds, the program stops.\n");
            exit(EXIT_SUCCESS);
            break;
        case 'd':
            debug_mode = true;
            break;
        default:
            abort ();
        }

    index = optind;
    strcpy(path, argv[index]);
    *mip_addr = atoi(argv[index+1]);
    return;
}

void clear_memory(int *sock_server, struct sockaddr_un *serv_addr){
    /*
    * clearing memory and closing sockets
    * sock_server: fd to the unix socket
    * serv_addr: socket address to unix socket
    */
    close(*sock_server);
    free(serv_addr);
}

void error(int ret, char *msg) {
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

void setup_unix_socket(char *path, int *sock_server, struct sockaddr_un *serv_addr) {
    /* 
    * setting up unix socket
    * path: path to unix socket fd
    * sock_server: will be fd to unix socket
    * serv_addr: socket address to unix socket 
    */
    if ((*sock_server = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
        error(*sock_server, "socket");
    }
    serv_addr->sun_family = AF_UNIX;
    strcpy(serv_addr->sun_path, path);
    if ((strlen(path) < strlen(serv_addr->sun_path))){
        error(-1, "Path is too long\n");
    }
    unlink(serv_addr->sun_path);

    //bind the socket
    if (bind(*sock_server, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_un)) < 0) {
        error(-1, "bind");
    }

    return;
}

void listen_unix_socket(int *sock_server, struct pollfd *fds){
    /* 
    * listening to unix socket and store it in fds
    * sock_server: fd to unix socket
    * fds: array to save fds
    */
    listen(*sock_server, 1);
    fds[0].fd = *sock_server;
    fds[0].events = POLLHUP | POLLIN;
}

void write_to_unix_socket(char *msg, uint8_t msg_size, uint8_t mip_dst, int sock_server, uint8_t ttl){
    /*
    * writing message to host with unix socket
    * msg: message to be sent
    * mip_dst: mip address to send to
    * sock_server: socket fd to send through
    */
    char buffer[BUFSIZE];
    memset(buffer, 0, sizeof(buffer));
    struct unix_packet *up = (struct unix_packet*)buffer;
    up->mip = mip_dst;
    up->ttl = ttl;
    memcpy(up->msg, msg, msg_size);
    if (sock_server == 0){
        printf("Routing daemon not connected. Message ignored.\n");
        return;
    }

    //the msgsize already has the size of the message + the mip and ttl
    int total_size = 2+msg_size;
    int rest = total_size % 4;
    total_size += rest ? 4-rest : 0;

    write(sock_server, buffer, total_size);
}

int check_cache(uint8_t mip){
    /*
    * checking if mip is in cache
    * current solution does not respect delete-functions in cache table
    * mip: mip to look up
    *
    * returns: index in cache table, -1 if not found
    */
    for (int i = 0; i < cache_current_len; i++){
        if (cache_table[i].mip == mip){
            if (debug_mode){
                printf("Found MIP-address %d in cache table\n", mip);
            }
            return i;
        }
    }
    if (debug_mode){
        printf("MIP-address %d not found in cache table\n", mip);
    }
    return -1;
}

struct arp_sdu create_arp_sdu(uint8_t mip, bool request){
    /*
    * creates the sdu of an arp request
    * mip: mip address destination
    * request: true if request, false if response

    * returns: the sdu to be sent with the arp 
    */
    struct arp_sdu arp_req;
    arp_req.address = mip;
    if (request){
        arp_req.type = 0x00;
    }else { //response
    arp_req.type = 0x01;
    }
    arp_req.padding = 0;
    return arp_req;
}

void get_mac_from_index(int index, uint8_t mac[6]){
    /*
    * gets mac based on interface index
    * index: index of mac to find
    * mac: buffer to store mac address
    */
    struct ifaddrs *ifaces, *ifp;
    if (getifaddrs(&ifaces)) {
        perror("getifaddrs");
        exit(-1);
    }
    int counter = 1;

    //go to interface with right index
    for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next){
        if (counter == index){
            struct sockaddr_ll *ll = (struct sockaddr_ll*)ifp->ifa_addr;
            memcpy(mac, ll->sll_addr, 6);
            freeifaddrs(ifaces);
            return;
        }
        counter++;
    }
    freeifaddrs(ifaces);
    error(-1, "Index bigger than number of ifaces");
}

int get_mac_from_interface(struct sockaddr_ll *senders_iface){
    /*
    * gets all interfaces where the sa_family is AF_PACKET and it is not of type loopback.
    * senders_iface: buffer to store interfaces 

    * returns: number of interfaces stored in senders_iface
    */
    struct ifaddrs *ifaces, *ifp;
    if (getifaddrs(&ifaces)) {
        perror("getifaddrs");
        exit(-1);
    }
    int num_of_if = 0;

    memset(senders_iface, 0, sizeof(struct sockaddr_ll)*MAX_IF);
    //counting number og local interfaces
    for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next){
        if (ifp->ifa_addr != NULL &&
            ifp->ifa_addr->sa_family == AF_PACKET &&
            (strcmp("lo", ifp->ifa_name))){
                memcpy(&senders_iface[num_of_if], (struct sockaddr_ll*)ifp->ifa_addr, sizeof(struct sockaddr_ll));
            num_of_if++;
        }
    }
    freeifaddrs(ifaces);
    return num_of_if;
}

void setup_raw_socket(int *raw_sock, struct pollfd *fds){
    /* 
    * create the raw socket
    * raw_sock: buffer to set fd
    * fds: array to store fds
    */
    *raw_sock = socket(AF_PACKET, SOCK_RAW, htons(0x88B5));
    if (*raw_sock == -1){
        error(*raw_sock, "Raw socket");
    }
    fds[1].fd = *raw_sock;
    fds[1].events = POLLIN | POLLHUP;
    return;
}

int send_raw_packet(int *raw_sock, struct sockaddr_ll *so_name, uint8_t *buf, size_t len, uint8_t dst_mac[6]){
    /*
    * send mip packets through the raw socket
    * raw_sock: socket fd
    * so_name: socket address
    * buf: content to send
    * len: length of buffer message
    * dst_mac: destination mac address

    returns: number of bytes sent, -1 if error
    */
    int rc;
    struct ether_frame frame_hdr;
    struct msghdr msg[1];
    memset(msg, 0, sizeof(struct msghdr));
    struct iovec msgvec[2]; 

    // fill ethernet header
    memcpy(frame_hdr.dst_addr, dst_mac, 6);

    int index = so_name->sll_ifindex;
    uint8_t src[6];
    memset(src, 0, 6);
    get_mac_from_index(index, src);
    memcpy(frame_hdr.src_addr, src, 6);

    frame_hdr.eth_protocol[0] = (htons(0x88B5) >> (8*0)) & 0xff;
    frame_hdr.eth_protocol[1] = (htons(0x88B5) >> (8*1)) & 0xff;

    //point to frame header
    msgvec[0].iov_base = &frame_hdr;
    msgvec[0].iov_len = sizeof(struct ether_frame);

    //point to frame payload
    msgvec[1].iov_base = buf;
    msgvec[1].iov_len = len;

    //fill out message metadata struct

    memcpy(so_name->sll_addr, dst_mac, 6);
    
    msg->msg_name = so_name;
    msg->msg_namelen = sizeof(struct sockaddr_ll);
    msg->msg_iovlen = 2;
    msg->msg_iov = msgvec;

    //construct message
    rc = sendmsg(*raw_sock, msg, 0);
    if (rc == -1){
        error(rc, "sendmsg");
    }

    struct mip_hdr *hdr = (struct mip_hdr*)buf;
    if (debug_mode){
        printf("\nMIP packet sent\n dst: %d\n src: %d\n ttl:   %d\n sdu length: %d\n sdu-type: %d\n", hdr->dst, hdr->src, hdr->ttl, hdr->sdu_len, hdr->sdu_type);
        printf(" dst-mac ");
        print_mac(frame_hdr.dst_addr);
        printf(" src-mac ");
        print_mac(frame_hdr.src_addr);
        print_cache();
    }
    return rc;
}

void cache(uint8_t mip, struct sockaddr_ll s_addr, uint8_t mac[6]){
    /*
    * map sockaddr to mip
    * mip: mip address
    * s_addr: the interface to send through
    * mac: mac address
    */
    int index = CACHE_TABLE_LEN - cache_current_len;
    if (CACHE_TABLE_LEN - cache_current_len > 0){
        struct cache insert;
        insert.iface = s_addr;
        insert.mip = mip;
        memcpy(insert.mac, mac, 6);
        cache_table[cache_current_len] = insert;
        cache_current_len++;
        return;
    }
    error(-1, "Not enough space in cache\n");
    return;
}


int recv_raw_packet(int raw_sock, uint8_t *buf, size_t len, struct sockaddr_ll *senders_iface){
    /*
    * recieve mip packets through the raw socket
    * raw_sock: socket fd
    * buf: buffer to store content
    * len: length of buffer
    * senders_iface: interface to receive from

    * returns: number of bytes received
    */
    struct ether_frame frame_hdr;
    struct msghdr msg;
    struct iovec msgvec[2];
    int rc;
    memset(&msg, 0, sizeof(struct msghdr));
    memset(senders_iface, 0, sizeof(struct sockaddr_ll));

    //point to frame header
    msgvec[0].iov_base = &frame_hdr;
    msgvec[0].iov_len = sizeof(struct ether_frame);
    //point to frame payload
    msgvec[1].iov_base = buf;
    msgvec[1].iov_len = len;

    //fill metadata struct
    msg.msg_name = senders_iface;
    msg.msg_namelen = sizeof(struct sockaddr_ll);
    msg.msg_iovlen = 2;
    msg.msg_iov = msgvec;

    rc = recvmsg(raw_sock, &msg, 0);
    if (rc == -1){
        error(-1, "recvmsg");
    }

    struct mip_hdr *hdr = (struct mip_hdr*)buf;
    if (debug_mode){
        printf("\nMIP packet received\nMIP header\n dst: %d\n src: %d\n ttl:   %d\n sdu length: %d\n sdu-type: %d\n", hdr->dst, hdr->src, hdr->ttl, hdr->sdu_len, hdr->sdu_type);
        printf(" dst-mac ");
        print_mac(frame_hdr.dst_addr);
        printf(" src-mac ");
        print_mac(frame_hdr.src_addr);
        print_cache();
    }
    int index = 0;
    if ((index = check_cache(hdr->src)) == -1)
        cache(hdr->src, *senders_iface, frame_hdr.src_addr);
    return rc;
}

struct mip_hdr create_mip_hdr(uint8_t dst, uint8_t src, uint8_t ttl, uint16_t sdu_len, uint8_t sdu_type){
    /*
    * creating header for mip packets
    * dst: mip destination
    * src: mip source
    * ttl: time to live
    * sdu_len: length of sdu
    * sdu_type: type of sdu, (ping, ARP, MIPTP)
    */
    struct mip_hdr hdr;
    hdr.dst = dst;
    hdr.src = src;
    hdr.ttl = ttl;
    hdr.sdu_len = sdu_len;
    hdr.sdu_type = sdu_type;
    return hdr;
}

void print_mac(uint8_t *mac){
    /*
    * printing a mac address
    * mac: mac address to be printed
    */
    printf("[");
    for (int i=0; i<5; i++){
        printf("%d:", *(mac+(sizeof(uint8_t)*i)));
    }   
    printf("%d]\n", *(mac+(sizeof(uint8_t)*5)));
}

void print_cache(){
    /*
    * printing the cache
    * dependent of cache_table
    */
    printf("In cache:\n");
    for (int i=0; i<cache_current_len; i++){
        printf("\tMIP: %d - MAC: ", cache_table[i].mip);
        print_mac(cache_table[i].iface.sll_addr);
    }
    printf("\n");
}

void handle_new_client(struct pollfd *pending_connections, int client_fd){
    /*
    * sets the pending connection into the last part of fds, wich is called pending_connections
    
    * pending_connections: the last part of fds, reserved for pending connections
    * client_fd: the new connection fd
    */
    for (int y=0; y<MAX_EVENTS; y++){
        if (pending_connections[y].fd == 0){
            pending_connections[y].fd = client_fd;
            pending_connections[y].events = POLLIN | POLLHUP;
            break;
        }
    }
}

void identify_unix_client(struct pollfd *fds, int index){
    /*
    * puts the new client on the appropriate position in the fds. 
    * The client can be a PING host or a routing daemon. 
    
    * fds: list of filedescriptors
    * index: index of the new client in fds 
    */
    char buffer[BUFSIZE];
    read(fds[index].fd, buffer, 1);
    uint8_t identifier = buffer[0];
    
    // a ping host
    if (identifier == 0x02){
        // moving the fd to the right spot
        memcpy(&fds[2], &fds[index], sizeof(struct pollfd));
        memset(&fds[index], 0, sizeof(struct pollfd));
    }
    // the routing daemon
    else if (identifier == 0x04) {
        // moving the fd to the right spot
        memcpy(&fds[3], &fds[index], sizeof(struct pollfd));
        memset(&fds[index], 0, sizeof(struct pollfd));
    }
    // MIPTP
    else if (identifier == 0x05) {
        // moving the fd to the right spot
        memcpy(&fds[4], &fds[index], sizeof(struct pollfd));
        memset(&fds[index], 0, sizeof(struct pollfd));
    }
}

void poll_loop(struct pollfd *fds, int timeout_msecs, int sock_server, uint8_t mip_addr){
    /*
    * the flow of the program
    * looking for events until termination
    * acts different for different events
    *
    * fds: all fds, including raw, and unix-server
    * timeout_msecs: timeout for polling
    * sock_server: fd for unix socket
    * mip_addr: mip address for host connected to this

    * global:
    * cache_table, waiting_massage, termination
    */
    char buffer[BUFSIZE];
    uint8_t raw_buffer[BUFSIZE];
    uint8_t send_raw_buffer[BUFSIZE];
    int res, sock_client;
    struct sockaddr_ll senders_iface[MAX_IF];
    struct pollfd *pending_connections = &fds[MAX_EVENTS];
    struct timeval start, now;
    gettimeofday(&start, NULL);

    //the program stops after polling for timeout_msecs of inactivity
    // or the program has been running for more than 60 seconds
    while ((res = poll(fds, MAX_EVENTS+NUMBER_OF_UNIX_CONNECTIONS, timeout_msecs)) > 0){
        gettimeofday(&now, NULL);
        if ((now.tv_sec-start.tv_sec) > 60){
            printf("Terminated. Has run over 60 seconds\n");
            return;
        }
        if (debug_mode){
            printf(LINE);
            printf("%d events found\n", res);
            }
        for (int i=0; i<MAX_EVENTS+NUMBER_OF_UNIX_CONNECTIONS; i++){

            // client has disconnected
            if (fds[i].revents & POLLHUP){
                printf("Client has disconnected\n");
                close(fds[i].fd);
                fds[i].fd = 0;
            }
            //new client wants to connect unix
            else if (i == 0 && fds[i].revents & POLLIN){
                if (debug_mode){
                    printf("New client wants to connect\n");
                }
                sock_client = accept(sock_server, NULL, NULL);
                if (sock_client < 0) {
                    printf("DEAMON: Failed to accept unix-socket\n");
                }else{
                    if (debug_mode){
                        printf("Client accepted\n");
                    }
                    handle_new_client(pending_connections, sock_client);
                }
            //new message in raw
            } else if (i == 1 && fds[i].revents & POLLIN) {
                memset(raw_buffer, 0, BUFSIZE);
                int rc;
                rc = recv_raw_packet(fds[1].fd, raw_buffer, BUFSIZE, senders_iface);
                if (rc == -1){
                    error(rc, "read from raw socket");
                }
                int size = rc;
                struct mip_hdr *hdr = (struct mip_hdr*)raw_buffer;

                // check if its ping, arp, routing or MIPTP
                // 0x01 -> MIP-ARP
                // 0x02 -> Ping 
                // 0x04 -> Routing 
                // 0x05 -> MIPTP
                if (hdr->sdu_type == 0x01){
                    struct arp_sdu *sdu = (struct arp_sdu*)&raw_buffer[4];
                    // check if the arp is request or response
                    // 0x00 -> request, 0x01 -> response
                    if (sdu->type == 0x00){ //its a request
                        if (sdu->address == mip_addr){
                            if (debug_mode){
                                printf("The arp message is for me!\n");
                                printf("Caching the mac\n");
                            }
                            //create arp packet
                            struct arp_sdu arp_res_sdu = create_arp_sdu(mip_addr, false);
                            struct mip_hdr mip_hdr = create_mip_hdr(hdr->src, mip_addr, 1, sizeof(struct arp_sdu), 0x01);
                            memset(send_raw_buffer, 0, BUFSIZE);
                            memcpy(send_raw_buffer, &mip_hdr, sizeof(struct mip_hdr));
                            memcpy(&send_raw_buffer[4], &arp_res_sdu, sizeof(struct arp_sdu));
                            
                            //sending packet
                            uint8_t index = check_cache(hdr->src);
                            send_raw_packet(&fds[1].fd, senders_iface, send_raw_buffer, 8, cache_table[index].mac);
                            if(debug_mode){
                                printf("Response packet sent with dst: %d\n", mip_hdr.dst);
                            }
                        }
                    }else if (sdu->type == 0x01) { // its a response
                        if (debug_mode){
                            printf("It was an ARP responce from %d\n",hdr->src);
                        }
                        //sending all messages to found mip
                        send_arp_queue(arp_queue, sdu->address, fds, cache_table);
                    }
                    //its a ping message for me
                }else if (hdr->sdu_type == 0x02 && hdr->dst == mip_addr) {
                    printf("I have received a PING message!\n");
                    int index = sizeof(struct mip_hdr);
                    char *translation = (char*)raw_buffer; 
                    int total_size = hdr->sdu_len*4;
                    struct unix_packet up;
                    memset(&up, 0, sizeof(struct unix_packet));
                    up.mip = hdr->src;
                    up.ttl = 0;
                    memcpy(up.msg, &translation[index], total_size);
                    write(fds[2].fd, &up, total_size);
                    if (debug_mode){
                        printf("\nSent message to client with unix socket\nMessage: %s\n", &translation[index]);
                    }
                } 
                //its a MIPTP message for me
                else if (hdr->sdu_type == 0x05 && hdr->dst == mip_addr) {
                    if (debug_mode){
                        printf("I have received a MIPTP message!\n");
                    }
                    int index = sizeof(struct mip_hdr);
                    char *translation = (char*)raw_buffer; 
                    int total_size = hdr->sdu_len*4;
                    struct unix_packet up;
                    memset(&up, 0, sizeof(struct unix_packet));
                    up.mip = hdr->src;
                    up.ttl = 0;
                    memcpy(up.msg, &translation[index], total_size);
                    write(fds[4].fd, &up, total_size);
                    if (debug_mode){
                        printf("\nSent message to MIPTP with unix socket\nMessage: %s\n", &translation[index]);
                    }
                }
                //its not for me, I have to pass it on
                else if ((hdr->sdu_type == 0x02 || hdr->sdu_type == 0x05) && hdr->dst != mip_addr) {
                    //ttl --
                    if (hdr->ttl == 0){continue;}
                    hdr->ttl--;
                    if (hdr->ttl > 0 || hdr->ttl != 15){
                        // saving waiting message
                        struct raw_packet packet_to_save;
                        memcpy(&packet_to_save, raw_buffer, size);
                        add_to_waiting_queue(routing_queue, packet_to_save);

                        send_req_to_router(mip_addr, hdr->dst, fds[3].fd);
                    }
                }
                // its a routing message
                else if (hdr->sdu_type == 0x04) {
                    char *translation = (char*)&raw_buffer[sizeof(struct mip_hdr)];
                    write_to_unix_socket(translation, hdr->sdu_len*4, hdr->src, fds[3].fd, 0);
                }
            }
            // client wants to identify
            else if (fds[i].revents & POLLIN && (i >= MAX_EVENTS)) {
                identify_unix_client(fds, i);
            }
            // routing daemon has sent a message
            else if (fds[i].revents & POLLIN && (i==3)) {
                handle_routing_msg(fds, mip_addr, cache_table, routing_queue, arp_queue);
            }
            // an upper client has sent a message
            else if (fds[i].revents & POLLIN){
                // read from socket 
                uint8_t mac[6];
                uint8_t mip_dst;
                int index;
                memset(buffer, 0, sizeof(buffer));
                int rc;
                rc = read(fds[i].fd, buffer, sizeof(buffer));
                if (rc == -1){
                    error(rc, "read from unix socket");
                }
                int size = rc;
                struct unix_packet *up = (struct unix_packet*)buffer;
                mip_dst = up->mip;
                if (debug_mode){
                    printf("Client has sent a message\n");
                    printf("DST MIP address: %d\n", mip_dst);
                    printf("Message: %s\n", up->msg);
                }

                //send a request to the routing daemon
                send_req_to_router(mip_addr, mip_dst, fds[3].fd);
                if (debug_mode){
                    printf("Sent a request to router\n");
                }
                // saving waiting message
                struct raw_packet packet_to_save;

                // default ttl
                if (up->ttl == 0){
                    up->ttl = TTL;
                }

                uint8_t protocol;
                switch (i) {
                    case 2:
                        protocol = 0x02; //ping
                    case 4:
                        protocol = 0x05; //miptp
                }
                
                //creating header
                struct mip_hdr hdr_to_save;
                hdr_to_save.dst = mip_dst;
                hdr_to_save.src = mip_addr;
                hdr_to_save.ttl = up->ttl;
                hdr_to_save.sdu_type = protocol;
                hdr_to_save.sdu_len = size/4;
                packet_to_save.hdr = hdr_to_save;

                //creating message
                memcpy(packet_to_save.sdu, up->msg, size);
                add_to_waiting_queue(routing_queue, packet_to_save);
                if (debug_mode){
                    printf("Will send a message!\n");
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    printf("\n\n\n");
    printf(LINE);

    //declare variables
    struct sockaddr_un serv_addr[1];
    int sock_server=0, raw_sock = 0;
    struct pollfd fds[MAX_EVENTS + NUMBER_OF_UNIX_CONNECTIONS];
    int timeout_msecs; 
    short unsigned int protocol_raw;
    struct sockaddr_ll so_names[MAX_IF];
    uint8_t mip_addr;
    char path[BUFSIZE];
    memset(path, 0, BUFSIZE);
    memset(fds, 0, sizeof(struct pollfd)*MAX_EVENTS);

    // initial values
    protocol_raw = 0x88B5;
    timeout_msecs = 10000;
    memset(fds, 0, sizeof(fds));
    memset(cache_table, 0, sizeof(cache_table));
    argparser(argc, argv, path, &mip_addr);
    routing_queue = malloc(sizeof(struct waiting_queue));
    routing_queue->next = NULL;
    arp_queue = malloc(sizeof(struct waiting_queue));
    arp_queue->next = NULL;

    //main flow
    setup_unix_socket(path, &sock_server, serv_addr);
    listen_unix_socket(&sock_server, fds);
    setup_raw_socket(&raw_sock, fds);
    poll_loop(fds, timeout_msecs, sock_server, mip_addr);
    
    // close socket 
    close(sock_server);
    close(raw_sock);
    free_waiting_queue(routing_queue);
    free_waiting_queue(arp_queue);
    return 0;
}