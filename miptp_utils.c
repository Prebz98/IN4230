#include "general.h"
#include "miptp_daemon.h"
#include <bits/getopt_core.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>

void error(int ret, char *msg) {
    //program error
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

void connect_to_mip_daemon(char* path_to_mip, int *mip_fd){
    struct sockaddr_un serv_addr; 

    if ((*mip_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0){
        error(*mip_fd, "socket setup failed\n");
    }
    memset(&serv_addr, 0, sizeof(struct sockaddr_un));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, path_to_mip);
    if ((strlen(path_to_mip)) < strlen(serv_addr.sun_path)){
        error(-1, "Path is too long");
    }
    if (0 != (connect(*mip_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_un)))){
        error(-1, "Failed to connect to server");
    }
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