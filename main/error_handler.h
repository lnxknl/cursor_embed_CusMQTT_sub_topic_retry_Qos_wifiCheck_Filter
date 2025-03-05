#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "esp_err.h"
#include <stdint.h>

// 错误级别定义
typedef enum {
    ERROR_LEVEL_INFO = 0,
    ERROR_LEVEL_WARNING,
    ERROR_LEVEL_ERROR,
    ERROR_LEVEL_FATAL
} error_level_t;

// 错误代码定义
typedef enum {
    ERROR_CODE_SUCCESS = 0,
    ERROR_CODE_WIFI_INIT_FAILED,
    ERROR_CODE_MQTT_INIT_FAILED,
    ERROR_CODE_MEMORY_ALLOCATION_FAILED,
    ERROR_CODE_QUEUE_FULL,
    ERROR_CODE_INVALID_PARAMETER,
    ERROR_CODE_TIMEOUT,
    ERROR_CODE_SYSTEM_ERROR
} error_code_t;

// 错误信息结构体
typedef struct {
    error_level_t level;
    error_code_t code;
    const char *file;
    int line;
    const char *func;
    const char *message;
    uint64_t timestamp;
} error_info_t;

// 错误处理回调函数类型
typedef void (*error_callback_t)(const error_info_t *error, void *user_data);

// 错误处理API
esp_err_t error_handler_init(void);
void error_handler_register_callback(error_callback_t callback, void *user_data);
void error_handler_report(error_level_t level, error_code_t code, 
                         const char *file, int line, const char *func, 
                         const char *format, ...);

#define ERROR_REPORT(level, code, ...) \
    error_handler_report(level, code, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#endif /* ERROR_HANDLER_H */ 