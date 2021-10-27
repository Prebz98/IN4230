#include "general.h"
#include <stdint.h>
#include <string.h>
#include <bits/getopt_core.h>
#include <stdio.h>
#include <stdlib.h>
#include "file_transfer.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void write_identifying_msg(int miptp_fd, uint8_t my_port){
    /*
    * write identifying message

    * mip_fd: fd to mip_daemon
    */
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    memset(buffer, my_port, 1);
    write(miptp_fd, buffer, 1);
    printf("Identify myself for miptp.\n");
}

void error(int ret, char *msg) {
    //program error
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

int connect_to_miptp(char* path_to_miptp){
    /*
    * connects to the mip daemon

    * path_to_mip: path to the unix socket to mip_daemon

    * returns: (int) the fd
    */
    struct sockaddr_un serv_addr;
    int mip_fd;
    printf("%s\n", path_to_miptp);
    if ((mip_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0){
        error(mip_fd, "socket setup failed\n");
    }
    memset(&serv_addr, 0, sizeof(struct sockaddr_un));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, path_to_miptp);
    if ((strlen(path_to_miptp)) < strlen(serv_addr.sun_path)){
        error(-1, "Path is too long");
    }
    if (0 != (connect(mip_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_un)))){
        error(-1, "Failed to connect to server");
    }

    return mip_fd;
}

int argparser(int argc, char **argv, char *path_to_file, char *path_to_miptp, uint8_t *dst_mip, uint8_t *dst_port){
    /*
    * parses all arguments

    * path_to_file: path to file to send
    * path_to_miptp: path to miptp socket
    * dst_mip: destination mip address
    * dst_port: destination port number
    */
    int index;
    int c;

    opterr = 0;

    while ((c = getopt (argc, argv, "h")) != -1)
        switch (c)
        {
        case 'h':
            printf("How to run\n./file_transfer [-h] <file> <socket lower> <mip> <port>\nAlternative: Use the makefile commands.\n");
            printf("Optional args:\n\
            -h Help\n\
            Non-optional args:\n\
            File: path to the file to send\n\
            Socket lower: filename for unix socket to miptp\n\
            MIP: MIP address to send to\n\
            Port: Port number to send to\n");
            printf("Program description:\nThe program will send a file to a file receiver. It sends 1400 byte chunks to the miptp_daemon that will ensure that the file reaches its destination in order. After sending the file, the program ends.\n");
            exit(EXIT_SUCCESS);
            break;
        default:
            abort ();
        }

    index = optind;
    strcpy(path_to_file, argv[index]);
    strcpy(path_to_miptp, argv[index+1]);
    *dst_mip = atoi(argv[index+2]);
    *dst_port = atoi(argv[index+3]);
    return 0;
}