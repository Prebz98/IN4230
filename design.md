### fds
The first space in fds is saved for the general unix socket that clients can connect to.
The second space is saved for the raw socket.
The third space is for the unix client.
The fourth is for the routing daemon.
The fifth and sixth is extra spaces that is there to store pending connections. That is connections that has not identified itself yet. 

### Routing packets
All packets sent from the routing_daemon will have 3 chars that identifies the type of the packet. (3 first chars of the sdu) RES-response, REQ-request, UPD-update, HLO-hello. Then follows one char with the total number of pairs. A pair is a MIP-address and the distance to it. 

### Linked list

### ARP cache
Since I implemented the cache to store all addresses that it receives from, it now also stores 
