#include <stdio.h>
#include <string.h>

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"
#include "psa/crypto.h"

#define PORT "3490"

int main(void)
{
    int ret = 0;

    mbedtls_net_context listen_fd;
    mbedtls_net_context client_fd;

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;

    mbedtls_x509_crt cert;
    mbedtls_pk_context key;

    unsigned char buffer[2048];

    psa_status_t status;

    const char response[] = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/html\r\n\r\n"
                            "<html><p>aek</p></html>\r\n";

    mbedtls_net_init(&listen_fd);
    mbedtls_net_init(&client_fd);

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);

    mbedtls_x509_crt_init(&cert);
    mbedtls_pk_init(&key);

    if (status != psa_crypto_init())
    {
        printf("psa_crypto_init failed %d\n", (int) status);
        ret = 1;
        goto exit;
    }

    if ((ret = mbedtls_x509_crt_parse_file( &cert, "certs/server.crt")) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_pk_parse_keyfile( &key, "certs/server.key", NULL)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_ssl_config_defaults( &conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_ssl_conf_own_cert( &conf, &cert, &key)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_ssl_setup( &ssl, &conf)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_net_bind(&listen_fd, NULL, PORT, MBEDTLS_NET_PROTO_TCP)) != 0)
    {
        goto exit;
    }

    printf("Server running on\n");
    printf("https://localhost:%s/\n", PORT);
    printf("Wait for client\n");

    if ((ret = mbedtls_net_accept( &listen_fd, &client_fd, NULL, 0, NULL)) != 0)
    {
        goto exit;
    }

    mbedtls_ssl_set_bio( &ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    printf("TLS handshake\n");

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            goto exit;
        }
    }

    memset(buffer, 0, sizeof(buffer));

    ret = mbedtls_ssl_read( &ssl, buffer, sizeof(buffer) - 1);

    if (ret > 0)
    {
        printf("\nHTTP request:\n");
        printf("%s\n", buffer);
    }
    else if (ret < 0)
    {
        goto exit;
    }

    size_t response_len = strlen(response);
    size_t written = 0;

    while (written < response_len)
    {
        ret = mbedtls_ssl_write( &ssl, (const unsigned char *) response + written, response_len - written);

        if (ret > 0)
        {
            written += ret;
        }
        else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            goto exit;
        }
    }

    printf("Response sent!\n");

    mbedtls_ssl_close_notify(&ssl);

    ret = 0;

exit:

    mbedtls_net_free(&client_fd);
    mbedtls_net_free(&listen_fd);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&cert);
    mbedtls_pk_free(&key);
    mbedtls_psa_crypto_free();
    mbedtls_exit(ret);
}