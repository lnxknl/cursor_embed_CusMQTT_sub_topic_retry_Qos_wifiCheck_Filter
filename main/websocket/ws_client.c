#include "ws_client.h"
#include "ws_handshake.h"
#include "ws_frame.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <string.h>

#define TAG "WS_CLIENT"
#define WS_TASK_STACK_SIZE 4096
#define WS_TASK_PRIORITY 5
#define WS_QUEUE_SIZE 10
#define WS_BUFFER_SIZE 4096

// WebSocket 客户端状态
typedef enum {
    WS_STATE_DISCONNECTED,
    WS_STATE_CONNECTING,
    WS_STATE_CONNECTED,
    WS_STATE_CLOSING
} ws_client_state_t;

// WebSocket 客户端上下文
typedef struct {
    int socket;
    ws_client_state_t state;
    ws_config_t config;
    TaskHandle_t task_handle;
    QueueHandle_t msg_queue;
    EventGroupHandle_t event_group;
    ws_callback_t callback;
    void *callback_arg;
    uint8_t *rx_buffer;
    size_t rx_buffer_len;
    uint8_t *tx_buffer;
    size_t tx_buffer_len;
} ws_client_ctx_t;

static ws_client_ctx_t *client_ctx = NULL;

// WebSocket 客户端任务
static void ws_client_task(void *arg) {
    uint8_t buffer[WS_BUFFER_SIZE];
    ws_message_t message;
    size_t frame_len;
    
    while (1) {
        switch (client_ctx->state) {
            case WS_STATE_DISCONNECTED:
                // 尝试连接
                if (ws_client_connect() == ESP_OK) {
                    client_ctx->state = WS_STATE_CONNECTING;
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                break;
                
            case WS_STATE_CONNECTING:
                // 执行 WebSocket 握手
                if (ws_perform_handshake() == ESP_OK) {
                    client_ctx->state = WS_STATE_CONNECTED;
                    // 触发连接事件
                    ws_event_t event = {
                        .type = WS_EVENT_CONNECTED
                    };
                    if (client_ctx->callback) {
                        client_ctx->callback(&event, client_ctx->callback_arg);
                    }
                } else {
                    client_ctx->state = WS_STATE_DISCONNECTED;
                    close(client_ctx->socket);
                }
                break;
                
            case WS_STATE_CONNECTED:
                // 处理发送队列
                ws_message_t *send_msg;
                if (xQueueReceive(client_ctx->msg_queue, &send_msg, 0) == pdTRUE) {
                    if (ws_encode_frame(send_msg, buffer, sizeof(buffer), &frame_len) == ESP_OK) {
                        if (send(client_ctx->socket, buffer, frame_len, 0) < 0) {
                            ESP_LOGE(TAG, "Send failed");
                            client_ctx->state = WS_STATE_DISCONNECTED;
                        }
                    }
                }
                
                // 处理接收数据
                int len = recv(client_ctx->socket, buffer, sizeof(buffer), 0);
                if (len > 0) {
                    if (ws_decode_frame(buffer, len, &message, &frame_len) == ESP_OK) {
                        // 处理控制帧
                        switch (message.opcode) {
                            case WS_OPCODE_CLOSE:
                                // 发送关闭帧
                                ws_send_close_frame();
                                client_ctx->state = WS_STATE_CLOSING;
                                break;
                                
                            case WS_OPCODE_PING:
                                // 发送 PONG
                                ws_send_pong_frame();
                                break;
                                
                            case WS_OPCODE_PONG:
                                // 处理 PONG
                                break;
                                
                            default:
                                // 触发数据事件
                                ws_event_t event = {
                                    .type = WS_EVENT_DATA,
                                    .data.message = message
                                };
                                if (client_ctx->callback) {
                                    client_ctx->callback(&event, client_ctx->callback_arg);
                                }
                                break;
                        }
                        
                        // 释放消息载荷
                        if (message.payload) {
                            free(message.payload);
                        }
                    }
                } else if (len < 0) {
                    ESP_LOGE(TAG, "Receive failed");
                    client_ctx->state = WS_STATE_DISCONNECTED;
                }
                break;
                
            case WS_STATE_CLOSING:
                // 等待关闭完成
                vTaskDelay(pdMS_TO_TICKS(100));
                close(client_ctx->socket);
                client_ctx->state = WS_STATE_DISCONNECTED;
                // 触发断开连接事件
                ws_event_t event = {
                    .type = WS_EVENT_DISCONNECTED
                };
                if (client_ctx->callback) {
                    client_ctx->callback(&event, client_ctx->callback_arg);
                }
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
} 