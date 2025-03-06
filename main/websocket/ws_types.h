#ifndef WS_TYPES_H
#define WS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// WebSocket 操作码
typedef enum {
    WS_OPCODE_CONTINUATION = 0x0,
    WS_OPCODE_TEXT        = 0x1,
    WS_OPCODE_BINARY     = 0x2,
    WS_OPCODE_CLOSE      = 0x8,
    WS_OPCODE_PING       = 0x9,
    WS_OPCODE_PONG       = 0xA
} ws_opcode_t;

// WebSocket 关闭状态码
typedef enum {
    WS_CLOSE_NORMAL          = 1000,
    WS_CLOSE_GOING_AWAY     = 1001,
    WS_CLOSE_PROTOCOL_ERROR = 1002,
    WS_CLOSE_UNSUPPORTED    = 1003,
    WS_CLOSE_NO_STATUS      = 1005,
    WS_CLOSE_ABNORMAL       = 1006,
    WS_CLOSE_INVALID_DATA   = 1007,
    WS_CLOSE_POLICY         = 1008,
    WS_CLOSE_TOO_LARGE      = 1009,
    WS_CLOSE_EXTENSION      = 1010,
    WS_CLOSE_UNEXPECTED     = 1011
} ws_close_code_t;

// WebSocket 帧头
typedef struct {
    uint8_t fin        : 1;
    uint8_t rsv1       : 1;
    uint8_t rsv2       : 1;
    uint8_t rsv3       : 1;
    uint8_t opcode     : 4;
    uint8_t mask       : 1;
    uint8_t payload_len : 7;
} ws_frame_header_t;

// WebSocket 消息
typedef struct {
    ws_opcode_t opcode;
    uint8_t *payload;
    uint64_t payload_len;
    bool is_final;
} ws_message_t;

// WebSocket 配置
typedef struct {
    const char *host;
    uint16_t port;
    const char *path;
    const char *origin;
    const char *protocols;
    bool use_ssl;
    int timeout_ms;
} ws_config_t;

// WebSocket 事件类型
typedef enum {
    WS_EVENT_CONNECTED,
    WS_EVENT_DISCONNECTED,
    WS_EVENT_DATA,
    WS_EVENT_ERROR
} ws_event_type_t;

// WebSocket 事件
typedef struct {
    ws_event_type_t type;
    union {
        struct {
            ws_message_t message;
        } data;
        struct {
            int code;
            const char *message;
        } error;
    };
} ws_event_t;

// WebSocket 事件回调函数
typedef void (*ws_callback_t)(ws_event_t *event, void *user_data);

#endif /* WS_TYPES_H */ 