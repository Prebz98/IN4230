#include "general.h"
#include "miptp_daemon.h"
#include <bits/getopt_core.h>
#include <bits/types/struct_timeval.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>

void error(int ret, char *msg) {
    //program error
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

int connect_to_mip_daemon(char* path_to_mip, struct pollfd *fds){
    /*
    * connects to the mip daemon

    * path_to_mip: path to the unix socket to mip_daemon
    * fds: list to save the fd

    * returns: (int) the fd
    */
    struct sockaddr_un serv_addr;
    int mip_fd;

    if ((mip_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0){
        error(mip_fd, "socket setup failed\n");
    }
    memset(&serv_addr, 0, sizeof(struct sockaddr_un));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, path_to_mip);
    if ((strlen(path_to_mip)) < strlen(serv_addr.sun_path)){
        error(-1, "Path is too long");
    }
    if (0 != (connect(mip_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_un)))){
        error(-1, "Failed to connect to server");
    }

    fds[0].fd = mip_fd;
    fds[0].events = POLLHUP | POLLIN;
    return mip_fd;
}

void write_identifying_msg(int mip_fd){
    /*
    * write identifying message

    * mip_fd: fd to mip_daemon
    */
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    uint8_t sdu_type = 0x05; //transport sdu type
    memset(buffer, sdu_type, 1);
    write(mip_fd, buffer, 1);
    printf("Identify myself for daemon.\n");
}

void send_ack(struct pollfd *mip_daemon, uint8_t dst_port, uint8_t dst_mip, uint16_t seq, uint8_t src_port){
    /*
    * sends an ack message to target

    * mip_daemon: mip_daemon pollfd
    * dst_port: port to target
    * dst_mip: mip to target
    * seq: seq number
    * src_port: source port
    */

    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    struct unix_packet *packet = (struct unix_packet*)buffer;
    struct miptp_pdu *miptp_pdu = (struct miptp_pdu*)packet->msg;

    // fill the packet sdu
    miptp_pdu->dst_port = dst_port;
    miptp_pdu->src_port = src_port;
    miptp_pdu->seq = seq;
    memcpy(miptp_pdu->sdu, "ACK", 3);

    // fill the packet header
    packet->mip = dst_mip;
    packet->ttl = 0;
    

    write(mip_daemon->fd, packet, 12);
}

int add_to_queue(struct message_node *new_node, struct message_node *queue){
    /*
    * adds a new message node to a queue

    * new_node: the node to add
    * queue: the queue

    * returns: index of new packet
    */

    struct message_node *current_node = queue;
    int i = 0;
    while (current_node->next != NULL) {
        current_node = current_node->next;
        i++;
    }
    current_node->next = new_node;
    return i;
}

void forward_to_mip(int mip_daemon, int application, struct host *host){
    /*
    * reads a message from the application and forwards it to mip daemon

    * mip_daemon: mip daemon fd
    * application: application fd
    */
    
    char buffer_up[BUFSIZE];
    char buffer_down[BUFSIZE];
    int rc = read(application, buffer_up, BUFSIZE);
    uint8_t dst_mip = buffer_up[0];
    uint8_t dst_port = buffer_up[1];
    memset(buffer_down, 0, BUFSIZE);

    //32 bit align
    int size = 6 + rc; //6 bytes of header, 4miptp + 2mip
    int rest = size % 4;
    size += rest ? 4-rest : 0;

    struct unix_packet *packet = (struct unix_packet*)buffer_down;
    struct miptp_pdu *miptp_pdu = (struct miptp_pdu*)packet->msg;

    miptp_pdu->dst_port = buffer_up[1];
    miptp_pdu->seq = htons(host->seq_send);
    //take care of overflow
    if (host->seq_send == 1 << 14){
        host->seq_send = 0;
    }else {
        host->seq_send = host->seq_send +1;
    }

    miptp_pdu->src_port = host->port;
    memcpy(miptp_pdu->sdu, buffer_up, rc);

    packet->mip = dst_mip;
    packet->ttl = 0; //default 
    memcpy(packet->msg, miptp_pdu, size); 

    //save to buffer
    struct message_node *new_node = malloc(sizeof(struct message_node));
    new_node->packet = *packet;
    new_node->next = NULL;
    new_node->seq = ntohs(miptp_pdu->seq);
    struct timeval time;
    gettimeofday(&time, NULL);
    new_node->time = time;
    new_node->size = size;

    add_to_queue(new_node, host->message_queue);
    //send now if its in the current window
    if ((ntohs(miptp_pdu->seq) - host->message_queue->next->seq) < 16){
        write(mip_daemon, buffer_down, size);
    }
}

int index_of_port(uint8_t port, struct host *hosts, int num_hosts){
    /*
    * returns the index of the portnumber in portnumbers

    * port: the port to look for
    * hosts: list of hosts
    * num_hosts: length of the list

    * returns (int) the index of the port, -1 if not found
    */

    for (int i=0; i<num_hosts; i++){
        if (hosts[i].port == port){
            return i;
        }
    }
    return -1;
}

void resend_window(struct message_node *queue, struct pollfd *mip_daemon){
    /*
    * send the 16 first packets

    * queue: the message queue
    */
    struct timeval time;
    gettimeofday(&time, NULL);
    struct message_node *current_node = queue;
    int i = 0;
    while (i < 17 && current_node->next != NULL) {
        current_node = current_node->next;
        current_node->time = time;
        write(mip_daemon->fd, &current_node->packet, current_node->size);
        i++;
    }
}

