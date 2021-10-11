
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
#include <time.h>
#include <fcntl.h>

struct sockaddr_un serv_addr[1];
int sock_server = 0;
bool done = false;

int argparser(int argc, char **argv, char* path, uint8_t *mip_dst, char *msg) {
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
            printf("How to run\n./ping_client [-h] <destination_host> <message> <socket lower>\nIf the message conatin spaces, use this syntax: \"word1 word2\"\nAlternative: Use the makefile commands.\n");
            printf("Optional args:\n\
            -h Help\n\
            Non-optional args:\n\
            destination_host: MIP address for destination host\n\
            Message: Message to be sent.\n\
            Socket lower: filename for unix socket to lower layer\n\n");
            printf("Program description:\nThe program will put the mip-destination and message in a packet, and send it to a lower layer through a unix socket. Then it will listen on that socket, waiting for a response, then printing the time used. If the program does not receive a matching response within 1 second, the program terminates with a timeout.\n");
            exit(EXIT_SUCCESS);
            break;
        default:
            abort ();
        }

    index = optind;
    *mip_dst = atoi(argv[index]);
    strcpy(msg, argv[index+1]);
    strcpy(path, argv[index+2]);
    return 0;
}

void clear_memory(){
    /*
    * clearing memory and closing sockets
    * sock_server: fd to the unix socket
    */
    close(sock_server);
}

void error(int ret, char *msg) {
    //program error
    if (ret == -1) {
        perror(msg);
        clear_memory();
        exit(EXIT_FAILURE);
    }
}

void socket_setdown(char *path) {
    /*
    * setting down unix socket, and connecting to it
    * path: path to unix socket fd
    */

    if ((sock_server = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
        error(sock_server, "socket setdown failed\n");
    }
    fcntl(sock_server, F_SETFL, O_NONBLOCK);
    memset(serv_addr, 0, sizeof(struct sockaddr_un));
    serv_addr->sun_family = AF_UNIX;
    strcpy(serv_addr->sun_path, path);
    if ((strlen(path)) < strlen(serv_addr->sun_path)){
        error(-1, "Path is too long\n");
    }

    if (0 != (connect(sock_server, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_un)))){
        error(-1, "Failed to connect to server\n");
    }
}

void write_to_server(char *msg, uint8_t msg_size, uint8_t mip_dst){
    /*
    * sending message to daemon
    * mip address first 8 bits, then msg
    * msg: message to send
    * mip_dst: MIP destination
    */

    int total_msg_size = 7+msg_size;
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    struct unix_packet *down = (struct unix_packet*)buffer;
    down->mip = mip_dst;
    down->ttl = 0;
    memcpy(down->msg, "PING:", 5);
    memcpy(down->msg+5, msg, msg_size);

    //32bit align
    int rest = total_msg_size % 4;
    total_msg_size += rest ? 4-rest : 0; 

    int rc;
    rc = write(sock_server, buffer, total_msg_size);
    printf("Sent %d bytes to %d: \"%s\"\n", rc, mip_dst, down->msg);
}

void write_identifying_msg(){
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    uint8_t sdu_type = 0x02; //ping sdu type
    memset(buffer, sdu_type, 1);
    write(sock_server, buffer, 1);
    printf("Identify myself for daemon.\n");
}

void read_from_socket(char* expected_resp){
    /*
    * reading from socket
    * if the responce is the expected response, terminate
    * expected_resp: expected response
    */
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    int rc;

    //server disconnect
    if ((rc = read(sock_server, buffer, sizeof(buffer))) == 0){
        done = true;
        return;
    }
    struct unix_packet *down = (struct unix_packet*)buffer;
    if (strcmp(down->msg, expected_resp) == 0){
        printf("%d bytes recieved from %d: %s\n", rc, down->mip, down->msg);
        done = true;
        return;
    }else if (strcmp(down->msg, expected_resp) > 0) {
        printf("Not match: %s\n", down->msg);
    }
}

int main(int argc, char* argv[]) {
    uint8_t mip_dst;
    char msg[BUFSIZE];
    char path[BUFSIZE];
    char expected_resp[BUFSIZE];
    float diff = 0;
    argparser(argc, argv, path, &mip_dst, msg);

    memset(serv_addr, 0, sizeof(struct sockaddr_un));
    printf(LINE);
    strcpy(expected_resp, "PONG:");
    strcat(expected_resp, msg);
    socket_setdown(path);
    write_identifying_msg();
    write_to_server(msg, strlen(msg), mip_dst);
    clock_t start = clock();

    while(!done){
        diff = ((clock()-start)*1000.0)/CLOCKS_PER_SEC;
        if (diff/1000 > 1){
            printf("Timeout\n");
            exit(EXIT_SUCCESS);
        }
        read_from_socket(expected_resp);
    }
    printf("Time: %f seconds\n", diff/1000);
    clear_memory();
    return EXIT_SUCCESS;
}