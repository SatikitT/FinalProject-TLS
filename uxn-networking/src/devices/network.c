#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
/* for close */
#include <unistd.h>
/* for  bzero */
#include <strings.h>
/* for memcpy */
#include <string.h>
/* for inet_ntoa */
#include <arpa/inet.h>
/* for select */
#include <sys/select.h>

#include "../uxn.h"


//#define NET_DBG 1
/* Calls */

#define NET_CMD_SOCKET 0x01
#define NET_CMD_SETSOCKOPT 0x02
#define NET_CMD_GETSOCKOPT 0x03
#define NET_CMD_CONNECT 0x04
#define NET_CMD_BIND 0x05
#define NET_CMD_LISTEN 0x06
#define NET_CMD_ACCEPT 0x07
#define NET_CMD_SEND 0x08
#define NET_CMD_RECV 0x09
#define NET_CMD_CLOSE 0x0a
#define NET_CMD_SELECT 0x0b
#define NET_CMD_INET_ADDR 0x0c
#define NET_CMD_INET_NTOA 0x0d

/* Device layout */
#define NET_DEVICE 0xd0
#define NET_sfd 0x00
#define NET_addr 0x01
#define NET_opt 0x03
#define NET_optval 0x04
#define NET_buf 0x05
#define NET_len 0x07
#define NET_flags 0x09
#define NET_status 0x0a
#define NET_call 0x0b
#define NET_type 0x0c
#define NET_bytes34 0x0e

#define INET_DEVICE 0xd0
#define INET_addr 0x00
#define INET_hostname 0x02

#define SELECT_DEVICE 0xd0
#define SELECT_nfds 0x00
#define SELECT_fdsets 0x01
#define SELECT_timeout 0x03

extern Uxn uxn;

/* POSIX call wrappers */
void net_close(void) {
#ifdef NET_DBG
    printf("DBG: Closing socket\n");
#endif

    uint8_t sfd = uxn.dev[NET_DEVICE+NET_sfd];
    int status = close((int)sfd);
    uxn.dev[NET_DEVICE+NET_status]=(uint8_t)status;
}

void net_socket(void) {
#ifdef NET_DBG
    printf("DBG: Creating socket\n");
#endif
    int sfd = 0;
    uint8_t type = uxn.dev[NET_DEVICE+NET_type];
    if ((sfd=socket(AF_INET, (type==1 ? SOCK_STREAM : SOCK_DGRAM), 0)) == -1) {
#ifdef NET_DBG    
        printf("DBG: Socket creation failed\n");
#endif        
        uxn.dev[NET_DEVICE+NET_status]= 0xFF; /* -1 in 2's complement, 1 byte */
    } else {
#ifdef NET_DBG
    printf("DBG: Socket of type %d created successfully, fd=%d\n", type,sfd);
#endif
        uxn.dev[NET_DEVICE+NET_sfd]=(uint8_t)sfd;
        uxn.dev[NET_DEVICE+NET_status]=(uint8_t)sfd;
    }
}

/*
@SO_REUSEADDR #02 JMP2r
@SO_TYPE #03 JMP2r
@SO_ERROR #04 JMP2r
@SO_DONTROUTE #05 JMP2r
@SO_BROADCAST #06 JMP2r
@SO_SNDBUF #07 JMP2r
@SO_RCVBUF #08 JMP2r
@SO_KEEPALIVE #09 JMP2r
@SO_OOBINLINE #0a JMP2r
@SO_LINGER #0d JMP2r
@SO_RCVLOWAT #12 JMP2r
@SO_SNDLOWAT #13 JMP2r
@SO_ACCEPTCONN #1e JMP2r

 */

int sockopt(uint8_t uxn_sock_opt) {
    switch (uxn_sock_opt) {
        case 0x02: 
            return SO_REUSEADDR;
        case 0x03: 
            return SO_TYPE; 
        case 0x04: 
            return SO_ERROR;
        case 0x05: 
            return SO_DONTROUTE;
        case 0x06: 
            return SO_BROADCAST;
        case 0x07: 
            return SO_SNDBUF;
        case 0x08: 
            return SO_RCVBUF;
        case 0x09: 
            return SO_KEEPALIVE;
        case 0x0a: 
            return SO_OOBINLINE;
        case 0x0d: 
            return SO_LINGER;
        case 0x12: 
            return SO_RCVLOWAT;
        case 0x13: 
            return SO_SNDLOWAT;
        case 0x1e: 
            return SO_ACCEPTCONN;
        default: 
            return 0;
    }
}

