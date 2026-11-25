#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#ifdef EXTENSION
#include <inttypes.h>

#ifdef EXT_NETWORKING
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef EXT_TLS
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"
#include "psa/crypto.h"

/* for  bzero */
#include <strings.h>
/* for inet_ntoa */
#include <arpa/inet.h>
/* for select */
#include <sys/select.h>
#endif
#endif

/*
Copyright (c) 2021-2026 Devine Lu Linvega, Andrew Alderwick,
Andrew Richards, Eiríkr Åsheim, Sigrid Solveig Haflínudóttir

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.

cc -DNDEBUG -O2 -g0 -s src/uxncli.c -o bin/uxncli
*/

typedef signed char Sint8;
typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef void (*deo_handler)(void);
typedef Uint8 (*dei_handler)(void);

#ifndef EXTENSION
#define DEV_SZ 0x100
#else
#define DEV_SZ 0x200
#endif
static Uint8 *ram, dev[DEV_SZ], ptr[2], stk[2][0x100];

static unsigned int uxn_eval(Uint16 pc);
static int rL1, rL2;

/* clang-format off */

#define BANKS 0x10
#define BANKS_CAP (BANKS * 0x10000)

static inline Uint16 peek2(const Uint8 *d) { return ((Uint16)d[0] << 8) | d[1]; }
static inline void poke2(Uint8 *d, Uint16 v) { d[0] = v >> 8, d[1] = v; }

/* clang-format on */
#ifdef EXTENSION
// #define EXT_DBG
/*
@|Extension ------------------------------------------------------------ */


#ifdef EXT_NETWORKING	    
void network_deo(Uint8);
#endif

#ifdef EXT_TLS
void tls_deo(Uint8);
#endif

#define EXT_DEVICE 0xd0
#define EXT_DEV_OFFSET 0x100
static uint8_t selected_ext = 0;

static void extension_deo(uint8_t addr) {
	uint8_t rel_addr = addr - EXT_DEVICE ;
	if (rel_addr == 0x0f) {
		 selected_ext = dev[addr];
#ifdef EXT_DBG		 
		 printf("Selected %d from device address %d\n",selected_ext,addr);
#endif		 
	} else {
        uint16_t ext_addr = EXT_DEV_OFFSET+(selected_ext-1)*16+rel_addr;
	    dev[ext_addr] = dev[addr];
	    switch (selected_ext) {
#ifdef EXT_NETWORKING	    
		    case 0x01: network_deo(rel_addr); break;
#endif	
#ifdef EXT_TLS	    
		    case 0x05: tls_deo(rel_addr); break;
#endif		    
/*		    case 0x02: parallel_deo(addr); break;
		    case 0x03: threads_deo(addr); break;
		    case 0x04: process_deo(addr); break;
		    case 0x05: callstack_deo(addr); break;
		    case 0x06: extmem_deo(addr); break 
		    default: return; */
	    }
	}
#ifdef EXT_DBG
	printf("DBG EXT DEO: %d,%d,%d\n",addr, rel_addr,EXT_DEV_OFFSET+(selected_ext-1)*16+rel_addr);
#endif	
}

static Uint8 extension_dei(uint8_t addr) {
	uint8_t rel_addr = addr - EXT_DEVICE ;
    dev[addr] = dev[EXT_DEV_OFFSET+(selected_ext-1)*16+rel_addr];
#ifdef EXT_DBG	
	printf("DBG EXT DEI: %d,%d,%d\n",addr, rel_addr,EXT_DEV_OFFSET+(selected_ext-1)*16+rel_addr);
#endif	
	return dev[addr];
}


#ifdef EXT_NETWORKING
// #define NET_DBG
/*
@|Networking ------------------------------------------------------------ */

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
#define NET_DEVICE EXT_DEV_OFFSET+0
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

/* POSIX call wrappers */
void net_close(void) {
#ifdef NET_DBG
    printf("DBG: Closing socket\n");
#endif

    uint8_t sfd = dev[NET_DEVICE+NET_sfd];
    int status = close((int)sfd);
    dev[NET_DEVICE+NET_status]=(uint8_t)status;
}

void net_socket(void) {
#ifdef NET_DBG
    printf("DBG: Creating socket\n");
#endif
    int sfd = 0;
    uint8_t type = dev[NET_DEVICE+NET_type];
    if ((sfd=socket(AF_INET, (type==1 ? SOCK_STREAM : SOCK_DGRAM), 0)) == -1) {
#ifdef NET_DBG    
        printf("DBG: Socket creation failed\n");
#endif        
        dev[NET_DEVICE+NET_status]= 0xFF; /* -1 in 2's complement, 1 byte */
    } else {
#ifdef NET_DBG
    printf("DBG: Socket of type %d created successfully, fd=%d\n", type,sfd);
#endif
        dev[NET_DEVICE+NET_sfd]=(uint8_t)sfd;
        dev[NET_DEVICE+NET_status]=(uint8_t)sfd;
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
    uint8_t yn = dev[NET_DEVICE+NET_optval];
    int opt = dev[NET_DEVICE+NET_opt]; /* SO_REUSEADDR */
    int sfd = dev[NET_DEVICE];
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
        dev[NET_DEVICE+NET_status] = 0xFF;
    } else {
        dev[NET_DEVICE+NET_status] = 0x00;
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

    int sfd = dev[NET_DEVICE];
#ifdef NET_DBG    
        printf(" %d ", sfd);
#endif 

    unsigned short addr_ptr = *((unsigned short*)(&dev[NET_DEVICE+NET_addr]));
#ifdef NET_DBG    
        printf("Uxn addr: %d\n", addr_ptr);
#endif
    in_addr_t addr = *(in_addr_t*)(&ram[addr_ptr]);
    in_port_t port = *((in_port_t*)(&ram[addr_ptr+4]));

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
        dev[NET_DEVICE+NET_status] =0xFF;
    } else {
        dev[NET_DEVICE+NET_status] =0x00;
    }
}

