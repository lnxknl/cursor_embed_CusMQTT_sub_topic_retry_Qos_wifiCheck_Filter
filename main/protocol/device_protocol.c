#include "device_protocol.h"
#include "error_handler.h"
#include "esp_log.h"
#include <string.h>

#define TAG "PROTOCOL"

static device_info_t device_info;
static uint32_t sequence_counter = 0;

// CRC16 查找表
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    // ... (完整的CRC16表)
};

esp_err_t protocol_init(const device_info_t *info) {
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&device_info, info, sizeof(device_info_t));
    sequence_counter = 0;

    return ESP_OK;
}

static uint16_t calculate_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ data[i]];
    }
    
    return crc;
}

esp_err_t protocol_pack_message(message_type_t type, const uint8_t *payload,
                              size_t payload_len, uint8_t *buffer,
                              size_t *buffer_len) {
    if (payload == NULL && payload_len > 0 || buffer == NULL || buffer_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (*buffer_len < sizeof(protocol_header_t) + payload_len) {
        return ESP_ERR_NO_MEM;
    }

    protocol_header_t *header = (protocol_header_t *)buffer;
    header->version = PROTOCOL_VERSION;
    header->type = type;
    header->length = payload_len;
    header->sequence = ++sequence_counter;
    header->timestamp = esp_timer_get_time() / 1000; // 转换为毫秒

    if (payload_len > 0) {
        memcpy(buffer + sizeof(protocol_header_t), payload, payload_len);
    }

    // 计算校验和
    header->checksum = calculate_crc16((uint8_t *)header,
                                     sizeof(protocol_header_t) - sizeof(uint16_t));
    if (payload_len > 0) {
        header->checksum ^= calculate_crc16(payload, payload_len);
    }

    *buffer_len = sizeof(protocol_header_t) + payload_len;
    return ESP_OK;
}

esp_err_t protocol_unpack_message(const uint8_t *buffer, size_t buffer_len,
                                protocol_header_t *header, uint8_t *payload,
                                size_t *payload_len) {
    if (buffer == NULL || header == NULL || 
        (payload == NULL && *payload_len > 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (buffer_len < sizeof(protocol_header_t)) {
        return ESP_ERR_INVALID_SIZE;
    }

    // 复制头部
    memcpy(header, buffer, sizeof(protocol_header_t));

    // 验证版本
    if (header->version != PROTOCOL_VERSION) {
        ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_INVALID_PARAMETER,
                    "Invalid protocol version: %d", header->version);
        return ESP_ERR_INVALID_VERSION;
    }

    // 验证长度
    if (buffer_len < sizeof(protocol_header_t) + header->length) {
        ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_INVALID_PARAMETER,
                    "Invalid message length");
        return ESP_ERR_INVALID_SIZE;
    }

    // 验证校验和
    uint16_t calculated_checksum = calculate_crc16(buffer,
                                                 sizeof(protocol_header_t) - sizeof(uint16_t));
    if (header->length > 0) {
        calculated_checksum ^= calculate_crc16(buffer + sizeof(protocol_header_t),
                                             header->length);
    }

    if (calculated_checksum != header->checksum) {
        ERROR_REPORT(ERROR_LEVEL_ERROR, ERROR_CODE_INVALID_PARAMETER,
                    "Checksum verification failed");
        return ESP_ERR_INVALID_CRC;
    }

    // 复制payload
    if (header->length > 0) {
        if (*payload_len < header->length) {
            return ESP_ERR_NO_MEM;
        }
        memcpy(payload, buffer + sizeof(protocol_header_t), header->length);
    }
    *payload_len = header->length;

    return ESP_OK;
} 