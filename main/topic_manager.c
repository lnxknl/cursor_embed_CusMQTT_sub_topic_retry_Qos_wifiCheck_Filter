#include "pubsub_core.h"
#include "memory_pool.h"
#include <string.h>

typedef struct subscriber {
    subscriber_callback_t callback;
    void *user_data;
    struct subscriber *next;
} subscriber_t;

typedef struct topic {
    char name[MAX_TOPIC_NAME_LENGTH];
    subscriber_t *subscribers;
    QueueHandle_t msg_queue;
    uint32_t subscriber_count;
    SemaphoreHandle_t lock;
} topic_t;

static topic_t topics[MAX_TOPICS];
static uint32_t topic_count = 0;
static SemaphoreHandle_t topics_lock = NULL;

static void topic_task(void *pvParameters) {
    topic_t *topic = (topic_t *)pvParameters;
    pubsub_msg_t msg;

    while (1) {
        if (xQueueReceive(topic->msg_queue, &msg, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(topic->lock, portMAX_DELAY);
            
            subscriber_t *current = topic->subscribers;
            while (current != NULL) {
                current->callback(&msg, current->user_data);
                current = current->next;
            }

            // 释放消息数据
            if (msg.data != NULL) {
                memory_pool_free(msg.data);
            }

            xSemaphoreGive(topic->lock);
        }
    }
}

pubsub_err_t pubsub_init(void) {
    if (topics_lock != NULL) {
        return PUBSUB_OK; // 已经初始化
    }

    topics_lock = xSemaphoreCreateMutex();
    if (topics_lock == NULL) {
        return PUBSUB_ERR_NO_MEMORY;
    }

    memset(topics, 0, sizeof(topics));
    return PUBSUB_OK;
}

pubsub_err_t pubsub_create_topic(const char *topic_name) {
    if (topic_name == NULL || strlen(topic_name) >= MAX_TOPIC_NAME_LENGTH) {
        return PUBSUB_ERR_INVALID_PARAM;
    }

    xSemaphoreTake(topics_lock, portMAX_DELAY);

    // 检查主题是否已存在
    for (uint32_t i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            xSemaphoreGive(topics_lock);
            return PUBSUB_ERR_TOPIC_EXISTS;
        }
    }

    if (topic_count >= MAX_TOPICS) {
        xSemaphoreGive(topics_lock);
        return PUBSUB_ERR_NO_MEMORY;
    }

    topic_t *topic = &topics[topic_count];
    strncpy(topic->name, topic_name, MAX_TOPIC_NAME_LENGTH - 1);
    topic->name[MAX_TOPIC_NAME_LENGTH - 1] = '\0';
    
    topic->msg_queue = xQueueCreate(MAX_QUEUE_SIZE, sizeof(pubsub_msg_t));
    if (topic->msg_queue == NULL) {
        xSemaphoreGive(topics_lock);
        return PUBSUB_ERR_NO_MEMORY;
    }

    topic->lock = xSemaphoreCreateMutex();
    if (topic->lock == NULL) {
        vQueueDelete(topic->msg_queue);
        xSemaphoreGive(topics_lock);
        return PUBSUB_ERR_NO_MEMORY;
    }

    topic->subscribers = NULL;
    topic->subscriber_count = 0;

    // 创建主题处理任务
    char task_name[32];
    snprintf(task_name, sizeof(task_name), "topic_%s", topic_name);
    
    BaseType_t ret = xTaskCreate(topic_task, task_name, 4096, topic, 5, NULL);
    if (ret != pdPASS) {
        vSemaphoreDelete(topic->lock);
        vQueueDelete(topic->msg_queue);
        xSemaphoreGive(topics_lock);
        return PUBSUB_ERR_NO_MEMORY;
    }

    topic_count++;
    xSemaphoreGive(topics_lock);
    return PUBSUB_OK;
}

这只是项目的一部分代码。由于回答长度限制，我将分几个部分继续提供其他模块的实现，包括：

1. 订阅者管理的实现
2. 发布者接口的实现
3. 网络通信层
4. 错误处理和日志系统
5. 示例应用程序

是否需要我继续提供后续的代码实现？ 