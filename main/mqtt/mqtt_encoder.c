#include "mqtt_encoder.h"
#include "mqtt_types.h"
#include <string.h>

// 编码剩余长度
static int encode_remaining_length(uint32_t length, uint8_t *buf) {
    int encoded = 0;
    do {
        uint8_t byte = length % 128;
        length = length / 128;
        if (length > 0) {
            byte |= 0x80;
        }
        buf[encoded++] = byte;
    } while (length > 0 && encoded < 4);
    
    return encoded;
}

// 编码字符串
static int encode_string(const char *str, uint8_t *buf) {
    uint16_t len = strlen(str);
    buf[0] = len >> 8;
    buf[1] = len & 0xFF;
    memcpy(buf + 2, str, len);
    return len + 2;
}

// 编码 CONNECT 包
int mqtt_encode_connect(const mqtt_connect_options_t *options, uint8_t *buf, int buf_len) {
    if (!options || !buf || buf_len < 10) {
        return -1;
    }

    int pos = 0;
    
    // 固定头部
    buf[pos++] = MQTT_CONNECT << 4;
    
    // 计算剩余长度
    int remaining_length = 10; // 协议名长度(6) + 协议级别(1) + 连接标志(1) + 保持连接(2)
    
    // 计算可变头部和载荷的长度
    remaining_length += 2 + strlen(options->client_id); // 客户端标识符
    
    if (options->will.topic && options->will.message) {
        remaining_length += 2 + strlen(options->will.topic);
        remaining_length += 2 + strlen(options->will.message);
    }
    
    if (options->username) {
        remaining_length += 2 + strlen(options->username);
        if (options->password) {
            remaining_length += 2 + strlen(options->password);
        }
    }
    
    // 编码剩余长度
    pos += encode_remaining_length(remaining_length, buf + pos);
    
    // 协议名
    buf[pos++] = 0;
    buf[pos++] = 4;
    memcpy(buf + pos, "MQTT", 4);
    pos += 4;
    
    // 协议级别
    buf[pos++] = 4; // MQTT 3.1.1
    
    // 连接标志
    uint8_t connect_flags = 0;
    if (options->clean_session) {
        connect_flags |= 0x02;
    }
    if (options->will.topic && options->will.message) {
        connect_flags |= 0x04;
        connect_flags |= (options->will.qos << 3);
        if (options->will.retain) {
            connect_flags |= 0x20;
        }
    }
    if (options->username) {
        connect_flags |= 0x80;
        if (options->password) {
            connect_flags |= 0x40;
        }
    }
    buf[pos++] = connect_flags;
    
    // 保持连接
    buf[pos++] = options->keep_alive >> 8;
    buf[pos++] = options->keep_alive & 0xFF;
    
    // 载荷
    pos += encode_string(options->client_id, buf + pos);
    
    if (options->will.topic && options->will.message) {
        pos += encode_string(options->will.topic, buf + pos);
        pos += encode_string(options->will.message, buf + pos);
    }
    
    if (options->username) {
        pos += encode_string(options->username, buf + pos);
        if (options->password) {
            pos += encode_string(options->password, buf + pos);
        }
    }
    
    return pos;
}

// 编码 PUBLISH 包
int mqtt_encode_publish(const mqtt_message_t *message, uint16_t packet_id, 
                       uint8_t *buf, int buf_len) {
    if (!message || !buf || buf_len < 2) {
        return -1;
    }

    int pos = 0;
    
    // 固定头部
    uint8_t header = MQTT_PUBLISH << 4;
    if (message->dup) {
        header |= 0x08;
    }
    header |= (message->qos << 1);
    if (message->retain) {
        header |= 0x01;
    }
    buf[pos++] = header;
    
    // 计算剩余长度
    int remaining_length = 2 + strlen(message->topic); // 主题长度
    if (message->qos > 0) {
        remaining_length += 2; // 报文标识符
    }
    remaining_length += message->payload_len;
    
    // 编码剩余长度
    pos += encode_remaining_length(remaining_length, buf + pos);
    
    // 可变头部
    pos += encode_string(message->topic, buf + pos);
    
    if (message->qos > 0) {
        buf[pos++] = packet_id >> 8;
        buf[pos++] = packet_id & 0xFF;
    }
    
    // 载荷
    memcpy(buf + pos, message->payload, message->payload_len);
    pos += message->payload_len;
    
    return pos;
}

// 编码 SUBSCRIBE 包
int mqtt_encode_subscribe(uint16_t packet_id, const mqtt_topic_filter_t *topics,
                         int topic_count, uint8_t *buf, int buf_len) {
    if (!topics || !buf || buf_len < 2 || topic_count <= 0) {
        return -1;
    }

    int pos = 0;
    
    // 固定头部
    buf[pos++] = MQTT_SUBSCRIBE << 4 | 0x02;
    
    // 计算剩余长度
    int remaining_length = 2; // 报文标识符
    for (int i = 0; i < topic_count; i++) {
        remaining_length += 2 + strlen(topics[i].topic) + 1;
    }
    
    // 编码剩余长度
    pos += encode_remaining_length(remaining_length, buf + pos);
    
    // 可变头部
    buf[pos++] = packet_id >> 8;
    buf[pos++] = packet_id & 0xFF;
    
    // 载荷
    for (int i = 0; i < topic_count; i++) {
        pos += encode_string(topics[i].topic, buf + pos);
        buf[pos++] = topics[i].qos;
    }
    
    return pos;
}

// 编码 UNSUBSCRIBE 包
int mqtt_encode_unsubscribe(uint16_t packet_id, const char **topics,
                           int topic_count, uint8_t *buf, int buf_len) {
    if (!topics || !buf || buf_len < 2 || topic_count <= 0) {
        return -1;
    }

    int pos = 0;
    
    // 固定头部
    buf[pos++] = MQTT_UNSUBSCRIBE << 4 | 0x02;
    
    // 计算剩余长度
    int remaining_length = 2; // 报文标识符
    for (int i = 0; i < topic_count; i++) {
        remaining_length += 2 + strlen(topics[i]);
    }
    
    // 编码剩余长度
    pos += encode_remaining_length(remaining_length, buf + pos);
    
    // 可变头部
    buf[pos++] = packet_id >> 8;
    buf[pos++] = packet_id & 0xFF;
    
    // 载荷
    for (int i = 0; i < topic_count; i++) {
        pos += encode_string(topics[i], buf + pos);
    }
    
    return pos;
}

// 编码简单的控制包 (PINGREQ, PINGRESP, DISCONNECT)
int mqtt_encode_simple_packet(mqtt_packet_type_t type, uint8_t *buf, int buf_len) {
    if (!buf || buf_len < 2) {
        return -1;
    }
    
    buf[0] = type << 4;
    buf[1] = 0;
    return 2;
} 