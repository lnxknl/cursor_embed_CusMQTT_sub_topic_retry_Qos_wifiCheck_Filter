#include "ws_handshake.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"
#include <string.h>

#define TAG "WS_HANDSHAKE"
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_KEY_LENGTH 24
#define HTTP_BUFFER_SIZE 1024

// 生成 WebSocket 密钥
static esp_err_t generate_websocket_key(char *key, size_t key_len) {
    uint8_t random[16];
    size_t output_len;
    
    // 生成随机数
    for (int i = 0; i < 16; i++) {
        random[i] = esp_random() & 0xFF;
    }
    
    // Base64 编码
    mbedtls_base64_encode((unsigned char *)key, key_len, &output_len,
                         random, sizeof(random));
    
    return ESP_OK;
}

// 计算 WebSocket 接受密钥
static esp_err_t calculate_accept_key(const char *key, char *accept_key, 
                                    size_t accept_key_len) {
    uint8_t sha1sum[20];
    char concat_key[128];
    size_t output_len;
    
    // 连接 key 和 GUID
    snprintf(concat_key, sizeof(concat_key), "%s%s", key, WS_GUID);
    
    // 计算 SHA1
    mbedtls_sha1((unsigned char *)concat_key, strlen(concat_key), sha1sum);
    
    // Base64 编码
    mbedtls_base64_encode((unsigned char *)accept_key, accept_key_len, &output_len,
                         sha1sum, sizeof(sha1sum));
    
    return ESP_OK;
}

// 生成握手请求
esp_err_t ws_generate_handshake_request(const ws_config_t *config, char *buffer, 
                                      size_t buffer_len, char *key) {
    if (!config || !buffer || !key) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 生成 WebSocket 密钥
    generate_websocket_key(key, WS_KEY_LENGTH);
    
    // 构建 HTTP 请求
    int len = snprintf(buffer, buffer_len,
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n",
        config->path, config->host, config->port, key);
    
    // 添加可选字段
    if (config->origin) {
        len += snprintf(buffer + len, buffer_len - len,
            "Origin: %s\r\n", config->origin);
    }
    
    if (config->protocols) {
        len += snprintf(buffer + len, buffer_len - len,
            "Sec-WebSocket-Protocol: %s\r\n", config->protocols);
    }
    
    // 添加空行结束请求
    len += snprintf(buffer + len, buffer_len - len, "\r\n");
    
    return ESP_OK;
}

// 验证握手响应
esp_err_t ws_verify_handshake_response(const char *response, size_t response_len,
                                     const char *key) {
    if (!response || !key) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 检查 HTTP 状态码
    if (strncmp(response, "HTTP/1.1 101", 12) != 0) {
        ESP_LOGE(TAG, "Invalid HTTP response status");
        return ESP_FAIL;
    }
    
    // 检查必需的头部字段
    if (strstr(response, "Upgrade: websocket\r\n") == NULL) {
        ESP_LOGE(TAG, "Missing Upgrade header");
        return ESP_FAIL;
    }
    
    if (strstr(response, "Connection: Upgrade\r\n") == NULL) {
        ESP_LOGE(TAG, "Missing Connection header");
        return ESP_FAIL;
    }
    
    // 验证 Sec-WebSocket-Accept
    char expected_accept[32];
    char *accept_header = strstr(response, "Sec-WebSocket-Accept: ");
    if (!accept_header) {
        ESP_LOGE(TAG, "Missing Sec-WebSocket-Accept header");
        return ESP_FAIL;
    }
    
    calculate_accept_key(key, expected_accept, sizeof(expected_accept));
    accept_header += strlen("Sec-WebSocket-Accept: ");
    if (strncmp(accept_header, expected_accept, strlen(expected_accept)) != 0) {
        ESP_LOGE(TAG, "Invalid Sec-WebSocket-Accept value");
        return ESP_FAIL;
    }
    
    return ESP_OK;
} 