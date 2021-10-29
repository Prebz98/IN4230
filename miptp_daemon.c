#include "miptp_daemon.h"
#include "general.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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
    char buffer_up[BUFSIZE];
    char buffer_down[BUFSIZE];
    
    uint8_t port_numbers[MAX_NODES];
    memset(port_numbers, 0, MAX_NODES);
    struct pollfd fds[MAX_NODES]; 
    //fds, 
    //  first space for mip_daemon
    //  second space for applications to connect 
    //  rest is for applications
    struct pollfd *mip_daemon = &fds[0];
    struct pollfd *connection_socket = &fds[1];


    argparser(argc, argv, &timeout_msecs, path_to_mip, path_to_higher);
    mip_fd = connect_to_mip_daemon(path_to_mip, fds);
    num_fds++;
    write_identifying_msg(mip_fd);
    setup_unix_socket(path_to_higher, fds);
    num_fds++;
    
    while (!done) {
        poll(fds, num_fds, 1000);

        //mip daemon disconnected
        if (mip_daemon->revents & POLLHUP){
            done = 1;
            printf("MIP daemon has disconneced\n");
        }

        // a new application want to connect
        else if ((connection_socket->revents & POLLIN)) {
            fds[num_fds].fd = accept(connection_socket->fd, NULL, NULL);
            fds[num_fds].events = POLLHUP | POLLIN;
            num_fds++;
            printf("New application connected\n");
        }

        // the mip daemon has sent a message
        else if (mip_daemon->revents & POLLIN) {
            //TODO handle this
            printf("MIP-daemon sent message - TODO\n");
        }

        else {
            // goes through all applications, skips mip daemon and connection socket
            for (int i=2; i<num_fds; i++){
                
                //an application has disconnected
                if (fds[i].revents & POLLHUP) {
                    close(fds[i].fd);
                    fds[i].fd = 0;
                }

                // an applicatin has sent a message
                else if (fds[i].revents & POLLIN) {
                    printf("Application sent message\n");
                    //check if it has a port-number
                    if (port_numbers[i] == 0){
                        read(fds[i].fd, buffer_up, 1);
                        if (available_port(port_numbers, buffer_up[0])){
                            port_numbers[i] = buffer_up[0];
                            send_port_response(fds[i].fd, port_numbers[i], APPROVED);
                        }else {
                            send_port_response(fds[i].fd, port_numbers[i], DENIED);
                        }
                    }
                    else { //portnumber is known, this is a message to pass on
                        forward_to_mip(mip_daemon->fd, fds[i].fd);   
                    }
                }
            }
        }
    }
    
}