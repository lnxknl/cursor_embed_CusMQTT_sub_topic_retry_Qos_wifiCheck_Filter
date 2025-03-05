#include "mqtt_decoder.h"
#include "mqtt_types.h"
#include <string.h>

// 解码剩余长度
static int decode_remaining_length(const uint8_t *buf, int buf_len, uint32_t *value) {
    if (buf_len < 1) return -1;
    
    int multiplier = 1;
    uint32_t length = 0;
    uint8_t byte;
    int decoded = 0;
    
    do {
        if (decoded >= buf_len) return -1;
        byte = buf[decoded++];
        length += (byte & 127) * multiplier;
        multiplier *= 128;
        if (multiplier > 128*128*128) return -1;
    } while ((byte & 128) != 0);
    
    *value = length;
    return decoded;
}

// 解码字符串
static int decode_string(const uint8_t *buf, int buf_len, char *str, int str_len) {
    if (buf_len < 2) return -1;
    
    uint16_t length = (buf[0] << 8) | buf[1];
    if (buf_len < length + 2 || str_len <= length) return -1;
    
    memcpy(str, buf + 2, length);
    str[length] = '\0';
    
    return length + 2;
}

// 解码固定头部
int mqtt_decode_fixed_header(const uint8_t *buf, int buf_len, 
                           mqtt_fixed_header_t *header) {
    if (!buf || !header || buf_len < 2) return -1;
    
    header->type = (buf[0] >> 4) & 0x0F;
    header->dup_flag = (buf[0] >> 3) & 0x01;
    header->qos_level = (buf[0] >> 1) & 0x03;
    header->retain = buf[0] & 0x01;
    
    int decoded = decode_remaining_length(buf + 1, buf_len - 1, 
                                        &header->remaining_length);
    if (decoded < 0) return -1;
    
    return decoded + 1;
}

// 解码 CONNACK 包
int mqtt_decode_connack(const uint8_t *buf, int buf_len, 
                       uint8_t *session_present, uint8_t *return_code) {
    if (!buf || !session_present || !return_code || buf_len < 4) return -1;
    
    mqtt_fixed_header_t header;
    int pos = mqtt_decode_fixed_header(buf, buf_len, &header);
    if (pos < 0) return -1;
    
    if (header.type != MQTT_CONNACK || header.remaining_length != 2) return -1;
    
    *session_present = buf[pos] & 0x01;
    *return_code = buf[pos + 1];
    
    return pos + 2;
}

// 解码 PUBLISH 包
int mqtt_decode_publish(const uint8_t *buf, int buf_len, mqtt_message_t *message) {
    if (!buf || !message || buf_len < 4) return -1;
    
    mqtt_fixed_header_t header;
    int pos = mqtt_decode_fixed_header(buf, buf_len, &header);
    if (pos < 0) return -1;
    
    if (header.type != MQTT_PUBLISH) return -1;
    
    message->dup = header.dup_flag;
    message->qos = header.qos_level;
    message->retain = header.retain;
    
    // 解码主题
    char topic[256];
    int topic_len = decode_string(buf + pos, buf_len - pos, topic, sizeof(topic));
    if (topic_len < 0) return -1;
    message->topic = strdup(topic);
    pos += topic_len;
    
    // 处理报文标识符
    uint16_t packet_id = 0;
    if (message->qos > 0) {
        if (buf_len - pos < 2) return -1;
        packet_id = (buf[pos] << 8) | buf[pos + 1];
        pos += 2;
    }
    
    // 处理载荷
    int payload_len = header.remaining_length - topic_len;
    if (message->qos > 0) payload_len -= 2;
    
    if (payload_len > 0) {
        uint8_t *payload = malloc(payload_len);
        if (!payload) return -1;
        memcpy(payload, buf + pos, payload_len);
        message->payload = payload;
        message->payload_len = payload_len;
    } else {
        message->payload = NULL;
        message->payload_len = 0;
    }
    
    return pos + payload_len;
}

// 解码 SUBACK 包
int mqtt_decode_suback(const uint8_t *buf, int buf_len, 
                      uint16_t *packet_id, uint8_t *return_codes,
                      int *return_code_count) {
    if (!buf || !packet_id || !return_codes || !return_code_count || 
        buf_len < 4) return -1;
    
    mqtt_fixed_header_t header;
    int pos = mqtt_decode_fixed_header(buf, buf_len, &header);
    if (pos < 0) return -1;
    
    if (header.type != MQTT_SUBACK) return -1;
    
    *packet_id = (buf[pos] << 8) | buf[pos + 1];
    pos += 2;
    
    *return_code_count = header.remaining_length - 2;
    if (*return_code_count <= 0) return -1;
    
    memcpy(return_codes, buf + pos, *return_code_count);
    
    return pos + *return_code_count;
} 