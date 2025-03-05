#include "network_manager.h"
#include "error_handler.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include <string.h>

#define TAG "NET_MGR"

static struct {
    network_manager_config_t config;
    network_status_t current_status;
    uint32_t reconnect_attempts;
    bool is_running;
    TaskHandle_t monitor_task;
    EventGroupHandle_t event_group;
    SemaphoreHandle_t lock;
} network_manager_ctx;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT     BIT1

static void network_monitor_task(void *pvParameters) {
    while (network_manager_ctx.is_running) {
        EventBits_t bits = xEventGroupWaitBits(network_manager_ctx.event_group,
                                             WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                             pdFALSE,
                                             pdFALSE,
                                             portMAX_DELAY);

        xSemaphoreTake(network_manager_ctx.lock, portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            network_manager_ctx.current_status = NETWORK_STATUS_CONNECTED;
            network_manager_ctx.reconnect_attempts = 0;
        } else if (bits & WIFI_FAIL_BIT) {
            network_manager_ctx.current_status = NETWORK_STATUS_DISCONNECTED;

            if (network_manager_ctx.config.auto_reconnect &&
                network_manager_ctx.reconnect_attempts < network_manager_ctx.config.max_reconnect_attempts) {
                
                network_manager_ctx.reconnect_attempts++;
                ESP_LOGI(TAG, "Attempting reconnection %d/%d",
                        network_manager_ctx.reconnect_attempts,
                        network_manager_ctx.config.max_reconnect_attempts);

                esp_wifi_connect();
            } else {
                network_manager_ctx.current_status = NETWORK_STATUS_ERROR;
                ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_SYSTEM_ERROR,
                            "Maximum reconnection attempts reached");
            }
        }

        // 调用状态回调
        if (network_manager_ctx.config.status_callback) {
            network_manager_ctx.config.status_callback(network_manager_ctx.current_status,
                                                    network_manager_ctx.config.user_data);
        }

        xSemaphoreGive(network_manager_ctx.lock);

        // 重连延时
        if (bits & WIFI_FAIL_BIT) {
            vTaskDelay(pdMS_TO_TICKS(network_manager_ctx.config.reconnect_interval_ms));
        }
    }

    vTaskDelete(NULL);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(network_manager_ctx.event_group, WIFI_FAIL_BIT);
        xEventGroupClearBits(network_manager_ctx.event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(network_manager_ctx.event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(network_manager_ctx.event_group, WIFI_FAIL_BIT);
    }
}

esp_err_t network_manager_init(const network_manager_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    network_manager_ctx.lock = xSemaphoreCreateMutex();
    if (network_manager_ctx.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    network_manager_ctx.event_group = xEventGroupCreate();
    if (network_manager_ctx.event_group == NULL) {
        vSemaphoreDelete(network_manager_ctx.lock);
        return ESP_ERR_NO_MEM;
    }

    memcpy(&network_manager_ctx.config, config, sizeof(network_manager_config_t));
    network_manager_ctx.current_status = NETWORK_STATUS_DISCONNECTED;
    network_manager_ctx.reconnect_attempts = 0;
    network_manager_ctx.is_running = false;

    // 注册WiFi事件处理程序
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      NULL));

    return ESP_OK;
} 