### fds
The first space in fds is saved for the general unix socket that clients can connect to.
The second space is saved for the raw socket.
The third space is for the unix client.
The fourth is for the routing daemon.
The fifth and sixth is extra spaces that is there to store pending connections. That is connections that has not identified itself yet. 

### Routing packets
All packets sent from the routing_daemon will have 3 chars that identifies the type of the packet. (3 first chars of the sdu) RES-response, REQ-request, UPD-update, HLO-hello. Then follows one char with the total number of pairs. A pair is a MIP-address and the distance to it. 

### Routing linked list
The routing table is stored as a linked list that contains the mip-target, the distance and the next node on the way. 


### ARP cache
Since I implemented the arp-cache to store all addresses that it receives from the raw socket, it now stores addresses that are not neighbors. I choose to let this stay the way it is since this does not affect the routing process.  
