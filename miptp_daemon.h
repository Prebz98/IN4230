#include "general.h"
#include <bits/types/struct_timeval.h>
#include <sys/poll.h>

#define APPROVED 1
#define DENIED 0

struct app_pdu{
    uint8_t mip;
    uint8_t port;
    char sdu[BUFSIZE-2];
}__attribute__((packed));

struct miptp_pdu{
    uint8_t src_port;
    uint8_t dst_port;
    uint16_t seq : 14;
    uint8_t padding : 2;
    char sdu[BUFSIZE-4];
}__attribute__((packed));

struct host{
    uint8_t port;
    int fd_index;
    int seq_recv;
    int seq_send;
    struct message_node *message_queue;
};

struct message_node{
    struct message_node *next;
    struct unix_packet packet;
    struct timeval time;
    int size;
    int seq;
};

int argparser(int argc, char **argv,int *timeout_secs, char* mip_daemon, char* path_to_higher);
int connect_to_mip_daemon(char* path_to_mip, struct pollfd *fds);
void write_identifying_msg(int mip_fd);
void setup_unix_socket(char *path, struct pollfd *fds);
int available_port(struct host *hosts, int num_hosts, uint8_t port);
void send_port_response(int fd, uint8_t port, int approved);
void forward_to_mip(int mip_daemon, int application, struct host *host);
int index_of_port(uint8_t port, struct host *hosts, int num_hosts);
void send_ack(struct pollfd *mip_daemon, uint8_t dst_port, uint8_t dst_mip, uint16_t seq, uint8_t src_port);
void read_message(struct pollfd *mip_daemon, struct host *hosts, int num_hosts, struct pollfd *applications);
void resend_window(struct message_node *queue, struct pollfd *mip_daemon);