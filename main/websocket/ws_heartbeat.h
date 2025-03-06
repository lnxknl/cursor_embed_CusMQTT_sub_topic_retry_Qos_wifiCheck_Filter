#ifndef WS_HEARTBEAT_H
#define WS_HEARTBEAT_H

#include "esp_err.h"
#include <stdint.h>

// 心跳配置
typedef struct {
    uint32_t interval_ms;          // 心跳间隔
    uint32_t timeout_ms;           // 超时时间
    uint32_t max_missed;           // 最大丢失次数
} ws_heartbeat_config_t;

// 心跳状态
typedef struct {
    uint32_t missed_count;         // 丢失次数
    uint64_t last_send_time;       // 上次发送时间
    uint64_t last_recv_time;       // 上次接收时间
    bool is_active;                // 是否激活
} ws_heartbeat_state_t;

// 心跳回调
typedef void (*ws_heartbeat_callback_t)(void *user_data);

// 心跳上下文
typedef struct {
    ws_heartbeat_config_t config;  // 配置
    ws_heartbeat_state_t state;    // 状态
    ws_heartbeat_callback_t on_timeout; // 超时回调
    void *user_data;               // 用户数据
} ws_heartbeat_ctx_t;

// 心跳API
esp_err_t ws_heartbeat_init(ws_heartbeat_ctx_t *ctx,
                           const ws_heartbeat_config_t *config,
                           ws_heartbeat_callback_t on_timeout,
                           void *user_data);
esp_err_t ws_heartbeat_start(ws_heartbeat_ctx_t *ctx);
esp_err_t ws_heartbeat_stop(ws_heartbeat_ctx_t *ctx);
esp_err_t ws_heartbeat_reset(ws_heartbeat_ctx_t *ctx);
esp_err_t ws_heartbeat_send(ws_heartbeat_ctx_t *ctx);
esp_err_t ws_heartbeat_receive(ws_heartbeat_ctx_t *ctx);
bool ws_heartbeat_check_timeout(ws_heartbeat_ctx_t *ctx);

#endif /* WS_HEARTBEAT_H */ 