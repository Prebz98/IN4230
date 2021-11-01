#include "general.h"
#include <string.h>
#include <stdint.h>
#include "file_transfer.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

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

int main(int argc, char* argv[]){
    char path_to_file[BUFSIZE];
    char path_to_miptp[BUFSIZE];
    uint8_t dst_mip;
    uint8_t dst_port;
    uint8_t my_port;
    uint8_t buffer[BUFSIZE];
    
    argparser(argc, argv, path_to_file, path_to_miptp, &dst_mip, &dst_port);

    srand(time(NULL)); //seed random number

    int miptp_fd = connect_to_miptp(path_to_miptp);
    int approved = 0;
    int i = 0;
    while (!approved) {
        if (i > 2){
            printf("Portnumber not accepted\n");
            exit(EXIT_SUCCESS);
        }
        my_port = rand();
        write_identifying_msg(miptp_fd, my_port);
        read(miptp_fd, buffer, 2);
        approved = buffer[0];
        i++;
    }
    my_port = buffer[1];
    printf("My portnumber is %d\n", my_port);

    //temp
    memset(buffer, 0, BUFSIZE);
    struct miptp_pdu *packet = (struct miptp_pdu*)buffer;
    packet->mip = 20;
    packet->port = 2;
    strcpy(packet->sdu, "HEISANN fra A");
    int size = 2+strlen("HEISANN fra A");
    write(miptp_fd, buffer, size);
}