#include "miptp_daemon.h"
#include "general.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char* argv[]){

    printf("\n\n\n");
    printf(LINE);
    printf("\n\n");

    int timeout_msecs; //timeout for a message
    char path_to_mip[BUFSIZE]; //path to lower layer unix
    int mip_fd; //mip daemon fd
    char path_to_higher[BUFSIZE]; //path to higher layer unix
    int done = 0;
    int num_fds = 0;
    // char buffer_up[BUFSIZE];
    // char buffer_down[BUFSIZE];
    
    uint8_t port_numbers[MAX_NODES];
    int number_of_ports = 0;
    memset(port_numbers, 0, MAX_NODES);
    struct pollfd fds[MAX_NODES]; 
    //fds, 
    //  first space for mip_daemon
    //  second space for applications to connect 
    //  rest is for applications
    struct pollfd *mip_daemon = &fds[0];
    struct pollfd *connection_socket = &fds[1];
    struct pollfd *applications = &fds[2];


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
        // forward it to the right app
        else if (mip_daemon->revents & POLLIN) {
            forward_to_app(mip_daemon, port_numbers, number_of_ports, applications);
        }

        else {
            // goes through all applications, skips mip daemon and connection socket
            for (int i=0; i<num_fds-2; i++){
                
                //an application has disconnected
                if (applications[i].revents & POLLHUP) {
                    close(applications[i].fd);
                    applications[i].fd = 0;
                }

                // an applicatin has sent a message
                else if (applications[i].revents & POLLIN) {
                    //check if it has a port-number
                    if (port_numbers[i] == 0){
                        char buffer_up[1];
                        read(applications[i].fd, buffer_up, 1);
                        if (available_port(port_numbers, buffer_up[0])){ 
                            port_numbers[i] = buffer_up[0];
                            number_of_ports++;
                            send_port_response(applications[i].fd, port_numbers[i], APPROVED);
                        }else {
                            send_port_response(applications[i].fd, port_numbers[i], DENIED);
                        }
                    }
                    else { //portnumber is known, this is a message to pass on
                        forward_to_mip(mip_daemon->fd, applications[i].fd, port_numbers[i]);   
                    }
                }
            }
        }
    }
    
}