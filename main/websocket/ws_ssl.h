#ifndef WS_SSL_H
#define WS_SSL_H

#include "esp_err.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/net_sockets.h"

// SSL 配置
typedef struct {
    const char *ca_cert;           // CA证书
    const char *client_cert;       // 客户端证书
    const char *client_key;        // 客户端私钥
    bool verify_server;            // 是否验证服务器证书
    int handshake_timeout_ms;      // 握手超时时间
} ws_ssl_config_t;

// SSL 上下文
typedef struct {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt ca_cert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context client_key;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_net_context net_ctx;
} ws_ssl_ctx_t;

// SSL API
esp_err_t ws_ssl_init(ws_ssl_ctx_t *ssl_ctx, const ws_ssl_config_t *config);
esp_err_t ws_ssl_connect(ws_ssl_ctx_t *ssl_ctx, const char *host, int port);
esp_err_t ws_ssl_write(ws_ssl_ctx_t *ssl_ctx, const uint8_t *data, size_t len);
esp_err_t ws_ssl_read(ws_ssl_ctx_t *ssl_ctx, uint8_t *data, size_t len, size_t *read_len);
esp_err_t ws_ssl_close(ws_ssl_ctx_t *ssl_ctx);
esp_err_t ws_ssl_destroy(ws_ssl_ctx_t *ssl_ctx);

#endif /* WS_SSL_H */ 