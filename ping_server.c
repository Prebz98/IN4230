#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/un.h>
#include "ping_client.h"
#include <unistd.h>
#include <sys/socket.h>
#include <ctype.h>
#include <stdbool.h>
#include "general.h"

struct sockaddr_un serv_addr[1];
int sock_server;
bool debug_mode = true;
bool done = false;

//parsing the arguments
int argparser(int argc, char **argv, char* path) {
    /*
    * parses all arguments
    * argc: number of system arguments given
    * argv: system arguments
    * path: path to unix socket fd
    */
    int index;
    int c;

    opterr = 0;

    while ((c = getopt (argc, argv, "h")) != -1)
        switch (c)
        {
        case 'h':
            printf("How to run\n./ping_client [-h] <socket lower>\nAlternative: Use the makefile commands.\n");
            printf("Optional args:\n\
            -h Help\n\
            Non-optional args:\n\
            Socket lower: filename for unix socket to lower layer\n\n");
            printf("Program description:\nThe program listens to a unix socket and waits for a message. Then replacing the PING: part of the message with PONG: and sends it back. The program terminates if the socket closes.\n");
            exit(EXIT_SUCCESS);
            break;
        default:
            abort ();
        }

    index = optind;
    strcpy(path, argv[index]);
    return 0;
}

void clear_memory(){
    close(sock_server);
}

void error(int ret, char *msg) {
    if (ret == -1) {
        perror(msg);
        clear_memory();
        exit(EXIT_FAILURE);
    }
}

void setup_unix_socket(char *path) {
    /*
    * setting up unix socket
    * path: path of socket fd
    */

    if ((sock_server = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
        error(sock_server, "socket");
    }
    memset(serv_addr, 0, sizeof(struct sockaddr_un));
    serv_addr->sun_family = AF_UNIX;
    strcpy(serv_addr->sun_path, path);
    if ((strlen(path) < strlen(serv_addr->sun_path))){
        error(-1, "Path is too long\n");
    }

    if (0 != (connect(sock_server, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_un)))){
        error(-1, "Failed to connect to server\n");
    }

    return;
}

void write_to_socket(char *msg){
    /*
    * replace PING with PONG and send the message back
    * msg: message to be written
    */
    char response[BUFSIZE];
    uint8_t mip_dst = msg[0];
    response[0] = mip_dst;
    strcpy(&response[1], "PONG:");
    strcpy(&response[6], &msg[6]);
    write(sock_server, response, strlen(response));
    printf("Sent back message: %s\n", response);
    printf(LINE);
}

void read_from_socket(){
    /*
    * Listens to the socket until it receives a message
    * then writes back
    */
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    int x;
    if ((x = read(sock_server, buffer, sizeof(buffer))) == -1 || x == 0){
        done = true;
        return;
    }
    printf("Message recieved: %s\n", &buffer[1]);
    write_to_socket(buffer);
}

int main(int argc, char** argv){
    //the program listens to the socket until the socket closes. 

    char path[BUFSIZE];
    argparser(argc, argv, path);
    setup_unix_socket(path);
    printf(LINE);

    while(!done){
        read_from_socket();
    }

    clear_memory();
}