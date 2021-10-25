#include "miptp_daemon.h"
#include "general.h"

int main(int argc, char* argv[]){

    int timeout_msecs; //timeout for a message
    char path_to_mip[BUFSIZE]; //path to lower layer unix
    int mip_fd;
    char path_to_higher[BUFSIZE]; //path to higher layer unix
    int applications[BUFSIZE]; //application fds

    argparser(argc, argv, &timeout_msecs, path_to_mip, path_to_higher);
    connect_to_mip_daemon(path_to_mip, &mip_fd);
}