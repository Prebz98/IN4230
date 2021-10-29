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
            char buffer[BUFSIZE];
            int rc = read(mip_daemon->fd, buffer, BUFSIZE);
            struct unix_packet *packet_received = (struct unix_packet*)buffer; 
            struct miptp_pdu *tp_pdu = (struct miptp_pdu*)packet_received->msg;
            uint8_t port = tp_pdu->port;
            int index_of_app = index_of_port(port, port_numbers, number_of_ports);
            struct pollfd app = fds[index_of_app];
            write(app.fd, tp_pdu, rc-2); // 2 bytes removed from unix_packet to miptp_pdu
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
                        char buffer_up[1];
                        read(fds[i].fd, buffer_up, 1);
                        if (available_port(port_numbers, buffer_up[0])){
                            port_numbers[i] = buffer_up[0];
                            number_of_ports++;
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