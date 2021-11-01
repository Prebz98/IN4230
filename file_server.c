#include "general.h"
#include <stdint.h>
#include <string.h>
#include <bits/getopt_core.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "file_transfer.h"


int argparser(int argc, char **argv, uint8_t *port, char *path_to_miptp, char *directory){
    /*
    * parses all arguments

    * port: portnumber to listen on
    * path_to_miptp: path to miptp socket
    * directory: to save incoming files
    */
    int index;
    int c;

    opterr = 0;

    while ((c = getopt (argc, argv, "h")) != -1)
        switch (c)
        {
        case 'h':
            printf("How to run\n./file_server [-h] <portnumber> <socket lower> <directory> \nAlternative: Use the makefile commands.\n");
            printf("Optional args:\n\
            -h Help\n\
            Non-optional args:\n\
            Portnumber: the port number to listen on\n\
            Socket lower: filename for unix socket to miptp\n\
            Directory: the directory to place incoming files\n");
            printf("Program description:\nThe program will listen on the port specified. Incoming files will be stored in a directory. The file in the directory will start by indicating where the incoming file came from.\n");
            exit(EXIT_SUCCESS);
            break;
        default:
            abort ();
        }

    index = optind;
    *port = atoi(argv[index]);
    strcpy(path_to_miptp, argv[index+1]);
    strcpy(directory, argv[index+2]);
    return 0;
}

int main(int argc, char* argv[]){
    uint8_t port;
    char path_miptp[BUFSIZE];
    char path_directory[BUFSIZE];
    char buffer[BUFSIZE];
    int done = 0;
    argparser(argc, argv, &port, path_miptp, path_directory);
    struct link links[MAX_NODES];
    int link_len = 0;
    int miptp_fd = connect_to_miptp(path_miptp);
    
    write_identifying_msg(miptp_fd, port);
    read(miptp_fd, buffer, 2);
    //if port is approved
    if (!buffer[0]){
        close(miptp_fd);
        printf("Port was not avaliable\n");
        exit(EXIT_SUCCESS);
    }
    printf("Port number accepted\n");

    while (!done) {
        int rc = read_from_socket(miptp_fd, &done, buffer);
        struct miptp_pdu *packet = (struct miptp_pdu*)buffer;
        if (!check_link(packet->port, packet->mip, links, link_len)){
            create_new_link(packet->port, packet->mip, links, &link_len);
        }
        FILE *file = get_file(packet->port, packet->mip, links, link_len);
        fprintf(file, "incoming_%d_%d\n", packet->mip, packet->port);
        fprintf(file, "%s", packet->sdu);
        fclose(file);
    }
    //close files and sockets TODO
}