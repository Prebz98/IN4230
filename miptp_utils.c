#include "general.h"
#include "miptp_daemon.h"
#include <bits/getopt_core.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>

void error(int ret, char *msg) {
    //program error
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

int connect_to_mip_daemon(char* path_to_mip, struct pollfd *fds){
    /*
    * connects to the mip daemon

    * path_to_mip: path to the unix socket to mip_daemon
    * fds: list to save the fd

    * returns: (int) the fd
    */
    struct sockaddr_un serv_addr;
    int mip_fd;

    if ((mip_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0){
        error(mip_fd, "socket setup failed\n");
    }
    memset(&serv_addr, 0, sizeof(struct sockaddr_un));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, path_to_mip);
    if ((strlen(path_to_mip)) < strlen(serv_addr.sun_path)){
        error(-1, "Path is too long");
    }
    if (0 != (connect(mip_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_un)))){
        error(-1, "Failed to connect to server");
    }

    fds[0].fd = mip_fd;
    fds[0].events = POLLHUP | POLLIN;
    return mip_fd;
}

void write_identifying_msg(int mip_fd){
    /*
    * write identifying message

    * mip_fd: fd to mip_daemon
    */
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    uint8_t sdu_type = 0x05; //transport sdu type
    memset(buffer, sdu_type, 1);
    write(mip_fd, buffer, 1);
    printf("Identify myself for daemon.\n");
}

void forward_to_mip(int mip_daemon, int application, uint8_t app_port){
    /*
    * reads a message from the application and forwards it to mip daemon

    * mip_daemon: mip daemon fd
    * application: application fd
    */
    
    char buffer_up[BUFSIZE];
    char buffer_down[BUFSIZE];
    int rc = read(application, buffer_up, BUFSIZE);
    uint8_t dst_mip = buffer_up[0];
    uint8_t dst_port = buffer_up[1];
    memset(buffer_down, 0, BUFSIZE);

    //32 bit align
    uint8_t size = 6 + rc; //6 bytes of header, 4miptp + 2mip
    uint8_t rest = size % 4;
    size += rest ? 4-rest : 0;

    struct unix_packet *packet = (struct unix_packet*)buffer_down;
    struct miptp_pdu *miptp_pdu = (struct miptp_pdu*)packet->msg;

    miptp_pdu->dst_port = buffer_up[1];
    miptp_pdu->seq = 1; //TODO
    miptp_pdu->src_port = app_port;
    memcpy(miptp_pdu->sdu, buffer_up, rc);

    packet->mip = dst_mip;
    packet->ttl = 0; //default 
    memcpy(packet->msg, miptp_pdu, size); 

    write(mip_daemon, buffer_down, size);
}

int index_of_port(uint8_t port, uint8_t *port_numbers, int number_of_ports){
    /*
    * returns the index of the portnumber in portnumbers

    * port: the port to look for
    * port_numbers: all active port numbers
    * number_of_ports: the length of port_numbers

    * returns (int) the index of the port, -1 if not found
    */

    for (int i=0; i<number_of_ports; i++){
        if (port_numbers[i] == port){
            return i;
        }
    }
    return -1;
}

void send_port_response(int fd, uint8_t port, int approved){
    /*
    * sends a response that the portnumber was accepted or denied

    * fd: fd to send to
    * port: portnumber that was approved
    * approved: 1 if approved, 0 if denied
    */
    uint8_t buffer[BUFSIZE];
    memcpy(buffer, &approved, 1);
    memcpy(&buffer[1], &port, 1);

    write(fd, buffer, 2);
}

int available_port(uint8_t *port_numbers, uint8_t port){
    /*
    * checks if a port is available

    * port_numbers: all port numbers in use
    * port: port number to check

    * returns (int) 1 if available, 0 if not
    */
    for (int i=0; i<MAX_NODES; i++){
        if (port_numbers[i] == port){
            return 0;
        }
    }
    return 1;
}

void setup_unix_socket(char *path, struct pollfd *fds) {
    /* 
    * setting up unix socket fro applications to connect
    * path: path to unix socket fd
    * fds: all fds
    */

    int sock_server;
    struct sockaddr_un serv_addr;
    if ((sock_server = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
        error(sock_server, "socket");
    }
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, path);
    if ((strlen(path) < strlen(serv_addr.sun_path))){
        error(-1, "Path is too long\n");
    }
    unlink(serv_addr.sun_path);

    //bind the socket
    if (bind(sock_server, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_un)) < 0) {
        error(-1, "bind");
    }

    // listen to the socket for applications to connect
    listen(sock_server, 1);
    fds[1].fd = sock_server;
    fds[1].events = POLLHUP | POLLIN;
}


int argparser(int argc, char **argv, int *timeout_msecs, char* mip_daemon, char* path_to_higher) {
    /*
    * parses all arguments

    * timeout_msecs: how long the MIPTP will wait before a packet is considered lost
    * mip_daemon: path to the lower layer unix socket
    * path_to_higher: path to the upper layer unix socket
    */
    int index;
    int c;

    opterr = 0;

    while ((c = getopt (argc, argv, "h")) != -1)
        switch (c)
        {
        case 'h':
            printf("How to run\n./miptp_daemon [-h] <timeout> <socket lower> <socket upper>\nAlternative: Use the makefile commands.\n");
            printf("Optional args:\n\
            -h Help\n\
            Non-optional args:\n\
            Timeout: timeout for go back N in msecs\
            Socket lower: filename for unix socket to lower layer\n\n");
            printf("Program description:\nThe program will work as a transport layer over the mip network layer. It will send packets received by the application down to the mip daemon. And it will send packet received by the mip daemon up to the application. The MIPTP daemon will reply with acks to ensure reliability and in-order data transmission.\n");
            exit(EXIT_SUCCESS);
            break;
        default:
            abort ();
        }

    index = optind;
    *timeout_msecs = atoi(argv[index]);
    strcpy(mip_daemon, argv[index+1]);
    strcpy(path_to_higher, argv[index+2]);
    return 0;
}