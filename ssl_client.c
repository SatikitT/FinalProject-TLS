#include <stdio.h>
#include <string.h>

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "psa/crypto.h"

#define SERVER "localhost"
#define PORT "3490"

int main(void)
{
    int ret = 0;

    mbedtls_net_context server_fd;

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;

    mbedtls_x509_crt ca;

    unsigned char buffer[2048];

    psa_status_t status;

    const char request[] = "GET / HTTP/1.1\r\n"
                           "Host: localhost:3490\r\n"
                           "Connection: close\r\n\r\n";

    mbedtls_net_init(&server_fd);

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);

    mbedtls_x509_crt_init(&ca);

    status = psa_crypto_init();

    if (status != PSA_SUCCESS)
    {
        printf("psa_crypto_init failed %d\n", (int) status);
        ret = 1;
        goto exit;
    }

    if ((ret = mbedtls_ssl_config_defaults( &conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        goto exit;
    }

    mbedtls_ssl_conf_authmode( &conf, MBEDTLS_SSL_VERIFY_NONE);

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_ssl_set_hostname( &ssl, SERVER)) != 0)
    {
        goto exit;
    }

    printf("Connecting to server\n");

    if ((ret = mbedtls_net_connect( &server_fd, SERVER, PORT, MBEDTLS_NET_PROTO_TCP)) != 0)
    {
        goto exit;
    }

    printf("Connected to %s:%s\n", SERVER, PORT);

    mbedtls_ssl_set_bio( &ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    printf("TLS handshake\n");

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            goto exit;
        }
    }

    printf("TLS handshake successful\n");

    size_t request_len = strlen(request);
    size_t written = 0;

    while (written < request_len)
    {
        ret = mbedtls_ssl_write(&ssl,(const unsigned char *) request + written, request_len - written);

        if (ret > 0)
        {
            written += ret;
        }
        else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            goto exit;
        }
    }

    printf("Request sent!\n");

    printf("\nResponse:\n");

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        ret = mbedtls_ssl_read( &ssl, buffer, sizeof(buffer) - 1);

        if (ret > 0)
        {
            printf("%.*s", ret, buffer);
        }
        else if (ret == 0)
        {
            break;
        }
        else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            continue;
        }
        else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
        {
            break;
        }
        else
        {
            goto exit;
        }
    }

    mbedtls_ssl_close_notify(&ssl);

    ret = 0;

exit:

    mbedtls_net_free(&server_fd);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&ca);
    mbedtls_psa_crypto_free();
    mbedtls_exit(ret);
}