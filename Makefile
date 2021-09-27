VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes

MIP_A = 1
MIP_B = 2
MIP_C = 3
SOCK_A = tmp
SOCK_B = tmp2
SOCK_C = tmp3

MIP_DAEMON_A = ./mip_daemon -d -t ${SOCK_A} ${MIP_A}
MIP_DAEMON_B = ./mip_daemon -d -t ${SOCK_B} ${MIP_B}
MIP_DAEMON_C = ./mip_daemon -d -t ${SOCK_C} ${MIP_C}

# terminating versions
MIP_DAEMON_A_t = ./mip_daemon -d -t ${SOCK_A} ${MIP_A}
MIP_DAEMON_B_t = ./mip_daemon -d -t ${SOCK_B} ${MIP_B}
MIP_DAEMON_C_t = ./mip_daemon -d -t ${SOCK_C} ${MIP_C}

MIP_CLIENT_AB = ./ping_client ${MIP_B} "Hello from A" ${SOCK_A}
MIP_CLIENT_BA = ./ping_client ${MIP_A} "Hello from B" ${SOCK_B}
MIP_CLIENT_BC = ./ping_client ${MIP_C} "Hello from B" ${SOCK_B}
MIP_CLIENT_CB = ./ping_client ${MIP_B} "Hello from C" ${SOCK_C}

MIP_SERVER_A = ./ping_server ${SOCK_A}
MIP_SERVER_B = ./ping_server ${SOCK_B}
MIP_SERVER_C = ./ping_server ${SOCK_C}

all: ping_client mip_daemon ping_server

mip_daemon: mip_daemon.c
	gcc -g mip_daemon.c -o mip_daemon

ping_client: ping_client.c
	gcc -g ping_client.c -o ping_client

ping_server: ping_server.c
	gcc -g ping_server.c -o ping_server

runDaemonA: mip_daemon
	${MIP_DAEMON_A}

runDaemonB: mip_daemon
	${MIP_DAEMON_B}

runDaemonC: mip_daemon
	${MIP_DAEMON_C}

runClientA: ping_client
	${MIP_CLIENT_AB}

runClientBA: ping_client
	${MIP_CLIENT_BA}

runClientBC: ping_client
	${MIP_CLIENT_BC}

runClientC: ping_client
	${MIP_CLIENT_CB}

runServerA: ping_server
	${MIP_SERVER_A}

runServerB: ping_server
	${MIP_SERVER_B}

runServerC: ping_server
	${MIP_SERVER_C}

clean: 
	rm -f *.o ping_client ping_server mip_daemon

valgrindDaemonA: mip_daemon
	${VALGRIND} ${MIP_DAEMON_A}

valgrindDaemonB: mip_daemon
	${VALGRIND} ${MIP_DAEMON_B}

valgrindDaemonC: mip_daemon
	${VALGRIND} ${MIP_DAEMON_C}

valgrindClientA: ping_client
	${VALGRIND} ${MIP_CLIENT_A}

valgrindClientB: ping_client
	${VALGRIND} ${MIP_CLIENT_B}

valgrindClientC: ping_client
	${VALGRIND} ${MIP_CLIENT_C}

valgrindServerA: ping_server
	${VALGRIND} ${MIP_SERVER_A}

valgrindServerB: ping_server
	${VALGRIND} ${MIP_SERVER_B}

valgrindServerC: ping_server
	${VALGRIND} ${MIP_SERVER_C}