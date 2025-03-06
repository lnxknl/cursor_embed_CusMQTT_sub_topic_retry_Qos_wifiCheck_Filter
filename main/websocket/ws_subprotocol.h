#ifndef WS_SUBPROTOCOL_H
#define WS_SUBPROTOCOL_H

#include "esp_err.h"
#include <stdint.h>

// 子协议处理函数
typedef esp_err_t (*ws_subprotocol_handler_t)(const uint8_t *data, 
                                             size_t data_len,
                                             void *user_data);

// 子协议定义
typedef struct {
    const char *name;              // 协议名称
    ws_subprotocol_handler_t handler; // 处理函数
    void *user_data;              // 用户数据
} ws_subprotocol_t;

// 子协议管理器
typedef struct {
    ws_subprotocol_t *protocols;  // 协议列表
    size_t protocol_count;        // 协议数量
    const char *selected;         // 选中的协议
} ws_subprotocol_mgr_t;

// 子协议API
esp_err_t ws_subprotocol_init(ws_subprotocol_mgr_t *mgr);
esp_err_t ws_subprotocol_register(ws_subprotocol_mgr_t *mgr,
                                 const char *name,
                                 ws_subprotocol_handler_t handler,
                                 void *user_data);
esp_err_t ws_subprotocol_select(ws_subprotocol_mgr_t *mgr,
                               const char *name);
esp_err_t ws_subprotocol_handle(ws_subprotocol_mgr_t *mgr,
                               const uint8_t *data,
                               size_t data_len);
const char *ws_subprotocol_get_selected(ws_subprotocol_mgr_t *mgr);

#endif /* WS_SUBPROTOCOL_H */ 