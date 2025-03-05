#ifndef MQTT_TYPES_H
#define MQTT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// MQTT 控制包类型
typedef enum {
    MQTT_CONNECT     = 1,  // 客户端请求连接服务端
    MQTT_CONNACK     = 2,  // 连接报文确认
    MQTT_PUBLISH     = 3,  // 发布消息
    MQTT_PUBACK      = 4,  // QoS 1消息发布收到确认
    MQTT_PUBREC      = 5,  // QoS 2消息发布收到
    MQTT_PUBREL      = 6,  // QoS 2消息发布释放
    MQTT_PUBCOMP     = 7,  // QoS 2消息发布完成
    MQTT_SUBSCRIBE   = 8,  // 客户端订阅请求
    MQTT_SUBACK      = 9,  // 订阅请求报文确认
    MQTT_UNSUBSCRIBE = 10, // 客户端取消订阅请求
    MQTT_UNSUBACK    = 11, // 取消订阅报文确认
    MQTT_PINGREQ     = 12, // PING请求
    MQTT_PINGRESP    = 13, // PING响应
    MQTT_DISCONNECT  = 14  // 客户端断开连接
} mqtt_packet_type_t;

// MQTT QoS 等级
typedef enum {
    MQTT_QOS_0 = 0, // 最多分发一次
    MQTT_QOS_1 = 1, // 至少分发一次
    MQTT_QOS_2 = 2  // 只分发一次
} mqtt_qos_t;

// MQTT 连接标志
typedef struct {
    uint8_t clean_session : 1;
    uint8_t will_flag     : 1;
    uint8_t will_qos      : 2;
    uint8_t will_retain   : 1;
    uint8_t password_flag : 1;
    uint8_t username_flag : 1;
    uint8_t reserved      : 1;
} mqtt_connect_flags_t;

// MQTT 固定头部
typedef struct {
    uint8_t type      : 4;
    uint8_t dup_flag  : 1;
    uint8_t qos_level : 2;
    uint8_t retain    : 1;
    uint32_t remaining_length;
} mqtt_fixed_header_t;

// MQTT 连接选项
typedef struct {
    const char *client_id;
    const char *username;
    const char *password;
    uint16_t keep_alive;
    bool clean_session;
    struct {
        const char *topic;
        const char *message;
        mqtt_qos_t qos;
        bool retain;
    } will;
} mqtt_connect_options_t;

// MQTT 主题过滤器
typedef struct {
    const char *topic;
    mqtt_qos_t qos;
} mqtt_topic_filter_t;

// MQTT 消息
typedef struct {
    const char *topic;
    const uint8_t *payload;
    uint32_t payload_len;
    mqtt_qos_t qos;
    bool retain;
    bool dup;
} mqtt_message_t;

#endif /* MQTT_TYPES_H */ 