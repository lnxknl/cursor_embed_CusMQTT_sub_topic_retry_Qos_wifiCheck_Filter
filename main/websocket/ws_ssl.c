#include "ws_ssl.h"
#include "esp_log.h"
#include <string.h>

#define TAG "WS_SSL"

esp_err_t ws_ssl_init(ws_ssl_ctx_t *ssl_ctx, const ws_ssl_config_t *config) {
    if (!ssl_ctx || !config) {
        return ESP_ERR_INVALID_ARG;
    }

    // 初始化 MbedTLS
    mbedtls_ssl_init(&ssl_ctx->ssl);
    mbedtls_ssl_config_init(&ssl_ctx->conf);
    mbedtls_x509_crt_init(&ssl_ctx->ca_cert);
    mbedtls_x509_crt_init(&ssl_ctx->client_cert);
    mbedtls_pk_init(&ssl_ctx->client_key);
    mbedtls_entropy_init(&ssl_ctx->entropy);
    mbedtls_ctr_drbg_init(&ssl_ctx->ctr_drbg);
    mbedtls_net_init(&ssl_ctx->net_ctx);

    // 配置随机数生成器
    int ret = mbedtls_ctr_drbg_seed(&ssl_ctx->ctr_drbg, mbedtls_entropy_func,
                                   &ssl_ctx->entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
        return ESP_FAIL;
    }

    // 配置 SSL
    ret = mbedtls_ssl_config_defaults(&ssl_ctx->conf,
                                    MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        return ESP_FAIL;
    }

    // 设置随机数生成器
    mbedtls_ssl_conf_rng(&ssl_ctx->conf, mbedtls_ctr_drbg_random,
                         &ssl_ctx->ctr_drbg);

    // 加载 CA 证书
    if (config->ca_cert) {
        ret = mbedtls_x509_crt_parse(&ssl_ctx->ca_cert,
                                    (const unsigned char *)config->ca_cert,
                                    strlen(config->ca_cert) + 1);
        if (ret < 0) {
            ESP_LOGE(TAG, "mbedtls_x509_crt_parse ca_cert returned %d", ret);
            return ESP_FAIL;
        }
        mbedtls_ssl_conf_ca_chain(&ssl_ctx->conf, &ssl_ctx->ca_cert, NULL);
    }

    // 加载客户端证书和私钥
    if (config->client_cert && config->client_key) {
        ret = mbedtls_x509_crt_parse(&ssl_ctx->client_cert,
                                    (const unsigned char *)config->client_cert,
                                    strlen(config->client_cert) + 1);
        if (ret < 0) {
            ESP_LOGE(TAG, "mbedtls_x509_crt_parse client_cert returned %d", ret);
            return ESP_FAIL;
        }

        ret = mbedtls_pk_parse_key(&ssl_ctx->client_key,
                                  (const unsigned char *)config->client_key,
                                  strlen(config->client_key) + 1,
                                  NULL, 0);
        if (ret < 0) {
            ESP_LOGE(TAG, "mbedtls_pk_parse_key returned %d", ret);
            return ESP_FAIL;
        }

        ret = mbedtls_ssl_conf_own_cert(&ssl_ctx->conf,
                                       &ssl_ctx->client_cert,
                                       &ssl_ctx->client_key);
        if (ret != 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_conf_own_cert returned %d", ret);
            return ESP_FAIL;
        }
    }

    // 设置证书验证模式
    mbedtls_ssl_conf_authmode(&ssl_ctx->conf,
                             config->verify_server ? 
                             MBEDTLS_SSL_VERIFY_REQUIRED :
                             MBEDTLS_SSL_VERIFY_NONE);

    // 设置握手超时
    mbedtls_ssl_conf_handshake_timeout(&ssl_ctx->conf,
                                      config->handshake_timeout_ms / 2,
                                      config->handshake_timeout_ms);

    // 设置 SSL 配置
    ret = mbedtls_ssl_setup(&ssl_ctx->ssl, &ssl_ctx->conf);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned %d", ret);
        return ESP_FAIL;
    }

    return ESP_OK;
} 