#include "mqtt_client.h"
#include "mqtt_encoder.h"
#include "mqtt_decoder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <string.h>

#define TAG "MQTT_CLIENT"
#define MQTT_TASK_STACK_SIZE 4096
#define MQTT_TASK_PRIORITY 5
#define MQTT_QUEUE_SIZE 10

// MQTT 客户端状态
typedef enum {
    MQTT_STATE_DISCONNECTED,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_DISCONNECTING
} mqtt_client_state_t;

// MQTT 客户端上下文
typedef struct {
    int socket;
    mqtt_client_state_t state;
    mqtt_connect_options_t connect_options;
    TaskHandle_t task_handle;
    QueueHandle_t msg_queue;
    EventGroupHandle_t event_group;
    uint16_t next_packet_id;
    mqtt_callback_t callback;
    void *callback_arg;
} mqtt_client_ctx_t;

// 内部消息类型
typedef struct {
    mqtt_packet_type_t type;
    union {
        mqtt_message_t publish;
        struct {
            const char *topic;
            mqtt_qos_t qos;
        } subscribe;
        struct {
            const char *topic;
        } unsubscribe;
    } data;
} mqtt_internal_message_t;

static mqtt_client_ctx_t *client_ctx = NULL;

// 生成下一个报文标识符
static uint16_t get_next_packet_id(void) {
    client_ctx->next_packet_id++;
    if (client_ctx->next_packet_id == 0) {
        client_ctx->next_packet_id = 1;
    }
    return client_ctx->next_packet_id;
}

// 发送 MQTT 数据
static int mqtt_send_packet(const uint8_t *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int ret = send(client_ctx->socket, buf + sent, len - sent, 0);
        if (ret < 0) {
            ESP_LOGE(TAG, "Send failed: errno %d", errno);
            return -1;
        }
        sent += ret;
    }
    return sent;
}

// 接收 MQTT 数据
static int mqtt_receive_packet(uint8_t *buf, int len) {
    int received = 0;
    while (received < len) {
        int ret = recv(client_ctx->socket, buf + received, len - received, 0);
        if (ret < 0) {
            ESP_LOGE(TAG, "Receive failed: errno %d", errno);
            return -1;
        }
        if (ret == 0) {
            ESP_LOGE(TAG, "Connection closed by peer");
            return -1;
        }
        received += ret;
    }
    return received;
}

// 处理接收到的 MQTT 包
static void handle_mqtt_packet(const uint8_t *buf, int len) {
    mqtt_fixed_header_t header;
    int pos = mqtt_decode_fixed_header(buf, len, &header);
    if (pos < 0) {
        ESP_LOGE(TAG, "Failed to decode fixed header");
        return;
    }

    switch (header.type) {
        case MQTT_CONNACK: {
            uint8_t session_present, return_code;
            if (mqtt_decode_connack(buf, len, &session_present, &return_code) < 0) {
                ESP_LOGE(TAG, "Failed to decode CONNACK");
                return;
            }
            if (return_code == 0) {
                client_ctx->state = MQTT_STATE_CONNECTED;
                if (client_ctx->callback) {
                    client_ctx->callback(MQTT_EVENT_CONNECTED, NULL, client_ctx->callback_arg);
                }
            } else {
                ESP_LOGE(TAG, "Connection refused: %d", return_code);
                client_ctx->state = MQTT_STATE_DISCONNECTED;
            }
            break;
        }
        
        case MQTT_PUBLISH: {
            mqtt_message_t message;
            if (mqtt_decode_publish(buf, len, &message) < 0) {
                ESP_LOGE(TAG, "Failed to decode PUBLISH");
                return;
            }
            if (client_ctx->callback) {
                client_ctx->callback(MQTT_EVENT_MESSAGE, &message, client_ctx->callback_arg);
            }
            // 释放解码时分配的内存
            free((void *)message.topic);
            free((void *)message.payload);
            break;
        }
        
        case MQTT_PUBACK:
        case MQTT_PUBREC:
        case MQTT_PUBREL:
        case MQTT_PUBCOMP:
        case MQTT_SUBACK:
        case MQTT_UNSUBACK:
            // 处理确认消息
            break;
            
        case MQTT_PINGRESP:
            // 处理 PING 响应
            break;
            
        default:
            ESP_LOGW(TAG, "Unhandled packet type: %d", header.type);
            break;
    }
}

