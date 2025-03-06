#ifndef WS_RECONNECT_H
#define WS_RECONNECT_H

#include "esp_err.h"
#include <stdint.h>

// 重连配置
typedef struct {
    uint32_t initial_delay_ms;     // 初始延迟
    uint32_t max_delay_ms;         // 最大延迟
    uint32_t multiplier;           // 延迟倍数
    uint32_t jitter_ms;            // 随机抖动
    uint32_t max_attempts;         // 最大尝试次数
    bool reset_delay_on_success;   // 连接成功时重置延迟
} ws_reconnect_config_t;

// 重连状态
typedef struct {
    uint32_t current_delay_ms;     // 当前延迟
    uint32_t attempt_count;        // 尝试次数
    uint64_t last_attempt_time;    // 上次尝试时间
    bool is_active;                // 是否激活
} ws_reconnect_state_t;

// 重连API
esp_err_t ws_reconnect_init(ws_reconnect_config_t *config);
esp_err_t ws_reconnect_start(void);
esp_err_t ws_reconnect_stop(void);
esp_err_t ws_reconnect_reset(void);
bool ws_reconnect_should_attempt(void);
uint32_t ws_reconnect_get_delay(void);
void ws_reconnect_on_connect_result(bool success);

#endif /* WS_RECONNECT_H */ 