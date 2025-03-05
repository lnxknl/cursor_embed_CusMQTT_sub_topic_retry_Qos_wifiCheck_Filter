#include "pubsub_core.h"
#include "memory_pool.h"
#include "esp_log.h"
#include <string.h>

#define TAG "SUBSCRIBER_MGR"

pubsub_err_t pubsub_subscribe(const char *topic_name, subscriber_callback_t callback, void *user_data) {
    if (topic_name == NULL || callback == NULL) {
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

    xSemaphoreTake(topic->lock, portMAX_DELAY);

    // 检查是否已经订阅
    subscriber_t *current = topic->subscribers;
    while (current != NULL) {
        if (current->callback == callback) {
            xSemaphoreGive(topic->lock);
            xSemaphoreGive(topics_lock);
            return PUBSUB_ERR_INVALID_PARAM;
        }
        current = current->next;
    }

    // 检查订阅者数量限制
    if (topic->subscriber_count >= MAX_SUBSCRIBERS_PER_TOPIC) {
        xSemaphoreGive(topic->lock);
        xSemaphoreGive(topics_lock);
        return PUBSUB_ERR_MAX_SUBSCRIBERS;
    }

    // 创建新订阅者
    subscriber_t *new_subscriber = (subscriber_t *)memory_pool_alloc(sizeof(subscriber_t));
    if (new_subscriber == NULL) {
        xSemaphoreGive(topic->lock);
        xSemaphoreGive(topics_lock);
        return PUBSUB_ERR_NO_MEMORY;
    }

    new_subscriber->callback = callback;
    new_subscriber->user_data = user_data;
    new_subscriber->next = topic->subscribers;
    topic->subscribers = new_subscriber;
    topic->subscriber_count++;

    xSemaphoreGive(topic->lock);
    xSemaphoreGive(topics_lock);
    
    ESP_LOGI(TAG, "New subscriber added to topic: %s", topic_name);
    return PUBSUB_OK;
}

pubsub_err_t pubsub_unsubscribe(const char *topic_name, subscriber_callback_t callback) {
    if (topic_name == NULL || callback == NULL) {
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

    xSemaphoreTake(topic->lock, portMAX_DELAY);

    subscriber_t *current = topic->subscribers;
    subscriber_t *prev = NULL;

    while (current != NULL) {
        if (current->callback == callback) {
            if (prev == NULL) {
                topic->subscribers = current->next;
            } else {
                prev->next = current->next;
            }
            memory_pool_free(current);
            topic->subscriber_count--;
            
            xSemaphoreGive(topic->lock);
            xSemaphoreGive(topics_lock);
            
            ESP_LOGI(TAG, "Subscriber removed from topic: %s", topic_name);
            return PUBSUB_OK;
        }
        prev = current;
        current = current->next;
    }

    xSemaphoreGive(topic->lock);
    xSemaphoreGive(topics_lock);
    return PUBSUB_ERR_INVALID_PARAM;
} 