#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "esp_err.h"
#include <stdint.h>

// 网络状态回调
typedef void (*network_status_callback_t)(network_status_t status, void *user_data);

// 网络管理器配置
typedef struct {
    uint32_t reconnect_interval_ms;
    uint32_t max_reconnect_attempts;
    bool auto_reconnect;
    network_status_callback_t status_callback;
    void *user_data;
} network_manager_config_t;

// 网络管理器API
esp_err_t network_manager_init(const network_manager_config_t *config);
esp_err_t network_manager_start(void);
esp_err_t network_manager_stop(void);
esp_err_t network_manager_reconnect(void);
esp_err_t network_manager_get_status(network_status_t *status);
esp_err_t network_manager_set_callback(network_status_callback_t callback, void *user_data);

#endif /* NETWORK_MANAGER_H */ 