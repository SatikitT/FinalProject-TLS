# uxn-networking

## Network device and Uxntal socket API

This is a POSIX network API for Uxn. The main reason for creating it is so my students can learn about networking in Uxntal rather than Python.

The overall approach is that the Network address space is used to hand over arguments from user space to the socket calls.
Apart from the actual socket API calls, there are a few other API calls that are commonly used, in particular inet_ntoa, inet_addr  and select() so I have added those as well.
The Inet and Select address spaces overlap with the Network address space. As all of them are stateless this is OK.

The implementation is very simple: we use DEO to provide the arguments and DEI for the return values. DEO of .Network/call triggers calling of a wrapper around the corresponding POSIX call.
The only extra functionality in the wrappers is that they close the socket on error, purely a convenience.
I have tested it with a minimal TCP server and client, they are in the repo as well. I've also included some C sources for reference.

To build the network-enabled version of `uxncli`:

	./build.sh

To build the TCP client-server echo example:	

	./build_demo.sh

To run the server:

	./bin/uxncli tcp-echo-server-minimal.rom

To run the client, in a separate terminal window:

	./bin/uxncli tcp-echo-client-minimal.rom

If all is well, the server will print "Hello, world!" and exit.

## API details 	

	( Network device layout ** )
	
	|d0 @Network ( 0 ) &socket $1 ( 1 ) &addr $2 ( 3 ) &opt $1 ( 4 ) &optval $1 ( 5 ) &buf $2 ( 7 ) &len $2 ( 9 ) &flags $1 ( 10 ) &status $1 ( 11 ) &call $1 
	|d0 @Inet ( 0 ) &addr $2 ( 2 ) &hostname $2
	|d0 @Select ( 0 ) &nfds $1 ( 1 ) &fdsets $2 ( 3 ) &timeout $2

The calls implemented so far are:

	( Create a socket )
	( -- sfd )
	socket

	( Set socket options, SOL_SOCKET level only )
	( sfd opt optval -- status )
	setsockopt

	( Get socket options, SOL_SOCKET level only )
	( sfd opt -- optval status )
	getsockopt 

	( Connect the peer addr and port to the socket )
	( sfd addr_ptr* -- status )
	connect

	( Bind the host addr and port to the socket )
	( sfd addr_ptr* -- status )
	bind

	( Listen for connections )
	( sfd -- status )
	listen

	( Accept a connection )
	( sfd addr_ptr* -- status )
	accept 

	( Send a message )
	( sfd buf_ptr* len*  flags -- len* )
	send 

	( Receive a message )
	( sfd buf_ptr* buf_sz* flags -- len*  )
	recv 

	( Close the socket )
	( sfd -- status )
	close 

	( Select a file descriptor )
	( nfds fd_sets_ptr* timeout* -- status )
	( ;FD_sets/nfds ;FD_sets timeout* )
	select 

	( IPv4 dotted quad address string to 4 byte number )
	( ipv4addrstr_ptr* addr_ptr* -- addr_ptr* )
	inet_addr

	( 4 byte number to IPv4 dotted quad address string )
	( addr_ptr* ipv4addrstr_ptr* -- ipv4addrstr_ptr* )
	inet_ntoa