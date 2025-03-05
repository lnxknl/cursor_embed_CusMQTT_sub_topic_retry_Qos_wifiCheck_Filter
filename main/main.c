#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "pubsub_core.h"
#include "network_layer.h"
#include "error_handler.h"

#define TAG "MAIN"

// 示例消息回调函数
static void message_callback(const pubsub_msg_t *msg, void *user_data) {
    ESP_LOGI(TAG, "Received message on topic: %s", msg->topic);
    ESP_LOGI(TAG, "Message length: %d bytes", msg->data_len);
    ESP_LOGI(TAG, "Message priority: %d", msg->priority);
    
    if (msg->data != NULL && msg->data_len > 0) {
        ESP_LOGI(TAG, "Message data: %.*s", msg->data_len, (char *)msg->data);
    }
}

// 网络状态回调函数
static void network_status_callback(network_status_t status, void *user_data) {
    switch (status) {
        case NETWORK_STATUS_CONNECTED:
            ESP_LOGI(TAG, "Network connected");
            break;
        case NETWORK_STATUS_DISCONNECTED:
            ESP_LOGI(TAG, "Network disconnected");
            break;
        case NETWORK_STATUS_ERROR:
            ESP_LOGI(TAG, "Network error");
            break;
        default:
            break;
    }
}

// 错误处理回调函数
static void error_callback(const error_info_t *error, void *user_data) {
    ESP_LOGI(TAG, "Error callback triggered");
    // 这里可以添加自定义错误处理逻辑
}

void app_main(void) {
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化错误处理系统
    ESP_ERROR_CHECK(error_handler_init());
    error_handler_register_callback(error_callback, NULL);

    // 初始化发布-订阅系统
    ESP_ERROR_CHECK(pubsub_init());

    // 配置网络
    network_config_t network_config = {
        .wifi_ssid = "YourWiFiSSID",
        .wifi_password = "YourWiFiPassword",
        .mqtt_broker = "mqtt://your-broker.com",
        .mqtt_port = 1883,
        .mqtt_username = "your_username",
        .mqtt_password = "your_password",
        .client_id = "esp32_device_001"
    };

    // 初始化网络
    ESP_ERROR_CHECK(network_init(&network_config));
    network_register_callback(network_status_callback, NULL);

    // 创建示例主题
    ESP_ERROR_CHECK(pubsub_create_topic("sensor/temperature"));
    ESP_ERROR_CHECK(pubsub_create_topic("sensor/humidity"));
    ESP_ERROR_CHECK(pubsub_create_topic("control/led"));

    // 订阅主题
    ESP_ERROR_CHECK(pubsub_subscribe("sensor/temperature", message_callback, NULL));
    ESP_ERROR_CHECK(pubsub_subscribe("sensor/humidity", message_callback, NULL));
    ESP_ERROR_CHECK(pubsub_subscribe("control/led", message_callback, NULL));

    // 示例发布消息
    const char *temp_msg = "25.5";
    ESP_ERROR_CHECK(pubsub_publish("sensor/temperature", 
                                 (const uint8_t *)temp_msg, 
                                 strlen(temp_msg), 
                                 MSG_PRIORITY_NORMAL));

    // 主循环
    while (1) {
        // 这里可以添加其他应用逻辑
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
} 