void net_connect(void) {
#ifdef NET_DBG
    printf("DBG: Connecting to host\n");
#endif

    int sfd = dev[NET_DEVICE];
    unsigned short addr_ptr = *((unsigned short*)(&dev[NET_DEVICE+NET_addr]));
    in_addr_t addr = *(in_addr_t*)(&ram[addr_ptr]);
    in_port_t port = *((in_port_t*)(&ram[addr_ptr+4]));

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
    dev[NET_DEVICE+NET_status] =0xFF;
  } else {
    dev[NET_DEVICE+NET_status] =0x00;
  }
}

void net_listen(void) {

    int sfd = dev[NET_DEVICE];
#ifdef NET_DBG
    unsigned short addr_ptr = *((unsigned short*)(&dev[NET_DEVICE+NET_addr]));
    in_addr_t addr = *(in_addr_t*)(&ram[addr_ptr]);
    in_port_t port = *((in_port_t*)(&ram[addr_ptr+4]));
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
        dev[NET_DEVICE+NET_status] =0xFF;
    } else {
        dev[NET_DEVICE+NET_status] =0x00;
    }   

}

void net_accept(void) {
#ifdef NET_DBG
    printf("DBG: Accepting connection\n");
#endif
/*
 * accept connection and read data until EOF, copying to standard output
 */
    int sfd = dev[NET_DEVICE];
    int cfd;
    struct sockaddr_in their_addr;
    socklen_t sin_size = sizeof(struct sockaddr_in);
    if ((cfd = accept(sfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
        perror("accept");
        close(sfd);
        dev[NET_DEVICE+NET_status] =0xFF;
    } else {
        /* We must store the cfd and the their_addr information */
#ifdef NET_DBG
        printf("s:%d,c:%d\n",sfd,cfd);
#endif
        dev[NET_DEVICE] = (Uint8)cfd;
        unsigned short addr_ptr = *((unsigned short*)(&dev[NET_DEVICE+NET_addr]));
        *(in_addr_t*)(&ram[addr_ptr]) = their_addr.sin_addr.s_addr;
        *((in_port_t*)(&ram[addr_ptr+4])) = their_addr.sin_port;
        dev[NET_DEVICE+NET_status] =0x00;
    }
}

/* ssize_t recv(int sockfd, void *buf, size_t len, int flags); */

void net_recv(void) { 
#ifdef NET_DBG
    printf("DBG: Receiving message\n");
#endif
    int sfd = dev[NET_DEVICE];
    unsigned short buf_ptr = *((unsigned short*)(&dev[NET_DEVICE+NET_buf]));
    void* buf = (void*)(&ram[buf_ptr]);
    size_t len = *(unsigned short*)(&dev[NET_DEVICE+NET_len]);
    int flags = dev[NET_DEVICE+NET_flags];
    ssize_t n = recv(sfd, buf, len, flags);
    *((unsigned short*)(&dev[NET_DEVICE+NET_len])) = (unsigned short)n;
}

void net_send(void) { 
#ifdef NET_DBG
    printf("DBG: Receiving message\n");
#endif
    int sfd = dev[NET_DEVICE];
    unsigned short buf_ptr = *((unsigned short*)(&dev[NET_DEVICE+NET_buf]));
    void* buf = (void*)(&ram[buf_ptr]);
    size_t len = *(unsigned short*)(&dev[NET_DEVICE+NET_len]);
    int flags = dev[NET_DEVICE+NET_flags];
    ssize_t n = send(sfd, buf, len, flags);
    *((unsigned short*)(&dev[NET_DEVICE+NET_len])) = (unsigned short)n;
}

/* TODO ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen); */
/* TODO ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags); */


void net_inet_addr(void) {
    unsigned short addr_ptr = *((unsigned short*)(&dev[INET_DEVICE+INET_addr]));
    unsigned short hostname_ptr = *((unsigned short*)(&dev[INET_DEVICE+INET_hostname]));
    char *cp = (char*)(&ram[hostname_ptr]);
#ifdef NET_DBG
    printf("DBG:ip addr str:%s\n",cp);
#endif
    in_addr_t addr = inet_addr(cp);
    *((in_addr_t*)(&ram[addr_ptr])) = addr;
}

void net_inet_ntoa(void) {
    unsigned short addr_ptr = *((unsigned short*)(&dev[INET_DEVICE+INET_addr]));
    unsigned short hostname_ptr = *((unsigned short*)(&dev[INET_DEVICE+INET_hostname]));
    in_addr_t addr = *((in_addr_t*)(&ram[addr_ptr]));
    struct in_addr in_addr_struct;
    bzero((char*) &in_addr_struct, sizeof(struct in_addr));
    in_addr_struct.s_addr=addr;
    char *cp = inet_ntoa(in_addr_struct);
    /* I don't think this can work, use memcpy! */
    memcpy(&ram[hostname_ptr],cp,16);
}


void net_select(void) {
    unsigned short timeout = *((unsigned short*)(&dev[SELECT_DEVICE+SELECT_timeout]));
    struct timeval timeout_struct;
    timeout_struct.tv_sec = (time_t)(timeout/1000);
    timeout_struct.tv_usec = (suseconds_t)((timeout-timeout_struct.tv_sec)*1000);

    int nfds = *((int*)(&dev[SELECT_DEVICE+SELECT_nfds]));
    unsigned short fdsets_ptr = *((unsigned short*)(&dev[SELECT_DEVICE+SELECT_fdsets]));

    fd_set* readfds = (fd_set*)(&ram[fdsets_ptr]);
    fd_set* writefds = (fd_set*)(&ram[fdsets_ptr+16]);
    fd_set* exceptfds = (fd_set*)(&ram[fdsets_ptr+32]);

    int status = select(nfds, readfds, writefds,
                  exceptfds, &timeout_struct);
    dev[SELECT_DEVICE+NET_status]= status == -1 ? 0xff : (Uint8)status;
}

/* 
The only case where we need to take action is if the addr is .Network/call
We execute the corresponding wrapper for the POSIX call
*/
void network_deo(Uint8 rel_addr) {
//    uint8_t rel_addr = (uint8_t)(addr - NET_DEVICE) ;
    uint16_t addr = NET_DEVICE + rel_addr;
#ifdef NET_DBG    
    printf("DBG: network_deo %d val %d \n",rel_addr,dev[addr]);
#endif
    if (rel_addr == NET_call) {    
        Uint8 value = dev[addr];
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
#endif
#endif


#ifdef EXT_TLS

#define TLS_CMD_INIT       0x01
#define TLS_CMD_BIND       0x02
#define TLS_CMD_ACCEPT     0x03
#define TLS_CMD_HANDSHAKE  0x04
#define TLS_CMD_WRITE      0x05
#define TLS_CMD_READ       0x06
#define TLS_CMD_CLOSE      0x07
#define TLS_CMD_CONNECT    0x08

#define TLS_DEVICE (EXT_DEV_OFFSET + 0x40)

#define TLS_mode      0x00
#define TLS_buf       0x01
#define TLS_len       0x03
#define TLS_hostname  0x05
#define TLS_status    0x07
#define TLS_call      0x09
#define TLS_port      0x0a

static mbedtls_net_context listen_fd, client_fd;
static mbedtls_ssl_context ssl;
static mbedtls_ssl_config conf;
static mbedtls_x509_crt cert, ca;
static mbedtls_pk_context key;

static int tls_initialized = 0;
static int tls_server;

static void tls_free(void)
{
    if (!tls_initialized)
        return;

    mbedtls_net_free(&client_fd);
    mbedtls_net_free(&listen_fd);

    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);

    mbedtls_x509_crt_free(&cert);
    mbedtls_x509_crt_free(&ca);
    mbedtls_pk_free(&key);

    tls_initialized = 0;
}


void tls_init(void)
{
	printf("TLS init\n");
	
    Uint16 cert_addr = peek2(dev + TLS_DEVICE + TLS_buf);
    Uint16 key_addr = peek2(dev + TLS_DEVICE + TLS_len);

    const char *cert_path = (const char *)(ram + cert_addr);
    const char *key_path = (const char *)(ram + key_addr);

    Uint8 mode = dev[TLS_DEVICE + TLS_mode];

    poke2(dev + TLS_DEVICE + TLS_status, 0xffff);

    if (mode > 1)
        return;

    if (!memchr(cert_path, 0, 0x10000u - cert_addr))
        return;

    if (mode == 0 &&
        !memchr(key_path, 0, 0x10000u - key_addr))
        return;

    tls_free();

    tls_server = mode == 0;

    if (psa_crypto_init() != PSA_SUCCESS)
        return;

    mbedtls_net_init(&listen_fd);
    mbedtls_net_init(&client_fd);

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);

    mbedtls_x509_crt_init(&cert);
    mbedtls_x509_crt_init(&ca);
    mbedtls_pk_init(&key);

    tls_initialized = 1;

    if (mbedtls_ssl_config_defaults(
        &conf,
        tls_server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT) != 0)
    {
        goto exit;
    }

    if (tls_server)
    {
        if (mbedtls_x509_crt_parse_file(&cert, cert_path) != 0)
            goto exit;

        if (mbedtls_pk_parse_keyfile(&key, key_path, NULL) != 0)
            goto exit;

        if (mbedtls_ssl_conf_own_cert(&conf, &cert, &key) != 0)
            goto exit;
    }
    else
    {
        if (mbedtls_x509_crt_parse_file(&ca, cert_path) != 0)
            goto exit;

        mbedtls_ssl_conf_ca_chain(&conf, &ca, NULL);
    }

    if (mbedtls_ssl_setup(&ssl, &conf) != 0)
        goto exit;

    poke2(dev + TLS_DEVICE + TLS_status, 0x0000);

    return;

exit:
    tls_free();
}


void tls_bind(void)
{
    char port[6];
    int ret;

    if (!tls_initialized || !tls_server)
    {
        poke2(dev + TLS_DEVICE + TLS_status, 0xffff);
        return;
    }

    snprintf(port, sizeof(port), "%u", (unsigned)peek2(dev + TLS_DEVICE + TLS_port));

    ret = mbedtls_net_bind( &listen_fd, NULL, port, MBEDTLS_NET_PROTO_TCP);

    if (ret != 0)
        poke2(dev + TLS_DEVICE + TLS_status, 0xffff);
    else
        poke2(dev + TLS_DEVICE + TLS_status, 0x0000);
}


void tls_accept(void)
{
    int ret;

    poke2(dev + TLS_DEVICE + TLS_status, 0xffff);

    if (!tls_initialized || !tls_server)
        return;

    if (mbedtls_ssl_session_reset(&ssl) != 0)
        return;

    mbedtls_net_free(&client_fd);

    ret = mbedtls_net_accept(&listen_fd, &client_fd, NULL, 0, NULL);

    if (ret != 0)
        return;

    mbedtls_ssl_set_bio( &ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    poke2(dev + TLS_DEVICE + TLS_status, 0x0000);
}


void tls_connect(void)
{
    int ret;
    char port[6];

    Uint16 addr = peek2(dev + TLS_DEVICE + TLS_hostname);
    const char *host = (const char *)(ram + addr);

    poke2(dev + TLS_DEVICE + TLS_status, 0xffff);

    if (!tls_initialized || tls_server)
        return;

    if (!addr || !*host || !memchr(host, 0, 0x10000u - addr))
        return;

    snprintf(port, sizeof(port), "%u", (unsigned)peek2(dev + TLS_DEVICE + TLS_port));

    if (mbedtls_ssl_session_reset(&ssl) != 0)
        return;

    if (mbedtls_ssl_set_hostname(&ssl, host) != 0)
        return;

    mbedtls_net_free(&client_fd);

    ret = mbedtls_net_connect( &client_fd, host, port, MBEDTLS_NET_PROTO_TCP);

    if (ret != 0)
        return;

    mbedtls_ssl_set_bio( &ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    poke2(dev + TLS_DEVICE + TLS_status, 0x0000);
}


void tls_handshake(void)
{
    int ret;

    poke2(dev + TLS_DEVICE + TLS_status, 0xffff);

    if (!tls_initialized)
        return;

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            tls_free();
            return;
        }
    }

    poke2(dev + TLS_DEVICE + TLS_status, 0x0000);
}


void tls_write(void)
{
    int ret;

    Uint16 addr = peek2(dev + TLS_DEVICE + TLS_buf);
    Uint16 len = peek2(dev + TLS_DEVICE + TLS_len);

    poke2(dev + TLS_DEVICE + TLS_status, 0xffff);
    poke2(dev + TLS_DEVICE + TLS_len, 0);

    if (!tls_initialized)
        return;

    if ((unsigned)addr + len > 0x10000u)
        return;

    do
    {
        ret = mbedtls_ssl_write(&ssl, ram + addr, len);
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret < 0)
    {
        tls_free();
        return;
    }

    poke2(dev + TLS_DEVICE + TLS_len, (Uint16)ret);
    poke2(dev + TLS_DEVICE + TLS_status, 0x0000);
}


void tls_read(void)
{
    int ret;

    Uint16 addr = peek2(dev + TLS_DEVICE + TLS_buf);
    Uint16 len = peek2(dev + TLS_DEVICE + TLS_len);

    poke2(dev + TLS_DEVICE + TLS_status, 0xffff);
    poke2(dev + TLS_DEVICE + TLS_len, 0);

    if (!tls_initialized)
        return;

    if ((unsigned)addr + len > 0x10000u)
        return;

    do
    {
        ret = mbedtls_ssl_read(&ssl, ram + addr, len);
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0)
    {
        tls_free();
        poke2(dev + TLS_DEVICE + TLS_status, 0x0001);
        return;
    }

    if (ret < 0)
    {
        tls_free();
        return;
    }

    poke2(dev + TLS_DEVICE + TLS_len, (Uint16)ret);
    poke2(dev + TLS_DEVICE + TLS_status, 0x0000);
}


void tls_close(void)
{
    int ret = 0;

    if (tls_initialized)
    {
        do
        {
            ret = mbedtls_ssl_close_notify(&ssl);
        }
        while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

        tls_free();
    }

    if (ret != 0)
        poke2(dev + TLS_DEVICE + TLS_status, 0xffff);
    else
        poke2(dev + TLS_DEVICE + TLS_status, 0x0000);
}


void tls_deo(Uint8 rel_addr) {
    uint16_t addr = TLS_DEVICE + rel_addr;

    if (rel_addr == TLS_call) {
        Uint8 value = dev[addr];

        switch (value) {
            case TLS_CMD_INIT:
                tls_init();
                break;
            case TLS_CMD_BIND:
                tls_bind();
                break;
            case TLS_CMD_ACCEPT:
                tls_accept();
                break;
            case TLS_CMD_CONNECT:
                tls_connect();
                break;
            case TLS_CMD_HANDSHAKE:
                tls_handshake();
                break;
            case TLS_CMD_WRITE:
                tls_write();
                break;
            case TLS_CMD_READ:
                tls_read();
                break;
            case TLS_CMD_CLOSE:
                tls_close();
                break;
            default:
                poke2(dev + TLS_DEVICE + TLS_status, 0xffff);
                break;
        }
		printf("TLS cmd: %02x, status: %04x\n", value, peek2(dev + TLS_DEVICE + TLS_status));
    }
}

#endif
#endif


/* 
@|System ------------------------------------------------------------ */

static char *system_boot_path;

static void
system_print(char *name, int r)
{
	Uint8 i;
	fprintf(stderr, "%s%c", name, ptr[r] - 8 ? ' ' : '|');
	for(i = ptr[r] - 8; i != ptr[r]; i++)
		fprintf(stderr, "%02x%c", stk[r][i], i == 0xff ? '|' : ' ');
	fprintf(stderr, "<%02x\n", ptr[r]);
}

static unsigned int
system_load(const char *rom_path)
{
	FILE *f = fopen(rom_path, "rb");
	if(f) {
		unsigned int i = 0, l = fread(ram + 0x100, 0x10000 - 0x100, 1, f);
		while(l && ++i < BANKS)
			l = fread(ram + i * 0x10000, 0x10000, 1, f);
		fclose(f);
	}
	return !!f;
}

static unsigned int
system_boot(char *rom_path, const unsigned int has_args)
{
	ram = (Uint8 *)calloc(BANKS_CAP, sizeof(Uint8));
	system_boot_path = rom_path;
	dev[0x17] = has_args;
	return ram && system_load(rom_path);
}

static void
system_deo_expansion(void)
{
	const Uint16 exp = peek2(dev + 2);
	Uint8 *aptr = ram + exp;
	Uint16 length = peek2(aptr + 1);
	unsigned int bank = peek2(aptr + 3) * 0x10000;
	unsigned int addr = peek2(aptr + 5);
	if(ram[exp] == 0x0) {
		if(bank < BANKS_CAP)
			memset(ram + bank + addr, ram[exp + 7], length);
	} else if(ram[exp] == 1 || ram[exp] == 2) {
		unsigned int dst_bank = peek2(aptr + 7) * 0x10000;
		unsigned int dst_addr = peek2(aptr + 9);
		if(bank < BANKS_CAP && dst_bank < BANKS_CAP)
			memmove(ram + dst_bank + dst_addr, ram + bank + addr, length);
	} else
		fprintf(stderr, "Unknown command: %02x\n", ram[exp]);
}

/* clang-format off */

static Uint8 system_dei_wst(void) { return ptr[0]; }
static Uint8 system_dei_rst(void) { return ptr[1]; }
static void system_deo_wst(void) { ptr[0] = dev[4]; }
static void system_deo_rst(void) { ptr[1] = dev[5]; }
static void system_deo_print(void) { system_print("WST", 0), system_print("RST", 1); }

/* clang-format on */

/*
@|Console ----------------------------------------------------------- */

#define CONSOLE_STD 0x1
#define CONSOLE_ARG 0x2
#define CONSOLE_EOA 0x3
#define CONSOLE_END 0x4

static unsigned int console_vector;

static void
console_input(int c, unsigned int type)
{
	dev[0x12] = c, dev[0x17] = type;
	if(console_vector && !dev[0x0f])
		uxn_eval(console_vector);
}

/* clang-format off */

static void console_deo_vector(void) { console_vector = peek2(&dev[0x10]); }
static void console_deo_stdout(void) { fputc(dev[0x18], stdout); }
static void console_deo_stderr(void) { fputc(dev[0x19], stderr); }
static void console_deo_hb(void) { fprintf(stderr, "%02x", dev[0x1a]); }
static void console_deo_lb(void) { fprintf(stderr, "%02x", dev[0x1b]); }

/* clang-format on */

/*
@|File -------------------------------------------------------------- */

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

typedef struct {
	FILE *f;
	DIR *dir;
	char *filepath;
	enum { IDLE,
		FILE_READ,
		FILE_WRITE,
		DIR_READ,
		DIR_WRITE
	} state;
} UxnFile;

static UxnFile ufs[2];

static void
make_pathfile(char *pathbuf, const char *filepath, const char *basename)
{
	char c = '/';
	while(*filepath)
		c = *filepath++, *pathbuf = c, pathbuf++;
	if(c != '/')
		*pathbuf = '/', pathbuf++;
	while(*basename)
		*pathbuf = *basename++, pathbuf++;
	*pathbuf = 0;
}

static inline unsigned int
put_fill(Uint8 *dest, unsigned int len, char c)
{
	memset(dest, c, len);
	return len;
}

static inline unsigned int
put_size(Uint8 *dest, unsigned int len, unsigned int size)
{
	unsigned int i;
	for(i = 0, dest += len; i < len; i++, size >>= 4)
		*(--dest) = "0123456789abcdef"[(Uint8)(size & 0xf)];
	return len;
}

static inline unsigned int
put_text(Uint8 *dest, const char *text)
{
	Uint8 *anchor = dest;
	while(*text)
		*dest = *text++, dest++;
	*dest = 0;
	return dest - anchor;
}

static unsigned int
put_stat(Uint8 *dest, unsigned int len, unsigned int size, unsigned int err, unsigned int dir, unsigned int capsize)
{
	if(err) return put_fill(dest, len, '!');
	if(dir) return put_fill(dest, len, '-');
	if(capsize && size >= 0x10000) return put_fill(dest, len, '?');
	return put_size(dest, len, size);
}

static unsigned int
put_statfile(Uint8 *dest, const char *filepath, const char *basename)
{
	unsigned int err, dir;
	struct stat st;
	Uint8 *anchor = dest;
	char pathbuf[0x200];
	make_pathfile(pathbuf, filepath, basename);
	err = stat(pathbuf, &st);
	dir = !err && S_ISDIR(st.st_mode);
	dest += put_stat(dest, 4, st.st_size, err, dir, 1);
	dest += put_text(dest, " ");
	dest += put_text(dest, basename);
	dest += put_text(dest, dir ? "/\n" : "\n");
	return dest - anchor;
}

static unsigned int
put_fdir(Uint8 *dest, unsigned int len, const char *filepath, DIR *dir)
{
	Uint8 *p = dest, *end = dest + len - 1;
	struct dirent *de;
	while((de = readdir(dir)) && p < end) {
		const char *name = de->d_name;
		if(name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2])))
			continue;
		p += put_statfile(p, filepath, name);
	}
	*p = 0;
	return p - dest;
}