// MQTT 客户端任务
static void mqtt_client_task(void *arg) {
    uint8_t buf[1024];
    mqtt_internal_message_t msg;

    while (1) {
        if (client_ctx->state == MQTT_STATE_CONNECTED) {
            // 检查消息队列
            if (xQueueReceive(client_ctx->msg_queue, &msg, 0) == pdTRUE) {
                switch (msg.type) {
                    case MQTT_PUBLISH: {
                        uint16_t packet_id = get_next_packet_id();
                        int len = mqtt_encode_publish(&msg.data.publish, packet_id, buf, sizeof(buf));
                        if (len > 0) {
                            mqtt_send_packet(buf, len);
                        }
                        break;
                    }
                    
                    case MQTT_SUBSCRIBE: {
                        mqtt_topic_filter_t filter = {
                            .topic = msg.data.subscribe.topic,
                            .qos = msg.data.subscribe.qos
                        };
                        uint16_t packet_id = get_next_packet_id();
                        int len = mqtt_encode_subscribe(packet_id, &filter, 1, buf, sizeof(buf));
                        if (len > 0) {
                            mqtt_send_packet(buf, len);
                        }
                        break;
                    }
                    
                    case MQTT_UNSUBSCRIBE: {
                        const char *topics[] = {msg.data.unsubscribe.topic};
                        uint16_t packet_id = get_next_packet_id();
                        int len = mqtt_encode_unsubscribe(packet_id, topics, 1, buf, sizeof(buf));
                        if (len > 0) {
                            mqtt_send_packet(buf, len);
                        }
                        break;
                    }
                    
                    default:
                        break;
                }
            }

            // 接收数据
            fd_set readfds;
            struct timeval tv = {
                .tv_sec = 1,
                .tv_usec = 0
            };

            FD_ZERO(&readfds);
            FD_SET(client_ctx->socket, &readfds);

            int ret = select(client_ctx->socket + 1, &readfds, NULL, NULL, &tv);
            if (ret > 0) {
                if (FD_ISSET(client_ctx->socket, &readfds)) {
                    int len = mqtt_receive_packet(buf, sizeof(buf));
                    if (len > 0) {
                        handle_mqtt_packet(buf, len);
                    } else {
                        // 连接断开
                        client_ctx->state = MQTT_STATE_DISCONNECTED;
                        if (client_ctx->callback) {
                            client_ctx->callback(MQTT_EVENT_DISCONNECTED, NULL, 
                                              client_ctx->callback_arg);
                        }
                    }
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 初始化 MQTT 客户端
esp_err_t mqtt_client_init(const mqtt_connect_options_t *options,
                          mqtt_callback_t callback, void *callback_arg) {
    if (client_ctx != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    client_ctx = calloc(1, sizeof(mqtt_client_ctx_t));
    if (client_ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // 复制连接选项
    memcpy(&client_ctx->connect_options, options, sizeof(mqtt_connect_options_t));
    
    // 创建消息队列
    client_ctx->msg_queue = xQueueCreate(MQTT_QUEUE_SIZE, 
                                       sizeof(mqtt_internal_message_t));
    if (client_ctx->msg_queue == NULL) {
        free(client_ctx);
        return ESP_ERR_NO_MEM;
    }

    // 创建事件组
    client_ctx->event_group = xEventGroupCreate();
    if (client_ctx->event_group == NULL) {
        vQueueDelete(client_ctx->msg_queue);
        free(client_ctx);
        return ESP_ERR_NO_MEM;
    }

    client_ctx->callback = callback;
    client_ctx->callback_arg = callback_arg;
    client_ctx->next_packet_id = 0;
    client_ctx->state = MQTT_STATE_DISCONNECTED;

    // 创建 MQTT 客户端任务
    BaseType_t ret = xTaskCreate(mqtt_client_task, "mqtt_client", 
                                MQTT_TASK_STACK_SIZE, NULL,
                                MQTT_TASK_PRIORITY, &client_ctx->task_handle);
    if (ret != pdPASS) {
        vEventGroupDelete(client_ctx->event_group);
        vQueueDelete(client_ctx->msg_queue);
        free(client_ctx);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
} 