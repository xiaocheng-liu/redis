#ifndef TLS_H
#define TLS_H

#include "server.h"

/*-----------------------------------------------------------------------------
 * TLS Context Configuration
 *----------------------------------------------------------------------------*/

typedef struct redisTLSContextConfig
{
    char *cert_file;        /* Server side and optionally client side cert file name */
    char *key_file;         /* Private key filename for cert_file */
    char *client_cert_file; /* Certificate to use as a client; if none, use cert_file */
    char *client_key_file;  /* Private key filename for client_cert_file */
    char *dh_params_file;
    char *ca_cert_file;
    char *ca_cert_dir;
    char *protocols;
    char *ciphers;
    char *ciphersuites;
    int prefer_server_ciphers;
    int session_caching;
    int session_cache_size;
    int session_cache_timeout;
} redisTLSContextConfig;

/* TLS stuff */
void tlsInit(void);

int tlsConfigure(redisTLSContextConfig *ctx_config);

#endif // TLS_H