void send_next(struct pollfd *mip_daemon, struct host *host){
    /*
    * sends the 16th packet in the queue if there is one
    * this happens after the window has been moved

    * mip_daemon: mip_daemon fd
    * host: application host
    */

    struct message_node *current_node = host->message_queue;
    int i = 0;
    while (current_node->next != NULL && i != 15) {
        current_node = current_node->next;
    }
    if (i == 15){
        struct timeval time;
        gettimeofday(&time, NULL);
        write(mip_daemon->fd, &current_node->packet, current_node->size);
        current_node->time = time;
    }
}

void read_message(struct pollfd *mip_daemon, struct host *hosts, int num_hosts, struct pollfd *applications){
    /*
    * reads a message and checks if its an ACK or a message for an app. Then treats it accordingly. 

    * mip_daemon: mip_daemon pollfd
    * hosts: all hosts
    * num_hosts: number of hosts
    * applications: fds to application hosts
    */
    char buffer[BUFSIZE];
    int rc = read(mip_daemon->fd, buffer, BUFSIZE);
    struct unix_packet *packet_received = (struct unix_packet*)buffer; 
    struct miptp_pdu *tp_pdu = (struct miptp_pdu*)packet_received->msg;
    uint8_t src_port = tp_pdu->src_port;
    uint8_t src_mip = packet_received->mip;
    uint8_t dst_port = tp_pdu->dst_port;
    uint16_t seq = ntohs(tp_pdu->seq);

    int index_of_app = index_of_port(dst_port, hosts, num_hosts);
    if (memcmp(tp_pdu->sdu, "ACK", 3) == 0){
        //move window by 1 if there are new messages in the window, and the sequencenumber is what is expected
        struct message_node *old = hosts[index_of_app].message_queue;
        if (old != NULL && seq==ntohs(hosts[index_of_app].message_queue->next->seq)){
            hosts[index_of_app].message_queue = hosts[index_of_app].message_queue->next;
            free(old);
            send_next(mip_daemon, &(hosts[index_of_app]));
        }
    }
    else { //its a message for an app
        int app_fd = applications[index_of_app].fd;
        int *exp_seq = &hosts[index_of_app].seq_recv;
        send_ack(mip_daemon, src_port, src_mip, seq, dst_port);
        // not expected seq, ignore
        if (seq != *exp_seq && *exp_seq != -1){
            return;
        }

        //first packet, sets seq
        if (*exp_seq == -1) {
            *exp_seq = seq;
        }

        //increase exp_seq
        //take care of overflow
        if (*exp_seq == 1 << 14) {
            *exp_seq = 0;    
        }else {
            *exp_seq = *exp_seq +1;
        }
        struct app_pdu *message_to_send = (struct app_pdu*)tp_pdu->sdu;
        message_to_send->mip = src_mip;
        message_to_send->port = src_port;

        write(app_fd, message_to_send, rc-6); // 2 bytes removed from unix_packet to miptp_pdu
        //4 removed from miptp_pdy to app_pdu
    }
}

void send_port_response(int fd, uint8_t port, int approved){
    /*
    * sends a response that the portnumber was accepted or denied

    * fd: fd to send to
    * port: portnumber that was approved
    * approved: 1 if approved, 0 if denied
    */
    uint8_t buffer[BUFSIZE];
    memcpy(buffer, &approved, 1);
    memcpy(&buffer[1], &port, 1);

    write(fd, buffer, 2);
}

int available_port(struct host *hosts, int num_hosts, uint8_t port){
    /*
    * checks if a port is available

    * hosts: list of hosts
    * num_hosts: length of the list
    * port: port number to check

    * returns (int) 1 if available, 0 if not
    */
    for (int i=0; i<num_hosts; i++){
        if (hosts[i].port == port){
            return 0;
        }
    }
    return 1;
}

void setup_unix_socket(char *path, struct pollfd *fds) {
    /* 
    * setting up unix socket fro applications to connect
    * path: path to unix socket fd
    * fds: all fds
    */

    int sock_server;
    struct sockaddr_un serv_addr;
    if ((sock_server = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
        error(sock_server, "socket");
    }
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, path);
    if ((strlen(path) < strlen(serv_addr.sun_path))){
        error(-1, "Path is too long\n");
    }
    unlink(serv_addr.sun_path);

    //bind the socket
    if (bind(sock_server, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_un)) < 0) {
        error(-1, "bind");
    }

    // listen to the socket for applications to connect
    listen(sock_server, 1);
    fds[1].fd = sock_server;
    fds[1].events = POLLHUP | POLLIN;
}


int argparser(int argc, char **argv, int *timeout_secs, char* mip_daemon, char* path_to_higher) {
    /*
    * parses all arguments

    * timeout_msecs: how long the MIPTP will wait before a packet is considered lost
    * mip_daemon: path to the lower layer unix socket
    * path_to_higher: path to the upper layer unix socket
    */
    int index;
    int c;

    opterr = 0;

    while ((c = getopt (argc, argv, "h")) != -1)
        switch (c)
        {
        case 'h':
            printf("How to run\n./miptp_daemon [-h] <timeout> <socket lower> <socket upper>\nAlternative: Use the makefile commands.\n");
            printf("Optional args:\n\
            -h Help\n\
            Non-optional args:\n\
            Timeout: timeout for go back N in secs\
            Socket lower: filename for unix socket to lower layer\n\n");
            printf("Program description:\nThe program will work as a transport layer over the mip network layer. It will send packets received by the application down to the mip daemon. And it will send packet received by the mip daemon up to the application. The MIPTP daemon will reply with acks to ensure reliability and in-order data transmission.\n");
            exit(EXIT_SUCCESS);
            break;
        default:
            abort ();
        }

    index = optind;
    *timeout_secs = atoi(argv[index]);
    strcpy(mip_daemon, argv[index+1]);
    strcpy(path_to_higher, argv[index+2]);
    return 0;
}