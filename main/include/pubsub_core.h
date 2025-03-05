#ifndef PUBSUB_CORE_H
#define PUBSUB_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// 定义最大主题数量和主题名称长度
#define MAX_TOPICS 50
#define MAX_TOPIC_NAME_LENGTH 64
#define MAX_SUBSCRIBERS_PER_TOPIC 20
#define MAX_MSG_SIZE 1024
#define MAX_QUEUE_SIZE 100

// 消息优先级定义
typedef enum {
    MSG_PRIORITY_LOW = 0,
    MSG_PRIORITY_NORMAL,
    MSG_PRIORITY_HIGH,
    MSG_PRIORITY_CRITICAL
} msg_priority_t;

// 消息结构体
typedef struct {
    char topic[MAX_TOPIC_NAME_LENGTH];
    uint8_t *data;
    uint32_t data_len;
    msg_priority_t priority;
    uint64_t timestamp;
    void *user_data;
} pubsub_msg_t;

// 订阅者回调函数类型
typedef void (*subscriber_callback_t)(const pubsub_msg_t *msg, void *user_data);

// 错误码定义
typedef enum {
    PUBSUB_OK = 0,
    PUBSUB_ERR_INVALID_PARAM,
    PUBSUB_ERR_NO_MEMORY,
    PUBSUB_ERR_TOPIC_EXISTS,
    PUBSUB_ERR_TOPIC_NOT_FOUND,
    PUBSUB_ERR_QUEUE_FULL,
    PUBSUB_ERR_MAX_SUBSCRIBERS
} pubsub_err_t;

// 主要API函数声明
pubsub_err_t pubsub_init(void);
pubsub_err_t pubsub_deinit(void);
pubsub_err_t pubsub_create_topic(const char *topic_name);
pubsub_err_t pubsub_delete_topic(const char *topic_name);
pubsub_err_t pubsub_subscribe(const char *topic_name, subscriber_callback_t callback, void *user_data);
pubsub_err_t pubsub_unsubscribe(const char *topic_name, subscriber_callback_t callback);
pubsub_err_t pubsub_publish(const char *topic_name, const uint8_t *data, uint32_t data_len, msg_priority_t priority);

#endif /* PUBSUB_CORE_H */ 