#include "miptp_daemon.h"
#include "general.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
    
    struct host hosts[MAX_LINKS]; //list with application hosts
    int num_hosts = 0;

    struct pollfd fds[MAX_NODES]; 
    //fds, 
    //  first space for mip_daemon
    //  second space for applications to connect 
    //  rest is for applications
    struct pollfd *mip_daemon = &fds[0];
    struct pollfd *connection_socket = &fds[1];
    struct pollfd *app_fds = &fds[2];


    argparser(argc, argv, &timeout_msecs, path_to_mip, path_to_higher);
    mip_fd = connect_to_mip_daemon(path_to_mip, fds);
    write_identifying_msg(mip_fd);
    setup_unix_socket(path_to_higher, fds);
    
    while (!done) {
        poll(fds, num_hosts+2, 1000); //+2 for mip_daemon and connection point

        //mip daemon disconnected
        if (mip_daemon->revents & POLLHUP){
            done = 1;
            printf("MIP daemon has disconneced\n");
        }

        // a new application want to connect
        else if ((connection_socket->revents & POLLIN)) {
            app_fds[num_hosts].fd = accept(connection_socket->fd, NULL, NULL);
            app_fds[num_hosts].events = POLLHUP | POLLIN;

            struct host new_host;
            new_host.fd_index = num_hosts;
            new_host.seq = 0; //todo

            struct message_node *new_node = malloc(sizeof(struct message_node));
            new_node->next = NULL;
            new_host.message_queue = new_node;
            hosts[num_hosts] = new_host;
            num_hosts++;
            printf("New application connected\n");
        }

        // the mip daemon has sent a message
        // forward it to the right app
        else if (mip_daemon->revents & POLLIN) {
            read_message(mip_daemon, hosts, num_hosts, app_fds);
        }

        else {
            // goes through all applications, skips mip daemon and connection socket
            for (int i=0; i<num_hosts; i++){
                
                //an application has disconnected
                if (app_fds[i].revents & POLLHUP) {
                    close(app_fds[i].fd);
                    app_fds[i].fd = 0;
                }

                // an applicatin has sent a message
                else if (app_fds[i].revents & POLLIN) {
                    //check if it has a port-number
                    if (hosts[i].port == 0){
                        char buffer_up[1];
                        read(app_fds[i].fd, buffer_up, 1);
                        if (available_port(hosts, num_hosts, buffer_up[0])){ 
                            hosts[i].port = buffer_up[0];
                            send_port_response(app_fds[i].fd, hosts[i].port, APPROVED);
                        }else {
                            send_port_response(app_fds[i].fd, hosts[i].port, DENIED);
                        }
                    }
                    else { //portnumber is known, this is a message to pass on
                        forward_to_mip(mip_daemon->fd, app_fds[i].fd, &hosts[i]);   
                    }
                }
            }
        }
    }
    
}