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
int argparser(int argc, char **argv, char* path, uint8_t *ttl) {
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
            printf("How to run\n./ping_client [-h] <ttl> <socket lower>\nAlternative: Use the makefile commands.\n");
            printf("Optional args:\n\
            -h Help\n\
            Non-optional args:\n\
            ttl: time to live\n\
            Socket lower: filename for unix socket to lower layer\n\n");
            printf("Program description:\nThe program listens to a unix socket and waits for a message. Then replacing the PING: part of the message with PONG: and sends it back. The program terminates if the socket closes.\n");
            exit(EXIT_SUCCESS);
            break;
        default:
            abort ();
        }

    index = optind;
    *ttl = atoi(argv[index]);
    strcpy(path, argv[index+1]);
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

void write_to_socket(char *msg, int msg_size, uint8_t mip, uint8_t ttl){
    /*
    * replace PING with PONG and send the message back
    * msg: message to be written
    */
    int total_msg_size = msg_size;
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    struct unix_packet *down = (struct unix_packet*)buffer;
    down->mip = mip;
    down->ttl = ttl;
    memcpy(down->msg, "PONG:", 5);
    memcpy(down->msg+5, &msg[5], msg_size);

    //32bit align
    int rest = total_msg_size % 4;
    total_msg_size += rest ? 4-rest : 0; 

    int rc;
    rc = write(sock_server, buffer, total_msg_size);
    printf("Sent back %d bytes to %d, message: \"%s\"\n", rc, down->mip, down->msg);
    printf(LINE);
}

void write_identifying_msg(){
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    uint8_t sdu_type = 0x02; //ping sdu type
    memset(buffer, sdu_type, 1);
    write(sock_server, buffer, 1);
    printf("Identify myself for daemon.\n");
}

void read_from_socket(uint8_t ttl){
    /*
    * Listens to the socket until it receives a message
    * then writes back
    */
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    int rc;
    if ((rc = read(sock_server, buffer, sizeof(buffer))) == -1 || rc == 0){
        done = true;
        return;
    }
    struct unix_packet *down = (struct unix_packet*)buffer;
    printf("Recieved %d bytes from: %d\n", rc, down->mip);
    printf("Message: %s\n", down->msg);
    write_to_socket(down->msg, rc, down->mip, ttl);
}

int main(int argc, char** argv){
    //the program listens to the socket until the socket closes. 

    char path[BUFSIZE];
    uint8_t ttl = 3;//default
    argparser(argc, argv, path, &ttl);
    setup_unix_socket(path);
    write_identifying_msg();
    printf(LINE);

    while(!done){
        read_from_socket(ttl);
    }

    clear_memory();
}