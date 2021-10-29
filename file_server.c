#include "general.h"
#include <stdint.h>
#include <string.h>
#include <bits/getopt_core.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void error(int ret, char *msg) {
    //program error
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}


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
    
}