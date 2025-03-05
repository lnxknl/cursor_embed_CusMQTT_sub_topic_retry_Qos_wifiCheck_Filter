#ifndef NETWORK_LAYER_H
#define NETWORK_LAYER_H

#include "esp_err.h"
#include <stdint.h>

// 网络配置结构体
typedef struct {
    char wifi_ssid[32];
    char wifi_password[64];
    char mqtt_broker[128];
    uint16_t mqtt_port;
    char mqtt_username[32];
    char mqtt_password[64];
    char client_id[32];
} network_config_t;

// 网络状态枚举
typedef enum {
    NETWORK_STATUS_DISCONNECTED = 0,
    NETWORK_STATUS_CONNECTING,
    NETWORK_STATUS_CONNECTED,
    NETWORK_STATUS_ERROR
} network_status_t;

// 网络事件回调函数类型
typedef void (*network_event_callback_t)(network_status_t status, void *user_data);

// 网络层API
esp_err_t network_init(const network_config_t *config);
esp_err_t network_deinit(void);
esp_err_t network_connect(void);
esp_err_t network_disconnect(void);
esp_err_t network_send_message(const char *topic, const uint8_t *data, size_t len);
esp_err_t network_register_callback(network_event_callback_t callback, void *user_data);
network_status_t network_get_status(void);

#endif /* NETWORK_LAYER_H */ 