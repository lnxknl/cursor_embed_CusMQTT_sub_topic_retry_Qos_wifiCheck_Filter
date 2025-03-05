#include "network_layer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/event_groups.h"
#include <string.h>

#define TAG "NETWORK"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT     BIT1
#define MQTT_CONNECTED_BIT BIT2

static EventGroupHandle_t network_event_group;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static network_config_t current_config;
static network_status_t current_status = NETWORK_STATUS_DISCONNECTED;
static network_event_callback_t user_callback = NULL;
static void *user_callback_data = NULL;

// WiFi事件处理
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(network_event_group, WIFI_CONNECTED_BIT);
        current_status = NETWORK_STATUS_DISCONNECTED;
        if (user_callback) {
            user_callback(current_status, user_callback_data);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(network_event_group, WIFI_CONNECTED_BIT);
    }
}

// MQTT事件处理
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected to broker");
            xEventGroupSetBits(network_event_group, MQTT_CONNECTED_BIT);
            current_status = NETWORK_STATUS_CONNECTED;
            if (user_callback) {
                user_callback(current_status, user_callback_data);
            }
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected from broker");
            xEventGroupClearBits(network_event_group, MQTT_CONNECTED_BIT);
            current_status = NETWORK_STATUS_DISCONNECTED;
            if (user_callback) {
                user_callback(current_status, user_callback_data);
            }
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT Subscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT Unsubscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT Published, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT Data received");
            // 处理接收到的数据
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT Error occurred");
            current_status = NETWORK_STATUS_ERROR;
            if (user_callback) {
                user_callback(current_status, user_callback_data);
            }
            break;
            
        default:
            ESP_LOGI(TAG, "Other MQTT event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

esp_err_t network_init(const network_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 创建事件组
    network_event_group = xEventGroupCreate();
    if (network_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // 保存配置
    memcpy(&current_config, config, sizeof(network_config_t));

    // 初始化WiFi
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        return ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    // 注册WiFi事件处理程序
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_got_ip));

    // 配置WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    memcpy(wifi_config.sta.ssid, config->wifi_ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, config->wifi_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    // 配置MQTT客户端
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = config->mqtt_broker,
        .port = config->mqtt_port,
        .username = config->mqtt_username,
        .password = config->mqtt_password,
        .client_id = config->client_id,
        .event_handle = mqtt_event_handler,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        return ESP_FAIL;
    }

    return ESP_OK;
} 