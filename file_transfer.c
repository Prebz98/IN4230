#include "general.h"
#include <stdint.h>
#include "file_transfer.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

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
        read(miptp_fd, buffer, 1);
        approved = buffer[0];
        i++;
    }
    printf("My portnumber is %d\n", my_port);
}