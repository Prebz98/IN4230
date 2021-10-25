int argparser(int argc, char **argv,int *timeout_msecs, char* mip_daemon, char* path_to_higher);
void connect_to_mip_daemon(char* path_to_mip, int *mip_fd);