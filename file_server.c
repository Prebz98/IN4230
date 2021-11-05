#include "general.h"
#include <netinet/in.h>
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
            printf("Program description:\nThe program will listen on the port specified. Incoming files will be stored in a directory. The filename will indicate where the incoming file came from.\n");
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
    struct link links[MAX_LINKS];
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
        memset(buffer, 0, BUFSIZE);
        int rc = read_from_socket(miptp_fd, buffer);
        if (rc == -1){
            done = 1;
            continue;
        }
        struct app_pdu *packet = (struct app_pdu*)buffer;
        //checking if the sender is known
        int index = check_link(packet->port, packet->mip, links, link_len);

        if (index == -1){ //if not known
            uint32_t file_size = ntohl(*(uint32_t*)packet->sdu);
            create_new_link(packet->port, packet->mip, file_size, links, &link_len);
        }
        else { //sender is known
            FILE *file = get_file(packet->port, packet->mip, links, link_len);
            fprintf(file, "%s", packet->sdu);

            uint32_t packet_len = strlen(packet->sdu);
            uint32_t *bytes_left = &links[index].file_size;
            *bytes_left = *bytes_left - packet_len;
            if (*bytes_left == 0){
                fclose(file);
                printf("Transmission complete\n");
            }
        }
        
    }
    // close_files(links, link_len);
}