void net_setsockopt(void) {
#ifdef NET_DBG    
    printf("DBG: Setting socket option\n");
#endif 
    uint8_t yn = uxn.dev[NET_DEVICE+NET_optval];
    int opt = uxn.dev[NET_DEVICE+NET_opt]; /* SO_REUSEADDR */
    int sfd = uxn.dev[NET_DEVICE];
/**** set the Reuse-Socket-Address option */  
#ifdef NET_DBG    
    printf("%d %d %d %d\n",sfd,SOL_SOCKET,opt, yn);
#endif 
    if (setsockopt(sfd, SOL_SOCKET, sockopt(opt), &yn, sizeof(&yn))==-1) {
#ifdef NET_DBG    
        printf("DBG: Setting socket option %d = %d on socket %d failed\n",opt,yn,sfd);
#endif 
        perror("setsockopt");
        close(sfd);
        uxn.dev[NET_DEVICE+NET_status] = 0xFF;
    } else {
        uxn.dev[NET_DEVICE+NET_status] = 0x00;
    }
}

void net_getsockopt(void) {
#ifdef NET_DBG    
        printf("DBG: Getting socket option");        
#endif 
    perror("TODO");
}

void net_bind(void) {
#ifdef NET_DBG    
        printf("DBG: Binding socket");
#endif 

    int sfd = uxn.dev[NET_DEVICE];
#ifdef NET_DBG    
        printf(" %d ", sfd);
#endif 

    unsigned short addr_ptr = *((unsigned short*)(&uxn.dev[NET_DEVICE+NET_addr]));
#ifdef NET_DBG    
        printf("Uxn addr: %d\n", addr_ptr);
#endif
    in_addr_t addr = *(in_addr_t*)(&uxn.ram[addr_ptr]);
    in_port_t port = *((in_port_t*)(&uxn.ram[addr_ptr+4]));

    /**** build server socket address */
    struct sockaddr_in my_addr;
    bzero((char*) &my_addr, sizeof(struct sockaddr_in));
    my_addr.sin_family = AF_INET;         
    my_addr.sin_addr.s_addr = htonl(addr); 
    my_addr.sin_port = htons(port);
#ifdef NET_DBG    
        printf("to %s:%d\n",inet_ntoa(my_addr.sin_addr),port);
#endif 

    /**** bind server socket to the local address */     
    if (bind(sfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
#ifdef NET_DBG    
        printf("DBG: Binding socket %d to %s:%d failed\n",sfd,inet_ntoa(my_addr.sin_addr),port);
#endif 
        perror("bind");
        close(sfd);
        uxn.dev[NET_DEVICE+NET_status] =0xFF;
    } else {
        uxn.dev[NET_DEVICE+NET_status] =0x00;
    }
}

void net_connect(void) {
#ifdef NET_DBG
    printf("DBG: Connecting to host\n");
#endif

    int sfd = uxn.dev[NET_DEVICE];
    unsigned short addr_ptr = *((unsigned short*)(&uxn.dev[NET_DEVICE+NET_addr]));
    in_addr_t addr = *(in_addr_t*)(&uxn.ram[addr_ptr]);
    in_port_t port = *((in_port_t*)(&uxn.ram[addr_ptr+4]));

/**** build server socket address */        
    struct sockaddr_in sv_addr; 
    bzero((char*) &sv_addr, sizeof(struct sockaddr_in));
    sv_addr.sin_family = AF_INET;         
    sv_addr.sin_addr.s_addr = htonl(addr); 
    sv_addr.sin_port = htons(port);

  if (connect(sfd, (struct sockaddr *)&sv_addr, sizeof(struct sockaddr)) == -1) {
#ifdef NET_DBG
    printf("DBG: Connecting to %s:%d failed\n", inet_ntoa(sv_addr.sin_addr), port);
#endif
    perror("connect");
    close(sfd);
    uxn.dev[NET_DEVICE+NET_status] =0xFF;
  } else {
    uxn.dev[NET_DEVICE+NET_status] =0x00;
  }
}

void net_listen(void) {

    int sfd = uxn.dev[NET_DEVICE];
#ifdef NET_DBG
    unsigned short addr_ptr = *((unsigned short*)(&uxn.dev[NET_DEVICE+NET_addr]));
    in_addr_t addr = *(in_addr_t*)(&uxn.ram[addr_ptr]);
    in_port_t port = *((in_port_t*)(&uxn.ram[addr_ptr+4]));
/**** build server socket address */        
    struct sockaddr_in sv_addr; 
    bzero((char*) &sv_addr, sizeof(struct sockaddr_in));
    sv_addr.sin_family = AF_INET;         
    sv_addr.sin_addr.s_addr = htonl(addr); 
    sv_addr.sin_port = htons(port);

    printf("DBG: Listen on %s:%d\n", inet_ntoa(sv_addr.sin_addr),port);
#endif
    if (listen(sfd, 32) == -1) {
#ifdef NET_DBG
        printf("DBG: Listen failed\n");
#endif
        perror("listen");
        close(sfd);
        uxn.dev[NET_DEVICE+NET_status] =0xFF;
    } else {
        uxn.dev[NET_DEVICE+NET_status] =0x00;
    }   

}

void net_accept(void) {
#ifdef NET_DBG
    printf("DBG: Accepting connection\n");
#endif
/*
 * accept connection and read data until EOF, copying to standard output
 */
    int sfd = uxn.dev[NET_DEVICE];
    int cfd;
    struct sockaddr_in their_addr;
    socklen_t sin_size = sizeof(struct sockaddr_in);
    if ((cfd = accept(sfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
        perror("accept");
        close(sfd);
        uxn.dev[NET_DEVICE+NET_status] =0xFF;
    } else {
        /* We must store the cfd and the their_addr information */
#ifdef NET_DBG
        printf("s:%d,c:%d\n",sfd,cfd);
#endif
        uxn.dev[NET_DEVICE] = (Uint8)cfd;
        unsigned short addr_ptr = *((unsigned short*)(&uxn.dev[NET_DEVICE+NET_addr]));
        *(in_addr_t*)(&uxn.ram[addr_ptr]) = their_addr.sin_addr.s_addr;
        *((in_port_t*)(&uxn.ram[addr_ptr+4])) = their_addr.sin_port;
        uxn.dev[NET_DEVICE+NET_status] =0x00;
    }
}

/* ssize_t recv(int sockfd, void *buf, size_t len, int flags); */

void net_recv(void) { 
#ifdef NET_DBG
    printf("DBG: Receiving message\n");
#endif
    int sfd = uxn.dev[NET_DEVICE];
    unsigned short buf_ptr = *((unsigned short*)(&uxn.dev[NET_DEVICE+NET_buf]));
    void* buf = (void*)(&uxn.ram[buf_ptr]);
    size_t len = *(unsigned short*)(&uxn.dev[NET_DEVICE+NET_len]);
    int flags = uxn.dev[NET_DEVICE+NET_flags];
    ssize_t n = recv(sfd, buf, len, flags);
    *((unsigned short*)(&uxn.dev[NET_DEVICE+NET_len])) = (unsigned short)n;
}

void net_send(void) { 
#ifdef NET_DBG
    printf("DBG: Receiving message\n");
#endif
    int sfd = uxn.dev[NET_DEVICE];
    unsigned short buf_ptr = *((unsigned short*)(&uxn.dev[NET_DEVICE+NET_buf]));
    void* buf = (void*)(&uxn.ram[buf_ptr]);
    size_t len = *(unsigned short*)(&uxn.dev[NET_DEVICE+NET_len]);
    int flags = uxn.dev[NET_DEVICE+NET_flags];
    ssize_t n = send(sfd, buf, len, flags);
    *((unsigned short*)(&uxn.dev[NET_DEVICE+NET_len])) = (unsigned short)n;
}

/* TODO ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen); */
/* TODO ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags); */


void net_inet_addr(void) {
    unsigned short addr_ptr = *((unsigned short*)(&uxn.dev[INET_DEVICE+INET_addr]));
    unsigned short hostname_ptr = *((unsigned short*)(&uxn.dev[INET_DEVICE+INET_hostname]));
    char *cp = (char*)(&uxn.ram[hostname_ptr]);
#ifdef NET_DBG
    printf("DBG:ip addr str:%s\n",cp);
#endif
    in_addr_t addr = inet_addr(cp);
    *((in_addr_t*)(&uxn.ram[addr_ptr])) = addr;
}

void net_inet_ntoa(void) {
    unsigned short addr_ptr = *((unsigned short*)(&uxn.dev[INET_DEVICE+INET_addr]));
    unsigned short hostname_ptr = *((unsigned short*)(&uxn.dev[INET_DEVICE+INET_hostname]));
    in_addr_t addr = *((in_addr_t*)(&uxn.ram[addr_ptr]));
    struct in_addr in_addr_struct;
    bzero((char*) &in_addr_struct, sizeof(struct in_addr));
    in_addr_struct.s_addr=addr;
    char *cp = inet_ntoa(in_addr_struct);
    /* I don't think this can work, use memcpy! */
    memcpy(&uxn.ram[hostname_ptr],cp,16);
}


void net_select(void) {
    unsigned short timeout = *((unsigned short*)(&uxn.dev[SELECT_DEVICE+SELECT_timeout]));
    struct timeval timeout_struct;
    timeout_struct.tv_sec = (time_t)(timeout/1000);
    timeout_struct.tv_usec = (suseconds_t)((timeout-timeout_struct.tv_sec)*1000);

    int nfds = *((int*)(&uxn.dev[SELECT_DEVICE+SELECT_nfds]));
    unsigned short fdsets_ptr = *((unsigned short*)(&uxn.dev[SELECT_DEVICE+SELECT_fdsets]));

    fd_set* readfds = (fd_set*)(&uxn.ram[fdsets_ptr]);
    fd_set* writefds = (fd_set*)(&uxn.ram[fdsets_ptr+16]);
    fd_set* exceptfds = (fd_set*)(&uxn.ram[fdsets_ptr+32]);

    int status = select(nfds, readfds, writefds,
                  exceptfds, &timeout_struct);
    uxn.dev[SELECT_DEVICE+NET_status]= status == -1 ? 0xff : (Uint8)status;
}

/* 
The only case where we need to take action is if the addr is .Network/call
We execute the corresponding wrapper for the POSIX call
*/
void network_deo(Uint8 addr) {
    uint8_t rel_addr = addr - NET_DEVICE ;
#ifdef NET_DBG    
    printf("DBG: network_deo %d - %d = %d val %d \n",addr,NET_DEVICE,rel_addr,uxn.dev[addr]);
#endif
    if (rel_addr == NET_call) {    
        Uint8 value = uxn.dev[addr];
        switch (value) {
            case NET_CMD_SOCKET:
                net_socket();
                break;
            case NET_CMD_SETSOCKOPT:
                net_setsockopt();
                break;
            case NET_CMD_GETSOCKOPT:
                net_getsockopt();
                break;
            case NET_CMD_BIND:
                net_bind();
                break;
            case NET_CMD_LISTEN:
                net_listen();
                break;       
            case NET_CMD_ACCEPT:
                net_accept();
                break;                 
            case NET_CMD_CONNECT:
                net_connect();
                break;          
            case NET_CMD_RECV:
                net_recv();
                break;              
            case NET_CMD_SEND:
                net_send();
                break;              
            case NET_CMD_CLOSE:
                net_close();
                break;              
            case NET_CMD_INET_ADDR:
                net_inet_addr();
                break;
            case NET_CMD_INET_NTOA:
                net_inet_ntoa();
                break;              
            case NET_CMD_SELECT:
                net_select();
                break;
            default:
                break;              
        }
    }
}


Uint8 network_dei(Uint8 addr) {
#ifdef NET_DBG    
    uint8_t rel_addr = addr - NET_DEVICE ;
    printf("DBG: network_dei %d - %d = %d val %d \n",addr,NET_DEVICE,rel_addr,uxn.dev[addr]);
#endif
    return uxn.dev[addr];
}
