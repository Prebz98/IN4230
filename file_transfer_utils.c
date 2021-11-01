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

int read_from_socket(int miptp_fd, int *done, char *buffer){
    /*
    * Listens to the socket until it receives a message
    * if something goes wrong or the server disconnects, return -1

    * miptp_fd: miptp fd
    * done: changes this to 1 if server disconnects
    * buffer: the buffer to store incoming packet

    * returns: (int) number of bytes received
    */
    int rc;
    if ((rc = read(miptp_fd, buffer, sizeof(buffer))) == -1 || rc == 0){
        *done = 1;
        return -1;
    }
    return rc;
}

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
