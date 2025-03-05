#ifndef SECURITY_LAYER_H
#define SECURITY_LAYER_H

#include "esp_err.h"
#include <stdint.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/ssl.h>

// 加密模式
typedef enum {
    SECURITY_MODE_NONE = 0,
    SECURITY_MODE_AES_128,
    SECURITY_MODE_AES_256,
    SECURITY_MODE_TLS_1_2,
    SECURITY_MODE_TLS_1_3
} security_mode_t;

// 证书信息
typedef struct {
    const uint8_t *cert;
    size_t cert_len;
    const uint8_t *key;
    size_t key_len;
    const uint8_t *ca_cert;
    size_t ca_cert_len;
} security_certificates_t;

// 安全配置
typedef struct {
    security_mode_t mode;
    security_certificates_t certs;
    const char *hostname;
    bool verify_peer;
    uint32_t handshake_timeout_ms;
} security_config_t;

// 安全上下文
typedef struct security_context security_context_t;

// 安全层API
esp_err_t security_init(const security_config_t *config);
esp_err_t security_create_context(security_context_t **ctx);
esp_err_t security_destroy_context(security_context_t *ctx);
esp_err_t security_encrypt(security_context_t *ctx, const uint8_t *input, size_t input_len,
                         uint8_t *output, size_t *output_len);
esp_err_t security_decrypt(security_context_t *ctx, const uint8_t *input, size_t input_len,
                         uint8_t *output, size_t *output_len);
esp_err_t security_handshake(security_context_t *ctx);
esp_err_t security_close(security_context_t *ctx);

#endif /* SECURITY_LAYER_H */ 