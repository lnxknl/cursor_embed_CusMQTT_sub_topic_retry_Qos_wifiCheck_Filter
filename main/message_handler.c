#include "message_handler.h"
#include "memory_pool.h"
#include "error_handler.h"
#include "esp_log.h"
#include "freertos/timers.h"
#include <string.h>

#define TAG "MSG_HANDLER"
#define MAX_PENDING_MESSAGES 100
#define MAX_RETRY_COUNT 5

typedef struct pending_message {
    uint32_t msg_id;
    char topic[MAX_TOPIC_NAME_LENGTH];
    uint8_t *data;
    uint32_t data_len;
    msg_priority_t priority;
    topic_qos_t qos;
    uint32_t retry_count;
    uint32_t max_retries;
    TimerHandle_t retry_timer;
    struct pending_message *next;
} pending_message_t;

static struct {
    message_handler_config_t config;
    pending_message_t *pending_messages;
    uint32_t next_msg_id;
    SemaphoreHandle_t lock;
} message_handler_ctx;

static void retry_timer_callback(TimerHandle_t timer) {
    pending_message_t *msg = (pending_message_t *)pvTimerGetTimerID(timer);
    
    if (msg->retry_count >= msg->max_retries) {
        ERROR_REPORT(ERROR_LEVEL_WARNING, ERROR_CODE_TIMEOUT,
                    "Message %d to topic %s exceeded max retries",
                    msg->msg_id, msg->topic);
        return;
    }

    // 重试发布消息
    esp_err_t err = pubsub_publish(msg->topic, msg->data, msg->data_len, msg->priority);
    if (err != ESP_OK) {
        ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                    "Failed to retry message %d to topic %s",
                    msg->msg_id, msg->topic);
    }

    msg->retry_count++;
}

esp_err_t message_handler_init(const message_handler_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    message_handler_ctx.lock = xSemaphoreCreateMutex();
    if (message_handler_ctx.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(&message_handler_ctx.config, config, sizeof(message_handler_config_t));
    message_handler_ctx.pending_messages = NULL;
    message_handler_ctx.next_msg_id = 1;

    return ESP_OK;
}

static pending_message_t *create_pending_message(const char *topic_name,
                                               const uint8_t *data,
                                               uint32_t data_len,
                                               msg_priority_t priority,
                                               topic_qos_t qos) {
    pending_message_t *msg = memory_pool_alloc(sizeof(pending_message_t));
    if (msg == NULL) {
        return NULL;
    }

    msg->data = memory_pool_alloc(data_len);
    if (msg->data == NULL) {
        memory_pool_free(msg);
        return NULL;
    }

    memcpy(msg->data, data, data_len);
    strncpy(msg->topic, topic_name, MAX_TOPIC_NAME_LENGTH - 1);
    msg->topic[MAX_TOPIC_NAME_LENGTH - 1] = '\0';
    msg->data_len = data_len;
    msg->priority = priority;
    msg->qos = qos;
    msg->retry_count = 0;
    msg->max_retries = message_handler_ctx.config.retry_count;
    msg->next = NULL;

    // 创建重试定时器
    char timer_name[32];
    snprintf(timer_name, sizeof(timer_name), "retry_timer_%u", msg->msg_id);
    msg->retry_timer = xTimerCreate(timer_name,
                                  pdMS_TO_TICKS(message_handler_ctx.config.retry_interval_ms),
                                  pdTRUE,
                                  msg,
                                  retry_timer_callback);

    if (msg->retry_timer == NULL) {
        memory_pool_free(msg->data);
        memory_pool_free(msg);
        return NULL;
    }

    return msg;
}

esp_err_t message_publish_with_qos(const char *topic_name,
                                 const uint8_t *data,
                                 uint32_t data_len,
                                 msg_priority_t priority,
                                 topic_qos_t qos,
                                 uint32_t *msg_id) {
    if (topic_name == NULL || data == NULL || data_len == 0 || msg_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(message_handler_ctx.lock, portMAX_DELAY);

    // 分配消息ID
    *msg_id = message_handler_ctx.next_msg_id++;

    // 对于QoS > 0的消息，创建待处理消息
    if (qos > TOPIC_QOS_AT_MOST_ONCE) {
        pending_message_t *pending = create_pending_message(topic_name,
                                                         data,
                                                         data_len,
                                                         priority,
                                                         qos);
        if (pending == NULL) {
            xSemaphoreGive(message_handler_ctx.lock);
            return ESP_ERR_NO_MEM;
        }

        pending->msg_id = *msg_id;

        // 添加到待处理列表
        pending->next = message_handler_ctx.pending_messages;
        message_handler_ctx.pending_messages = pending;

        // 启动重试定时器
        xTimerStart(pending->retry_timer, 0);
    }

    // 发布消息
    esp_err_t err = pubsub_publish(topic_name, data, data_len, priority);

    xSemaphoreGive(message_handler_ctx.lock);
    return err;
} 