#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include "pubsub_core.h"
#include <stdint.h>

// 消息确认回调
typedef void (*message_ack_callback_t)(const char *topic, uint32_t msg_id, void *user_data);

// 消息处理配置
typedef struct {
    bool enable_ack;
    uint32_t retry_count;
    uint32_t retry_interval_ms;
    message_ack_callback_t ack_callback;
    void *user_data;
} message_handler_config_t;

// 消息处理API
esp_err_t message_handler_init(const message_handler_config_t *config);
esp_err_t message_publish_with_qos(const char *topic_name, 
                                 const uint8_t *data, 
                                 uint32_t data_len,
                                 msg_priority_t priority,
                                 topic_qos_t qos,
                                 uint32_t *msg_id);
esp_err_t message_acknowledge(const char *topic_name, uint32_t msg_id);
esp_err_t message_set_retry_policy(uint32_t retry_count, uint32_t retry_interval_ms);

#endif /* MESSAGE_HANDLER_H */ 