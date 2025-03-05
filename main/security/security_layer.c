#include "security_layer.h"
#include "error_handler.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>

#define TAG "SECURITY"
#define AES_BLOCK_SIZE 16
#define MAX_HANDSHAKE_ATTEMPTS 5

struct security_context {
    security_mode_t mode;
    union {
        struct {
            mbedtls_aes_context aes;
            uint8_t iv[16];
        } aes;
        struct {
            mbedtls_ssl_context ssl;
            mbedtls_ssl_config conf;
            mbedtls_x509_crt cert;
            mbedtls_pk_context pkey;
            mbedtls_x509_crt ca_cert;
        } tls;
    };
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
};

static security_config_t global_config;

esp_err_t security_init(const security_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&global_config, config, sizeof(security_config_t));

    // 初始化mbedTLS
    mbedtls_platform_set_calloc_free(calloc, free);

    return ESP_OK;
}

esp_err_t security_create_context(security_context_t **ctx) {
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *ctx = calloc(1, sizeof(security_context_t));
    if (*ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    (*ctx)->mode = global_config.mode;

    // 初始化随机数生成器
    mbedtls_entropy_init(&(*ctx)->entropy);
    mbedtls_ctr_drbg_init(&(*ctx)->ctr_drbg);

    int ret = mbedtls_ctr_drbg_seed(&(*ctx)->ctr_drbg, mbedtls_entropy_func,
                                   &(*ctx)->entropy, NULL, 0);
    if (ret != 0) {
        ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                    "Failed to initialize random number generator: -0x%x", -ret);
        free(*ctx);
        return ESP_FAIL;
    }

    // 根据模式初始化相应的安全上下文
    switch ((*ctx)->mode) {
        case SECURITY_MODE_AES_128:
        case SECURITY_MODE_AES_256:
            mbedtls_aes_init(&(*ctx)->aes.aes);
            // 生成随机IV
            ret = mbedtls_ctr_drbg_random(&(*ctx)->ctr_drbg, (*ctx)->aes.iv, 16);
            if (ret != 0) {
                ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                            "Failed to generate IV: -0x%x", -ret);
                free(*ctx);
                return ESP_FAIL;
            }
            break;

        case SECURITY_MODE_TLS_1_2:
        case SECURITY_MODE_TLS_1_3:
            mbedtls_ssl_init(&(*ctx)->tls.ssl);
            mbedtls_ssl_config_init(&(*ctx)->tls.conf);
            mbedtls_x509_crt_init(&(*ctx)->tls.cert);
            mbedtls_pk_init(&(*ctx)->tls.pkey);
            mbedtls_x509_crt_init(&(*ctx)->tls.ca_cert);

            // 加载证书
            if (global_config.certs.cert && global_config.certs.cert_len > 0) {
                ret = mbedtls_x509_crt_parse(&(*ctx)->tls.cert,
                                           global_config.certs.cert,
                                           global_config.certs.cert_len);
                if (ret != 0) {
                    ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                                "Failed to parse certificate: -0x%x", -ret);
                    security_destroy_context(*ctx);
                    return ESP_FAIL;
                }
            }

            // 加载私钥
            if (global_config.certs.key && global_config.certs.key_len > 0) {
                ret = mbedtls_pk_parse_key(&(*ctx)->tls.pkey,
                                         global_config.certs.key,
                                         global_config.certs.key_len,
                                         NULL, 0,
                                         mbedtls_ctr_drbg_random,
                                         &(*ctx)->ctr_drbg);
                if (ret != 0) {
                    ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                                "Failed to parse private key: -0x%x", -ret);
                    security_destroy_context(*ctx);
                    return ESP_FAIL;
                }
            }

            // 加载CA证书
            if (global_config.certs.ca_cert && global_config.certs.ca_cert_len > 0) {
                ret = mbedtls_x509_crt_parse(&(*ctx)->tls.ca_cert,
                                           global_config.certs.ca_cert,
                                           global_config.certs.ca_cert_len);
                if (ret != 0) {
                    ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                                "Failed to parse CA certificate: -0x%x", -ret);
                    security_destroy_context(*ctx);
                    return ESP_FAIL;
                }
            }

            // 配置TLS
            ret = mbedtls_ssl_config_defaults(&(*ctx)->tls.conf,
                                            MBEDTLS_SSL_IS_CLIENT,
                                            MBEDTLS_SSL_TRANSPORT_STREAM,
                                            MBEDTLS_SSL_PRESET_DEFAULT);
            if (ret != 0) {
                ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                            "Failed to set SSL configuration: -0x%x", -ret);
                security_destroy_context(*ctx);
                return ESP_FAIL;
            }

            mbedtls_ssl_conf_authmode(&(*ctx)->tls.conf,
                                    global_config.verify_peer ?
                                    MBEDTLS_SSL_VERIFY_REQUIRED :
                                    MBEDTLS_SSL_VERIFY_NONE);

            mbedtls_ssl_conf_ca_chain(&(*ctx)->tls.conf,
                                    &(*ctx)->tls.ca_cert, NULL);

            if (global_config.certs.cert && global_config.certs.key) {
                ret = mbedtls_ssl_conf_own_cert(&(*ctx)->tls.conf,
                                              &(*ctx)->tls.cert,
                                              &(*ctx)->tls.pkey);
                if (ret != 0) {
                    ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                                "Failed to configure own certificate: -0x%x", -ret);
                    security_destroy_context(*ctx);
                    return ESP_FAIL;
                }
            }

            mbedtls_ssl_conf_rng(&(*ctx)->tls.conf,
                                mbedtls_ctr_drbg_random,
                                &(*ctx)->ctr_drbg);

            ret = mbedtls_ssl_setup(&(*ctx)->tls.ssl, &(*ctx)->tls.conf);
            if (ret != 0) {
                ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                            "Failed to setup SSL: -0x%x", -ret);
                security_destroy_context(*ctx);
                return ESP_FAIL;
            }

            if (global_config.hostname) {
                ret = mbedtls_ssl_set_hostname(&(*ctx)->tls.ssl,
                                             global_config.hostname);
                if (ret != 0) {
                    ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                                "Failed to set hostname: -0x%x", -ret);
                    security_destroy_context(*ctx);
                    return ESP_FAIL;
                }
            }
            break;

        default:
            break;
    }

    return ESP_OK;
}

esp_err_t security_destroy_context(security_context_t *ctx) {
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (ctx->mode) {
        case SECURITY_MODE_AES_128:
        case SECURITY_MODE_AES_256:
            mbedtls_aes_free(&ctx->aes.aes);
            break;

        case SECURITY_MODE_TLS_1_2:
        case SECURITY_MODE_TLS_1_3:
            mbedtls_ssl_free(&ctx->tls.ssl);
            mbedtls_ssl_config_free(&ctx->tls.conf);
            mbedtls_x509_crt_free(&ctx->tls.cert);
            mbedtls_pk_free(&ctx->tls.pkey);
            mbedtls_x509_crt_free(&ctx->tls.ca_cert);
            break;

        default:
            break;
    }

    mbedtls_entropy_free(&ctx->entropy);
    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);

    free(ctx);
    return ESP_OK;
} 