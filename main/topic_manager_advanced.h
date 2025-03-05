#ifndef TOPIC_MANAGER_ADVANCED_H
#define TOPIC_MANAGER_ADVANCED_H

#include "pubsub_core.h"
#include <stdint.h>

// 主题统计信息
typedef struct {
    uint32_t msg_received;
    uint32_t msg_published;
    uint32_t msg_dropped;
    uint32_t subscriber_count;
    uint64_t last_msg_timestamp;
    uint32_t queue_space_available;
} topic_stats_t;

// 主题过滤器
typedef struct {
    char pattern[MAX_TOPIC_NAME_LENGTH];
    bool include_subtopics;
} topic_filter_t;

// 主题QoS级别
typedef enum {
    TOPIC_QOS_AT_MOST_ONCE = 0,
    TOPIC_QOS_AT_LEAST_ONCE = 1,
    TOPIC_QOS_EXACTLY_ONCE = 2
} topic_qos_t;

// 主题配置
typedef struct {
    uint32_t max_msg_size;
    uint32_t queue_size;
    topic_qos_t qos_level;
    bool retain_last_message;
    uint32_t message_ttl;  // Time to live in milliseconds
} topic_config_t;

// 高级主题管理API
esp_err_t topic_manager_init_advanced(void);
esp_err_t topic_create_with_config(const char *topic_name, const topic_config_t *config);
esp_err_t topic_delete_with_cleanup(const char *topic_name);
esp_err_t topic_get_stats(const char *topic_name, topic_stats_t *stats);
esp_err_t topic_set_filter(const topic_filter_t *filter);
esp_err_t topic_clear_filter(void);
esp_err_t topic_flush_messages(const char *topic_name);
esp_err_t topic_get_retained_message(const char *topic_name, pubsub_msg_t *msg);

#endif /* TOPIC_MANAGER_ADVANCED_H */ 