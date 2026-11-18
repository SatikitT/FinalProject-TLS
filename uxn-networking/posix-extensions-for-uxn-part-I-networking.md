<!-- Working title: POSIX extensions for Uxn -->

# Uxn extensions part I: networking

<!-- Another project: create a CPU version of Simone's parallelism device using pthreads -->

## Uxn and Uxntal

[`Uxntal`](https://wiki.xxiivv.com/site/uxntal.html) is the programming language for the [`Uxn`](https://wiki.xxiivv.com/site/uxn.html) virtual machine. As Uxn is a stack machine, Uxntal is a stack language, similar to e.g. [Forth](https://forth-standard.org/) or [Joy](https://dev.to/palm86/church-encoding-in-the-concatenative-language-joy-3nd8) in that it uses reverse Polish notation (postfix). It is an assembly language with opcodes for 8-bit and 16-bit operations on the stack and memory. To get the most out of this article, it is best if you have basic knowledge of Uxntal, either from the above resources or for example [the great tutorial at Compudanzas](https://compudanzas.net/uxn_tutorial.html).

## Extensions

My students and I have implemented a number of extensions for Uxn, with the aim of providing networking, process and thread functionality in line with the [POSIX](https://pubs.opengroup.org/onlinepubs/9799919799/) specification, and with an Uxntal API that stays close to the POSIX one. This post aims to introduce the networking API and also serves as its documentation.

## Uxn extension mechanism

Uxn uses devices for I/O but also as an extension mechanism: via the `DEO` and `DEI` calls we can interact with custom C code implemented as a device. [Varvara](https://wiki.xxiivv.com/site/varvara.html), the specification for devices communicating with the Uxn virtual machine, provides two reserved devices that can be used for implementation specific features that do not need to be part of the spec. I use one of these to implement an (implicit) pseudo-device that serves as the interface to all extensions. To use a specific extension, there is a simple meta-API:

        use-network
        use-threads
        use-process
        
Each of these selects a particular extension until another call replaces the selection. All extensions manage their own state internally so these calls can be alternated. The idea is that the programmer does not have to be aware of the extensions beyond this mechanism, and beyond the interface of each API call.

## Notational convention

To describe an API call, we list its input arguments and return values with their size. Unlike the Uxntal documentation, which assumes a byte by default and uses the `*` suffix to indicate a short, we use the suffix `:8` for a byte and `:16` for a short.
We follow the convention of the Uxntal documentation of separating the values taken from the working stack and those returned to it with a double dash `--`. To separate the call from the arguments we use `::`. For example:

        socket :: type:8 -- sfd:8
        
The call is called `socket`, it takes a byte and returns a byte. 

All API calls operate on the working stack only.

## Example: a trivial TCP client

This client sends a string to the server, which will print it.

        ~networking-devices.tal ( TODO: use values in API code so this can go )

        ( Address and port )
        %LOCALHOST { #7f00 #0001 } ( 127.0.0.01 IPv4 address, 4 bytes )
        %INADDR_ANY { #0000 #0000 } ( Any IPv4 address, 4 bytes )
        %MYPORT { #0da2 } ( 3490 )

        |0100
        @main
        ( Open the server TCP socket )
          socket DUP #ff EQU ?&error ( sfd:8 )
          
        ( Set address and port )
         LOCALHOST MYPORT ;hostaddr create_in_addr ( sfd:8 )
         
        ( Connect to server socket address )
          DUP ;hostaddr connect ?&error ( sfd:8 )
          
        ( Send the message)
          DUP ;msg #0010 #00 send #0010 NEQ2 ?&error ( sfd:8 )
          
        ( Close the socket )
          close
          
          &error
        BRK

        ( The actual API implementation )
        ~networking.tal
        
        ( The message to send to the server )
        @msg "Hello, 20 "world! 0a 00 00
        
        ( Address storage )
        @hostaddr &ipv4_addr $4 &port $2 
        
## Example: a trivial TCP server
        
        ~networking-devices.tal

        ( Address and port )
        %LOCALHOST { #7f00 #0001 } ( 127.0.0.01 IPv4 address, 4 bytes )
        %INADDR_ANY { #0000 #0000 } ( Any IPv4 address, 4 bytes )
        %MYPORT { #0da2 } ( 3490 )
        
        |0100
        @main
        ( Open the server TCP socket )
          socket DUP #ff EQU ?&error ( sfd:8 )
          
        ( Set the Reuse-Socket-Address option   )
          DUP DUP ( sfd:8 sfd:8 ) SO_REUSEADDR #01 setsockopt #ff EQU ?&error ( sfd:8 )          
        ( Set address and port )
         LOCALHOST MYPORT ;hostaddr create_in_addr
        ( Build server socket address         )
          DUP ;hostaddr bind ?&error ( sfd:8 )          
        ( create queue 1 only for client connection requests )
          DUP listen ?&error ( sfd:8 )
           
        ( Accept connection and read data until EOF, copying to STDOUT )
          DUP ;peeraddr accept DUP #ff EQU ?&error ( sfd:8 cfd:8 )
        ( cfd:8 is in the socket slot, the address and port are stored in peeraddr )
        ( WV: I don't like this. accept should return the cfd! CHECK! )
           ( .Network/socket DEI ( sfd:8 cfd:8 ) )
            &while
            DUP ;buf #0400 #00 recv ( len:16 ) 
            DUP2 #0000 GTH2 ?&cont 
            POP2
            !&done  
            &cont ( len:16 )
            ;buf 
            DUP2  ( len:16 ;buf:16 ;buf:16 )
            ROT2 ( ;buf:16 ;buf:16 len:16 )
            ADD2 ( ;buf:16 buf[n] )
            #00 ROT ROT STA ( buf[n] = 0; buf:16 )
           ( Thank you Devine )
            &whilebuf                  ( Create while label )
            LDAk DUP ?{         ( Load byte at address, jump if not null )
                POP POP2 BRK }  ( When null, pop text pointer, halt )
            #18 DEO             ( Send byte to Console/write port )
            INC2 !&whilebuf         ( Incr text pointer, jump to label )

            !&while
            &done ( sfd:8 cfd:8 )
          close
          close
        BRK
        &error

        BRK

        ( This should be in networking.tal )  
        ~networking.tal
        ( Address and buffer storage  )
        @hostaddr &ipv4_addr $4 &port $2 
        @peeraddr &ipv4_addr $4 &port $2 
        @buf $400  ( 1 kB max, ad hoc )

## API overview 

For each API call, the discussion provides the C call signature as reference. This is taken straight from the `man` page. The networking API is very similar to the Berkeley sockets API, which is also the POSIX API. At the moment, we only support IPv4. It is a low-level API, we don't provide any abstractions on top of it. The API is split into three parts: Network, Inet and Select. The main differences with the POSIX API arise because Uxn only supports 1-and 2-byte words.

### Network

Unlike the POSIX API, socket file descriptors are a single byte. For the error status we use `0xff` which is `-1` in two's complement.

#### `socket`: create a socket, return its descriptor  

        int socket(int domain, int type, int protocol);
    
Uxntal:

        socket :: type:8 -- sfd:8
        
We only support IPv4 sockets (`domain=AF_INET`). As a result, the type specifies the protocol (so `protocol==0`).
The valid `type`s are `SOCK_STREAM` for TCP and `SOCK_DGRAM` for UDP, which in the Uxntal API are defined as `0x01` and `0x00` respectively.

#### `setsockopt/getsockopt`: set/get socket options

        int setsockopt(int sockfd, int level, int optname,
                      const void *optval, socklen_t optlen);

        int getsockopt(int sockfd, int level, int optname,
                      void *optval, socklen_t *optlen);

Uxntal:

        setsockopt :: sfd:8 optname:8 optval:8 -- status:8
        getsockopt  :: sfd:8 opt:8 -- optval:8 status:8

We only support socket-level options (`level=SOL_SOCKET`) and ignore `optlen`. 

#### `connect`: Connect the peer address and port to the socket 

        int connect(int sockfd, const struct sockaddr *addr,
                   socklen_t addrlen);

Uxntal:

        connect :: sfd:8 addr_ptr:16 -- status:8
    
Here, `addr_ptr` is the 2-byte address of a location in Uxn memory which stores the IPv4 address and the port number in 6 bytes:

        @addr &ipv4_addr $4 &port $2 
    
The `addrlen` field in the C call is not used.

#### `bind`: Bind the host address and port to the socket

        int bind(int sockfd, const struct sockaddr *addr,
                socklen_t addrlen);
                
Uxntal:

        bind :: sfd:8 addr_ptr:16 -- status:8 
    
Same remarks as for `connect`.

#### `listen`: listen for connections
  
        int listen(int sockfd, int backlog);
        
Uxntal:

        listen :: sfd:8 -- status:8
    
We use the default value for `backlog`.
    
#### `accept`: Accept a connection 

        int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
        
Uxntal:

        accept :: sfd:8 addr_ptr:16 -- cfd:8 
        
Same remarks as for `connect`.

#### `send`: Send a message

        ssize_t send(int sockfd, const void *buf, size_t len, int flags);

        send :: sfd:8 buf_ptr:16 len:16  flags:8 -- len:16

#### `recv`: Receive a message 

        ssize_t recv(int sockfd, void *buf, size_t len, int flags);

        recv :: sfd:8 buf_ptr:16 buf_sz:16 flags:8 -- len:16

#### `close`: Close the socket 

        int close(int fd)
    
Uxntal:

        close :: sfd:8 -- status:8

### Device implementation

The network device adds a `DEI` and `DEO` handler:

        Uint8 network_dei(Uint8 addr);
        void network_deo(uint8_t addr);

These would be added to the `dei_handlers` and `deo_handlers` arrays in [`uxn2.c`]().

The implementaton of `network_dei()` is trivial:

        Uint8 network_dei(Uint8 addr) {
            return uxn.dev[addr];
        }
        
The implementaton of `network_deo()` is straightforward: for every POSIX socket API call, there is a corresponding wrapper call which unpacks the values stored in the network device slots, runs the actual POSIX call and if necessary update any relevant network device slots. For example, the wrapper for the `socket` call is:

        void net_socket(void) {
            int sfd = 0;
            uint8_t type = uxn.dev[NET_DEVICE+NET_type];
            if ((sfd=socket(AF_INET, (type==1 ? SOCK_STREAM : SOCK_DGRAM), 0)) == -1) {
                uxn.dev[NET_DEVICE+NET_status]= 0xFF; /* -1 in 2's complement, 1 byte */
            } else {
                uxn.dev[NET_DEVICE+NET_sfd]=(uint8_t)sfd;
                uxn.dev[NET_DEVICE+NET_status]=(uint8_t)sfd;
            }
        }


The appropriate call is selected based on the address written to the device:

        void network_deo(Uint8 addr) {
            uint8_t rel_addr = addr - NET_DEVICE ;
            if (rel_addr == NET_call) {    
                Uint8 value = uxn.dev[addr];
                switch (value) {
                    case NET_CMD_SOCKET:
                        net_socket();
                        break;
                    // ... etc ...
                }
            }
        }





