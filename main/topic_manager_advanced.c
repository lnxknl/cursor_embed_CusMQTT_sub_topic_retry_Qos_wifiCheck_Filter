#include "topic_manager_advanced.h"
#include "memory_pool.h"
#include "error_handler.h"
#include "esp_log.h"
#include <string.h>
#include <regex.h>

#define TAG "TOPIC_MGR_ADV"

typedef struct retained_message {
    pubsub_msg_t msg;
    uint64_t timestamp;
    struct retained_message *next;
} retained_message_t;

typedef struct topic_advanced {
    topic_config_t config;
    topic_stats_t stats;
    retained_message_t *retained_msg;
    SemaphoreHandle_t stats_lock;
} topic_advanced_t;

static topic_advanced_t topic_advanced_data[MAX_TOPICS];
static topic_filter_t current_filter;
static bool filter_active = false;

esp_err_t topic_manager_init_advanced(void) {
    for (int i = 0; i < MAX_TOPICS; i++) {
        topic_advanced_data[i].stats_lock = xSemaphoreCreateMutex();
        if (topic_advanced_data[i].stats_lock == NULL) {
            ERROR_REPORT(ERROR_LEVEL_FATAL, ERROR_CODE_SYSTEM_ERROR,
                        "Failed to create stats lock for topic %d", i);
            return ESP_ERR_NO_MEM;
        }
        memset(&topic_advanced_data[i].stats, 0, sizeof(topic_stats_t));
        topic_advanced_data[i].retained_msg = NULL;
    }
    return ESP_OK;
}

static bool topic_matches_filter(const char *topic_name) {
    if (!filter_active) {
        return true;
    }

    regex_t regex;
    int ret = regcomp(&regex, current_filter.pattern, REG_EXTENDED);
    if (ret != 0) {
        ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_INVALID_PARAMETER,
                    "Invalid filter pattern: %s", current_filter.pattern);
        return false;
    }

    ret = regexec(&regex, topic_name, 0, NULL, 0);
    regfree(&regex);

    return (ret == 0);
}

esp_err_t topic_create_with_config(const char *topic_name, const topic_config_t *config) {
    if (topic_name == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!topic_matches_filter(topic_name)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 查找可用槽位
    int slot = -1;
    for (int i = 0; i < MAX_TOPICS; i++) {
        if (topics[i].name[0] == '\0') {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                    "Maximum number of topics reached");
        return ESP_ERR_NO_MEM;
    }

    // 复制配置
    memcpy(&topic_advanced_data[slot].config, config, sizeof(topic_config_t));

    // 初始化统计信息
    xSemaphoreTake(topic_advanced_data[slot].stats_lock, portMAX_DELAY);
    memset(&topic_advanced_data[slot].stats, 0, sizeof(topic_stats_t));
    topic_advanced_data[slot].stats.last_msg_timestamp = esp_timer_get_time();
    xSemaphoreGive(topic_advanced_data[slot].stats_lock);

    // 创建基本主题
    return pubsub_create_topic(topic_name);
}

esp_err_t topic_delete_with_cleanup(const char *topic_name) {
    if (topic_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 查找主题
    int slot = -1;
    for (int i = 0; i < MAX_TOPICS; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        return ESP_ERR_NOT_FOUND;
    }

    // 清理保留的消息
    xSemaphoreTake(topic_advanced_data[slot].stats_lock, portMAX_DELAY);
    retained_message_t *current = topic_advanced_data[slot].retained_msg;
    while (current != NULL) {
        retained_message_t *next = current->next;
        if (current->msg.data != NULL) {
            memory_pool_free(current->msg.data);
        }
        memory_pool_free(current);
        current = next;
    }
    topic_advanced_data[slot].retained_msg = NULL;

    // 重置统计信息
    memset(&topic_advanced_data[slot].stats, 0, sizeof(topic_stats_t));
    xSemaphoreGive(topic_advanced_data[slot].stats_lock);

    // 删除基本主题
    return pubsub_delete_topic(topic_name);
}

esp_err_t topic_get_stats(const char *topic_name, topic_stats_t *stats) {
    if (topic_name == NULL || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 查找主题
    int slot = -1;
    for (int i = 0; i < MAX_TOPICS; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        return ESP_ERR_NOT_FOUND;
    }

    // 复制统计信息
    xSemaphoreTake(topic_advanced_data[slot].stats_lock, portMAX_DELAY);
    memcpy(stats, &topic_advanced_data[slot].stats, sizeof(topic_stats_t));
    xSemaphoreGive(topic_advanced_data[slot].stats_lock);

    return ESP_OK;
} 