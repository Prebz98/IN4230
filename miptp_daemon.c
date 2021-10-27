#include "miptp_daemon.h"
#include "general.h"
#include <stdio.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char* argv[]){

    int timeout_msecs; //timeout for a message
    char path_to_mip[BUFSIZE]; //path to lower layer unix
    int mip_fd; //mip daemon fd
    char path_to_higher[BUFSIZE]; //path to higher layer unix
    int done = 0;
    int num_fds = 0;
    
    struct pollfd fds[BUFSIZE]; 
    //fds, 
    //  first space for mip_daemon
    //  second space for applications to connect 
    //  rest is for applications


    argparser(argc, argv, &timeout_msecs, path_to_mip, path_to_higher);
    mip_fd = connect_to_mip_daemon(path_to_mip, fds);
    num_fds++;
    write_identifying_msg(mip_fd);
    setup_unix_socket(path_to_higher, fds);
    
    while (!done) {
        poll(fds, num_fds, 1000);

        // goes through all fds
        for (int i=0; i<num_fds; i++){

            //mip daemon disconnected
            if (fds[i].revents & POLLHUP && i==0){
                done = 1;
                printf("MIP daemon has disconneced\n");
            }
            //an application has disconnected
            else if (fds[i].revents & POLLHUP) {
                close(fds[i].fd);
                fds[i].fd = 0;
            }
            // a new application want to connect
            else if (fds[i].revents & POLLIN && i==1) {
                fds[num_fds].fd = accept(fds[i].fd, NULL, NULL);
                fds[num_fds].events = POLLHUP | POLLIN;
                num_fds++;
            }
        }
    }
    
}