static unsigned int
is_dir_path(const char *p)
{
	const char *end = p;
	while(*end) end++;
	return end > p && end[-1] == '/';
}

static unsigned int
is_dir_real(char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static unsigned int
file_write_dir(const char *p)
{
	char buf[0x200];
	char *s = buf;
	unsigned int ok = 1;
	while(*p && ok) {
		*s++ = *p;
		if(*p++ == '/') {
			s[-1] = '\0';
			ok = is_dir_real(buf) || mkdir(buf, 0755) == 0;
			s[-1] = '/';
		}
	}
	return ok;
}

static void
file_reset(unsigned int id)
{
	if(ufs[id].f != NULL) fclose(ufs[id].f), ufs[id].f = NULL;
	if(ufs[id].dir != NULL) closedir(ufs[id].dir), ufs[id].dir = NULL;
	ufs[id].state = IDLE;
}

static unsigned int
file_init(unsigned int id, Uint16 addr)
{
	file_reset(id);
	ufs[id].filepath = (char *)&ram[addr];
	return 0;
}

static unsigned int
file_read(unsigned int id, Uint16 addr, unsigned int len)
{
	void *dest = &ram[addr];
	struct stat st;
	unsigned int n;
	if(ufs[id].filepath == 0)
		return 0;
	if(addr + len > 0x10000)
		len = 0x10000 - addr;
	if(ufs[id].state != FILE_READ && ufs[id].state != DIR_READ) {
		file_reset(id);
		if(stat(ufs[id].filepath, &st) == 0 && S_ISDIR(st.st_mode)) {
			if((ufs[id].dir = opendir(ufs[id].filepath)) != NULL)
				ufs[id].state = DIR_READ;
		} else if((ufs[id].f = fopen(ufs[id].filepath, "rb")) != NULL) {
			ufs[id].state = FILE_READ;
		}
	}
	if(ufs[id].state == FILE_READ) {
		n = fread(dest, 1, len, ufs[id].f);
		if(n == 0) file_reset(id);
		return n;
	}
	if(ufs[id].state == DIR_READ) {
		n = put_fdir(dest, len, ufs[id].filepath, ufs[id].dir);
		if(n == 0) file_reset(id);
		return n;
	}
	return 0;
}

static unsigned int
file_write(unsigned int id, Uint16 addr, unsigned int len, Uint8 flags)
{
	unsigned int ret = 0;
	if(ufs[id].filepath == 0)
		return 0;
	if(addr + len > 0x10000)
		len = 0x10000 - addr;
	if(ufs[id].state != FILE_WRITE && ufs[id].state != DIR_WRITE) {
		file_reset(id);
		file_write_dir(ufs[id].filepath);
		if(is_dir_path(ufs[id].filepath))
			ufs[id].state = DIR_WRITE;
		else if((ufs[id].f = fopen(ufs[id].filepath, (flags & 0x01) ? "ab" : "wb")) != NULL)
			ufs[id].state = FILE_WRITE;
	}
	if(ufs[id].state == FILE_WRITE)
		ret = fwrite(&ram[addr], 1, len, ufs[id].f);
	if(ufs[id].state == DIR_WRITE)
		ret = is_dir_real(ufs[id].filepath);
	return ret;
}

static unsigned int
file_stat(unsigned int id, Uint16 addr, unsigned int len)
{
	unsigned int err, dir;
	struct stat st;
	if(ufs[id].filepath == 0)
		return 0;
	if(addr + len > 0x10000)
		len = 0x10000 - addr;
	err = stat(ufs[id].filepath, &st);
	dir = !err && S_ISDIR(st.st_mode);
	return put_stat(&ram[addr], len, st.st_size, err, dir, 0);
}

static unsigned int
file_delete(unsigned int id)
{
	if(ufs[id].filepath == 0)
		return 0;
	return !unlink(ufs[id].filepath);
}

/* clang-format off */

static void filea_deo_stat(void) { poke2(&dev[0xa2], file_stat(0, peek2(&dev[0xa4]), rL1)); }
static void filea_deo_delete(void) { poke2(&dev[0xa2], file_delete(0)); }
static void filea_deo_name(void) { poke2(&dev[0xa2], file_init(0, peek2(&dev[0xa8]))); }
static void filea_deo_length(void) { rL1 = peek2(&dev[0xaa]); }
static void filea_deo_read(void) { poke2(&dev[0xa2], file_read(0, peek2(&dev[0xac]), rL1)); }
static void filea_deo_write(void) { poke2(&dev[0xa2], file_write(0, peek2(&dev[0xae]), rL1, dev[0xa7])); }
static void fileb_deo_stat(void) { poke2(&dev[0xb2], file_stat(1, peek2(&dev[0xb4]), rL2)); }
static void fileb_deo_delete(void) { poke2(&dev[0xb2], file_delete(1)); }
static void fileb_deo_name(void) { poke2(&dev[0xb2], file_init(1, peek2(&dev[0xb8]))); }
static void fileb_deo_length(void) { rL2 = peek2(&dev[0xba]); }
static void fileb_deo_read(void) { poke2(&dev[0xb2], file_read(1, peek2(&dev[0xbc]), rL2)); }
static void fileb_deo_write(void) { poke2(&dev[0xb2], file_write(1, peek2(&dev[0xbe]), rL2, dev[0xb7])); }

/* clang-format on */

/*
@|Datetime ---------------------------------------------------------- */

#include <time.h>

static int datetime_busy;
static time_t datetime_seconds;
static struct tm *datetime_t, datetime_zt = {0};

static void
datetime_update(void)
{
	if(!datetime_busy) {
		datetime_seconds = time(NULL);
		datetime_t = localtime(&datetime_seconds);
		if(datetime_t == NULL)
			datetime_t = &datetime_zt;
		datetime_busy = 1;
	}
}

/* clang-format off */

static Uint8 datetime_dei_yhb(void) { datetime_update(); return (datetime_t->tm_year + 1900) >> 8; }
static Uint8 datetime_dei_ylb(void) { datetime_update(); return (datetime_t->tm_year + 1900); }
static Uint8 datetime_dei_mon(void) { datetime_update(); return datetime_t->tm_mon; }
static Uint8 datetime_dei_day(void) { datetime_update(); return datetime_t->tm_mday; }
static Uint8 datetime_dei_hou(void) { datetime_update(); return datetime_t->tm_hour; }
static Uint8 datetime_dei_min(void) { datetime_update(); return datetime_t->tm_min; }
static Uint8 datetime_dei_sec(void) { datetime_update(); return datetime_t->tm_sec; }
static Uint8 datetime_dei_wday(void) { datetime_update(); return datetime_t->tm_wday; }
static Uint8 datetime_dei_ydayhb(void) { datetime_update(); return datetime_t->tm_yday >> 8; }
static Uint8 datetime_dei_ydaylb(void) { datetime_update(); return datetime_t->tm_yday; }
static Uint8 datetime_dei_dst(void) { datetime_update(); return datetime_t->tm_isdst; }

/* clang-format on */

/*
@|Core -------------------------------------------------------------- */

static const dei_handler dei_handlers[256] = {
	[0x04] = system_dei_wst,
	[0x05] = system_dei_rst,
	[0xc0] = datetime_dei_yhb,
	[0xc1] = datetime_dei_ylb,
	[0xc2] = datetime_dei_mon,
	[0xc3] = datetime_dei_day,
	[0xc4] = datetime_dei_hou,
	[0xc5] = datetime_dei_min,
	[0xc6] = datetime_dei_sec,
	[0xc7] = datetime_dei_wday,
	[0xc8] = datetime_dei_ydayhb,
	[0xc9] = datetime_dei_ydaylb,
	[0xca] = datetime_dei_dst,
};

static const deo_handler deo_handlers[256] = {
	[0x03] = system_deo_expansion,
	[0x04] = system_deo_wst,
	[0x05] = system_deo_rst,
	[0x0e] = system_deo_print,
	[0x11] = console_deo_vector,
	[0x18] = console_deo_stdout,
	[0x19] = console_deo_stderr,
	[0x1a] = console_deo_hb,
	[0x1b] = console_deo_lb,
	[0xa5] = filea_deo_stat,
	[0xa6] = filea_deo_delete,
	[0xa9] = filea_deo_name,
	[0xab] = filea_deo_length,
	[0xad] = filea_deo_read,
	[0xaf] = filea_deo_write,
	[0xb5] = fileb_deo_stat,
	[0xb6] = fileb_deo_delete,
	[0xb9] = fileb_deo_name,
	[0xbb] = fileb_deo_length,
	[0xbd] = fileb_deo_read,
	[0xbf] = fileb_deo_write,
    };
static inline Uint8
emu_dei(const Uint8 port)
{
#ifdef EXTENSION
    if ((port & 0xf0) == 0xd0) {
        return extension_dei(port);
    } else {
#endif
        dei_handler h = dei_handlers[port];
        return h ? h() : dev[port];
#ifdef EXTENSION
    }
#endif
}

static inline void
emu_deo(const Uint8 port, const Uint8 value)
{
#ifdef EXTENSION
    if ((port & 0xf0) == 0xd0) {
	    dev[port] = value;
        extension_deo(port);
    } else {
#endif
	deo_handler h = deo_handlers[port];
	dev[port] = value;
	if(h) h();
#ifdef EXTENSION
    }
#endif
        
}

/* clang-format off */

/*
@|Uxn --------------------------------------------------------------- */

#define Ld1(o)      stk[r][(Uint8)(ptr[r]-(o))]
#define Ld2(o)      (Ld1(o) << 8 | stk[r][(Uint8)(ptr[r]-(o)+1)])
#define Ldx(o8,o16) d ? Ld2(o16) : Ld1(o8)
#define Re1(m)      ptr[r] -= m;
#define Rex(m1,m2)  ptr[r] -= d ? m2 : m1;
#define Pu1(s,v)    stk[s][ptr[s]++] = v;
#define Pu2(s,v)    Pu1(s,v>>8) Pu1(s,v)
#define Pux(s,v)    if(d) Pu1(s,(v)>>8) Pu1(s,v)
#define Lda(s,o)    Pu1(r,ram[o]) if(d) Pu1(r,ram[(s)(o+1)])
#define Sta(s,o,u)  if(d) ram[o]=u>>8, ram[(s)(o+1)]=u; else ram[o]=u;
#define Dei         Pu1(r,emu_dei(x)) if(d) Pu1(r,emu_dei(x+1))
#define Deo         if(d) emu_deo(x,y>>8), emu_deo(x+1,y); else emu_deo(x,y);

#define MUTE (void)x;(void)y;(void)z;(void)r;(void)d;

#define OP(opc,X,Y,Z,K,P) \
	case (opc)|0x00: { const int d=0,r=0,x=X,y=Y,z=Z; K P MUTE } break; \
	case (opc)|0x20: { const int d=1,r=0,x=X,y=Y,z=Z; K P MUTE } break; \
	case (opc)|0x40: { const int d=0,r=1,x=X,y=Y,z=Z; K P MUTE } break; \
	case (opc)|0x60: { const int d=1,r=1,x=X,y=Y,z=Z; K P MUTE } break; \
	case (opc)|0x80: { const int d=0,r=0,x=X,y=Y,z=Z; P MUTE } break; \
	case (opc)|0xa0: { const int d=1,r=0,x=X,y=Y,z=Z; P MUTE } break; \
	case (opc)|0xc0: { const int d=0,r=1,x=X,y=Y,z=Z; P MUTE } break; \
	case (opc)|0xe0: { const int d=1,r=1,x=X,y=Y,z=Z; P MUTE } break;

static unsigned int
uxn_eval(Uint16 start_pc)
{
	Uint16 pc = start_pc;
	for(;;)
	switch(ram[pc++]) {
	/* BRK */ case 0x00: return 1;
	/* JCI */ case 0x20: { Uint16 a=ram[pc]<<8|ram[pc+1]; pc+=2; if(stk[0][--ptr[0]]) pc+=a; } break;
	/* JMI */ case 0x40: { Uint16 a=ram[pc]<<8|ram[pc+1]; pc+=2+a; } break;
	/* JSI */ case 0x60: { Uint16 a=ram[pc]<<8|ram[pc+1]; const Uint8 d=1; pc+=2; Pux(1,pc); pc+=a; } break;
	/* LIT */ case 0x80: { const Uint8 r=0,d=0; Lda(Uint16,pc); pc+=1; } break;
	/* LI2 */ case 0xa0: { const Uint8 r=0,d=1; Lda(Uint16,pc); pc+=2; } break;
	/* LIr */ case 0xc0: { const Uint8 r=1,d=0; Lda(Uint16,pc); pc+=1; } break;
	/* L2r */ case 0xe0: { const Uint8 r=1,d=1; Lda(Uint16,pc); pc+=2; } break;
	/* INC */ OP(0x01, Ldx(1,2),0,       0,       Rex(1,2),Pux(r,x+1))
	/* POP */ OP(0x02, 0,       0,       0,       Rex(1,2),{})
	/* NIP */ OP(0x03, Ldx(1,2),0,       0,       Rex(2,4),Pux(r,x))
	/* SWP */ OP(0x04, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pux(r,x) Pux(r,y))
	/* ROT */ OP(0x05, Ldx(1,2),Ldx(2,4),Ldx(3,6),Rex(3,6),Pux(r,y) Pux(r,x) Pux(r,z))
	/* DUP */ OP(0x06, Ldx(1,2),0,       0,       Rex(1,2),Pux(r,x) Pux(r,x))
	/* OVR */ OP(0x07, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pux(r,y) Pux(r,x) Pux(r,y))
	/* EQU */ OP(0x08, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pu1(r,x==y))
	/* NEQ */ OP(0x09, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pu1(r,y!=x))
	/* GTH */ OP(0x0a, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pu1(r,y>x))
	/* LTH */ OP(0x0b, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pu1(r,y<x))
	/* JMP */ OP(0x0c, Ldx(1,2),0,       0,       Rex(1,2),pc=d?x:(pc+(Sint8)x);)
	/* JCN */ OP(0x0d, Ldx(1,2),Ld1(d+2),0,       Rex(2,3),if(y) pc=d?x:(pc+(Sint8)x);)
	/* JSR */ OP(0x0e, Ldx(1,2),0,       0,       Rex(1,2),Pu2(!r,pc) pc=d?x:(pc+(Sint8)x);)
	/* STH */ OP(0x0f, Ldx(1,2),0,       0,       Rex(1,2),Pux(!r,x))
	/* LDZ */ OP(0x10, Ld1(1),  0,       0,       Re1(1),  Lda(Uint8,x))
	/* STZ */ OP(0x11, Ld1(1),  Ldx(2,3),0,       Rex(2,3),Sta(Uint8,x,y))
	/* LDR */ OP(0x12, Ld1(1),  0,       0,       Re1(1),  Uint16 t=pc+(Sint8)x; Lda(Uint16,t))
	/* STR */ OP(0x13, Ld1(1),  Ldx(2,3),0,       Rex(2,3),Uint16 t=pc+(Sint8)x; Sta(Uint16,t,y))
	/* LDA */ OP(0x14, Ld2(2),  0,       0,       Re1(2),  Lda(Uint16,x))
	/* STA */ OP(0x15, Ld2(2),  Ldx(3,4),0,       Rex(3,4),Sta(Uint16,x,y))
	/* DEI */ OP(0x16, Ld1(1),  0,       0,       Re1(1),  Dei)
	/* DEO */ OP(0x17, Ld1(1),  Ldx(2,3),0,       Rex(2,3),Deo)
	/* ADD */ OP(0x18, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pux(r,y+x))
	/* SUB */ OP(0x19, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pux(r,y-x))
	/* MUL */ OP(0x1a, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pux(r,y*x))
	/* DIV */ OP(0x1b, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pux(r,x?y/x:0))
	/* AND */ OP(0x1c, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pux(r,y&x))
	/* ORA */ OP(0x1d, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pux(r,y|x))
	/* EOR */ OP(0x1e, Ldx(1,2),Ldx(2,4),0,       Rex(2,4),Pux(r,y^x))
	/* SFT */ OP(0x1f, Ld1(1)  ,Ldx(2,3),0,       Rex(2,3),Pux(r,(y>>(x&0xf))<<(x>>4)))
	}
	return 0;
}

/* clang-format on */

int
main(int argc, char **argv)
{
	int i = 1;
	if(argc == 2 && argv[1][0] == '-' && argv[1][1] == 'v')
		return !fprintf(stdout, "Uxncli - Varvara Emulator(80K), 1 Jun 2026.\n");
	else if(argc == 1)
		return !fprintf(stdout, "usage: %s [-v] file.rom [args..]\n", argv[0]);
	else if(!system_boot(argv[i++], argc > 2))
		return !fprintf(stdout, "Could not load %s.\n", argv[i - 1]);
	if(uxn_eval(0x100) && console_vector) {
		for(; i < argc; i++) {
			int c;
			char *p = argv[i];
			while(!dev[0x0f] && (c = *p++))
				console_input(c, CONSOLE_ARG);
			console_input('\n', i == argc - 1 ? CONSOLE_END : CONSOLE_EOA);
		}
		while(!dev[0x0f]) {
			int c = fgetc(stdin);
			if(c == EOF) break;
			console_input(c, CONSOLE_STD);
			datetime_busy = 0;
		}
		console_input('\n', CONSOLE_END);
	}
	return dev[0x0f] & 0x7f;
}
