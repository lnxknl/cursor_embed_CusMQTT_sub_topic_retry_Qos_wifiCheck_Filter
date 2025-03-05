#include "error_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdarg.h>
#include <string.h>

#define TAG "ERROR_HANDLER"
#define ERROR_QUEUE_SIZE 20
#define ERROR_MESSAGE_MAX_LENGTH 256

static QueueHandle_t error_queue = NULL;
static error_callback_t error_callback = NULL;
static void *error_callback_data = NULL;
static TaskHandle_t error_task_handle = NULL;

// 错误处理任务
static void error_handler_task(void *pvParameters) {
    error_info_t error;
    char message_buffer[ERROR_MESSAGE_MAX_LENGTH];

    while (1) {
        if (xQueueReceive(error_queue, &error, portMAX_DELAY) == pdTRUE) {
            // 打印错误信息到日志
            const char *level_str;
            switch (error.level) {
                case ERROR_LEVEL_INFO:
                    level_str = "INFO";
                    break;
                case ERROR_LEVEL_WARNING:
                    level_str = "WARNING";
                    break;
                case ERROR_LEVEL_ERROR:
                    level_str = "ERROR";
                    break;
                case ERROR_LEVEL_FATAL:
                    level_str = "FATAL";
                    break;
                default:
                    level_str = "UNKNOWN";
                    break;
            }

            ESP_LOGI(TAG, "[%s] Code: %d, File: %s, Line: %d, Function: %s, Message: %s",
                     level_str, error.code, error.file, error.line, error.func, error.message);

            // 调用用户回调函数
            if (error_callback != NULL) {
                error_callback(&error, error_callback_data);
            }

            // 对于致命错误，重启系统
            if (error.level == ERROR_LEVEL_FATAL) {
                ESP_LOGE(TAG, "Fatal error occurred, restarting system...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            }
        }
    }
}

esp_err_t error_handler_init(void) {
    // 创建错误队列
    error_queue = xQueueCreate(ERROR_QUEUE_SIZE, sizeof(error_info_t));
    if (error_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // 创建错误处理任务
    BaseType_t ret = xTaskCreate(error_handler_task,
                                "error_handler",
                                4096,
                                NULL,
                                configMAX_PRIORITIES - 1,
                                &error_task_handle);
    if (ret != pdPASS) {
        vQueueDelete(error_queue);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void error_handler_register_callback(error_callback_t callback, void *user_data) {
    error_callback = callback;
    error_callback_data = user_data;
}

void error_handler_report(error_level_t level, error_code_t code,
                         const char *file, int line, const char *func,
                         const char *format, ...) {
    if (error_queue == NULL) {
        return;
    }

    error_info_t error;
    error.level = level;
    error.code = code;
    error.file = file;
    error.line = line;
    error.func = func;
    error.timestamp = esp_timer_get_time();

    // 格式化错误消息
    va_list args;
    va_start(args, format);
    static char message_buffer[ERROR_MESSAGE_MAX_LENGTH];
    vsnprintf(message_buffer, ERROR_MESSAGE_MAX_LENGTH, format, args);
    va_end(args);
    error.message = message_buffer;

    // 发送错误信息到队列
    if (xQueueSend(error_queue, &error, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Error queue is full, message dropped");
    }
} 