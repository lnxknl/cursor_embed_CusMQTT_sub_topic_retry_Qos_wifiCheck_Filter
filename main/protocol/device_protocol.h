#ifndef DEVICE_PROTOCOL_H
#define DEVICE_PROTOCOL_H

#include "esp_err.h"
#include <stdint.h>

// 协议版本
#define PROTOCOL_VERSION 1

// 消息类型
typedef enum {
    MSG_TYPE_HELLO = 0,
    MSG_TYPE_ACK,
    MSG_TYPE_DATA,
    MSG_TYPE_COMMAND,
    MSG_TYPE_STATUS,
    MSG_TYPE_ERROR
} message_type_t;

// 协议头部
typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t sequence;
    uint32_t timestamp;
    uint16_t checksum;
} protocol_header_t;

// 设备信息
typedef struct {
    char device_id[32];
    char device_type[16];
    uint32_t capabilities;
    uint8_t protocol_version;
} device_info_t;

// 协议API
esp_err_t protocol_init(const device_info_t *info);
esp_err_t protocol_pack_message(message_type_t type, const uint8_t *payload,
                              size_t payload_len, uint8_t *buffer,
                              size_t *buffer_len);
esp_err_t protocol_unpack_message(const uint8_t *buffer, size_t buffer_len,
                                protocol_header_t *header, uint8_t *payload,
                                size_t *payload_len);
esp_err_t protocol_verify_checksum(const protocol_header_t *header,
                                 const uint8_t *payload, size_t payload_len);
esp_err_t protocol_generate_checksum(protocol_header_t *header,
                                   const uint8_t *payload, size_t payload_len);

#endif /* DEVICE_PROTOCOL_H */ 