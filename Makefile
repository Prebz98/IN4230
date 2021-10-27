VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes

TTL = 3
TIMEOUT = 1000
FILE_TO_SEND = noe

MIP_A = 10
MIP_B = 20
MIP_C = 30
MIP_D = 40
MIP_E = 50

PORT_A = 1
PORT_B = 2
PORT_C = 3
PORT_D = 4
PORT_E = 5


SOCK_A = tmp
SOCK_B = tmp2
SOCK_C = tmp3
SOCK_D = tmp4
SOCK_E = tmp5

MIPTP_SOCK_A = tpA
MIPTP_SOCK_B = tpB
MIPTP_SOCK_C = tpC
MIPTP_SOCK_D = tpD
MIPTP_SOCK_E = tpE

MIP_DAEMON_A = ./mip_daemon ${SOCK_A} ${MIP_A}
MIP_DAEMON_B = ./mip_daemon ${SOCK_B} ${MIP_B}
MIP_DAEMON_C = ./mip_daemon ${SOCK_C} ${MIP_C}
MIP_DAEMON_D = ./mip_daemon ${SOCK_D} ${MIP_D}
MIP_DAEMON_E = ./mip_daemon ${SOCK_E} ${MIP_E}

MIP_CLIENT_AB = ./ping_client ${MIP_B} ${TTL} "Hello from A" ${SOCK_A}
MIP_CLIENT_BA = ./ping_client ${MIP_A} ${TTL} "Hello from B" ${SOCK_B}
MIP_CLIENT_BC = ./ping_client ${MIP_C} ${TTL} "Hello from B" ${SOCK_B}
MIP_CLIENT_CB = ./ping_client ${MIP_B} ${TTL} "Hello from C" ${SOCK_C}
MIP_CLIENT_DB = ./ping_client ${MIP_B} ${TTL} "Hello from C" ${SOCK_D}
MIP_CLIENT_EA = ./ping_client ${MIP_A} ${TTL} "Hello from C" ${SOCK_E}
MIP_CLIENT_AE = ./ping_client ${MIP_E} ${TTL} "Hello from A" ${SOCK_A}
MIP_CLIENT_AC = ./ping_client ${MIP_C} ${TTL} "Hello from A" ${SOCK_A}

MIP_SERVER_A = ./ping_server ${TTL} ${SOCK_A}
MIP_SERVER_B = ./ping_server ${TTL} ${SOCK_B}
MIP_SERVER_C = ./ping_server ${TTL} ${SOCK_C}
MIP_SERVER_D = ./ping_server ${TTL} ${SOCK_D}
MIP_SERVER_E = ./ping_server ${TTL} ${SOCK_E}


ROUTING_DAEMON_A = ./routing_daemon ${SOCK_A}
ROUTING_DAEMON_B = ./routing_daemon ${SOCK_B}
ROUTING_DAEMON_C = ./routing_daemon ${SOCK_C}
ROUTING_DAEMON_D = ./routing_daemon ${SOCK_D}
ROUTING_DAEMON_E = ./routing_daemon ${SOCK_E}

MIPTP_DAEMON_A = ./miptp_daemon ${TIMEOUT} ${SOCK_A} ${MIPTP_SOCK_A}
MIPTP_DAEMON_B = ./miptp_daemon ${TIMEOUT} ${SOCK_B} ${MIPTP_SOCK_B}
MIPTP_DAEMON_C = ./miptp_daemon ${TIMEOUT} ${SOCK_C} ${MIPTP_SOCK_C}
MIPTP_DAEMON_D = ./miptp_daemon ${TIMEOUT} ${SOCK_D} ${MIPTP_SOCK_D}
MIPTP_DAEMON_E = ./miptp_daemon ${TIMEOUT} ${SOCK_E} ${MIPTP_SOCK_E}

FILE_TRANSFER_A = ./file_transfer ${FILE_TO_SEND} ${MIPTP_SOCK_A} ${MIP_E} ${PORT_E}

all: ping_client mip_daemon ping_server routing_daemon miptp_daemon file_transfer

mip_daemon: mip_daemon.c mip_daemon_utils.c
	gcc -g mip_daemon.c mip_daemon_utils.c -o mip_daemon

ping_client: ping_client.c
	gcc -g ping_client.c -o ping_client

ping_server: ping_server.c
	gcc -g ping_server.c -o ping_server

routing_daemon: routing_daemon.c routing_utils.c
	gcc -g routing_daemon.c routing_utils.c -o routing_daemon

miptp_daemon: miptp_daemon.c miptp_utils.c
	gcc -g miptp_daemon.c miptp_utils.c -o miptp_daemon

file_transfer: file_transfer.c file_transfer_utils.c
	gcc -g file_transfer.c file_transfer_utils.c -o file_transfer

runDaemonA: mip_daemon
	${MIP_DAEMON_A}

runDaemonB: mip_daemon
	${MIP_DAEMON_B}

runDaemonC: mip_daemon
	${MIP_DAEMON_C}

runDaemonD: mip_daemon
	${MIP_DAEMON_D}

runDaemonE: mip_daemon
	${MIP_DAEMON_E}

runClientA: ping_client
	${MIP_CLIENT_AB}

runClientBA: ping_client
	${MIP_CLIENT_BA}

runClientAC: ping_client
	${MIP_CLIENT_AC}

runClientAE: ping_client
	${MIP_CLIENT_AE}

runClientBC: ping_client
	${MIP_CLIENT_BC}

runClientC: ping_client
	${MIP_CLIENT_CB}

runClientD: ping_client
	${MIP_CLIENT_DB}

runClientE: ping_client
	${MIP_CLIENT_EA}

runServerA: ping_server
	${MIP_SERVER_A}

runServerB: ping_server
	${MIP_SERVER_B}

runServerC: ping_server
	${MIP_SERVER_C}

runServerD: ping_server
	${MIP_SERVER_D}

runServerE: ping_server
	${MIP_SERVER_E}

runRouterA: routing_daemon
	${ROUTING_DAEMON_A}

runRouterB: routing_daemon
	${ROUTING_DAEMON_B}

runRouterC: routing_daemon
	${ROUTING_DAEMON_C}

runRouterD: routing_daemon
	${ROUTING_DAEMON_D}

runRouterE: routing_daemon
	${ROUTING_DAEMON_E}

runMiptpA: miptp_daemon
	${MIPTP_DAEMON_A}

runMiptpB: miptp_daemon
	${MIPTP_DAEMON_B}

runMiptpE: miptp_daemon
	${MIPTP_DAEMON_E}

runFTA: file_transfer
	${FILE_TRANSFER_A}

runFTB: file_transfer
	${FILE_TRANSFER_B}

runFTC: file_transfer
	${FILE_TRANSFER_C}

runFTD: file_transfer
	${FILE_TRANSFER_D}

runFTE: file_transfer
	${FILE_TRANSFER_E}

clean: 
	rm -f *.o ping_client ping_server mip_daemon routing_daemon

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

valgrindRouterA: routing_daemon
	${VALGRIND} ${ROUTING_DAEMON_A}