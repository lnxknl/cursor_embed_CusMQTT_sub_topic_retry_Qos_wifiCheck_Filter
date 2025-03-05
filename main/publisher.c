#include "pubsub_core.h"
#include "memory_pool.h"
#include "esp_log.h"
#include <string.h>

#define TAG "PUBLISHER"

pubsub_err_t pubsub_publish(const char *topic_name, const uint8_t *data, uint32_t data_len, msg_priority_t priority) {
    if (topic_name == NULL || (data == NULL && data_len > 0)) {
        return PUBSUB_ERR_INVALID_PARAM;
    }

    xSemaphoreTake(topics_lock, portMAX_DELAY);

    // 查找主题
    topic_t *topic = NULL;
    for (uint32_t i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            topic = &topics[i];
            break;
        }
    }

    if (topic == NULL) {
        xSemaphoreGive(topics_lock);
        return PUBSUB_ERR_TOPIC_NOT_FOUND;
    }

    // 创建消息
    pubsub_msg_t msg;
    memset(&msg, 0, sizeof(pubsub_msg_t));
    
    strncpy(msg.topic, topic_name, MAX_TOPIC_NAME_LENGTH - 1);
    msg.topic[MAX_TOPIC_NAME_LENGTH - 1] = '\0';
    
    if (data_len > 0) {
        msg.data = memory_pool_alloc(data_len);
        if (msg.data == NULL) {
            xSemaphoreGive(topics_lock);
            return PUBSUB_ERR_NO_MEMORY;
        }
        memcpy(msg.data, data, data_len);
    } else {
        msg.data = NULL;
    }
    
    msg.data_len = data_len;
    msg.priority = priority;
    msg.timestamp = esp_timer_get_time();

    // 发送消息到队列
    BaseType_t result;
    if (priority == MSG_PRIORITY_CRITICAL) {
        result = xQueueSendToFront(topic->msg_queue, &msg, 0);
    } else {
        result = xQueueSend(topic->msg_queue, &msg, 0);
    }

    if (result != pdTRUE) {
        if (msg.data != NULL) {
            memory_pool_free(msg.data);
        }
        xSemaphoreGive(topics_lock);
        return PUBSUB_ERR_QUEUE_FULL;
    }

    xSemaphoreGive(topics_lock);
    ESP_LOGI(TAG, "Message published to topic: %s, size: %d bytes", topic_name, data_len);
    return PUBSUB_OK;
} 