#include "ws_frame.h"
#include "esp_log.h"
#include <string.h>

#define TAG "WS_FRAME"

// 编码 WebSocket 帧
esp_err_t ws_encode_frame(const ws_message_t *message, uint8_t *buffer,
                         size_t buffer_len, size_t *frame_len) {
    if (!message || !buffer || !frame_len) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 计算帧长度
    size_t header_len = 2; // 基本头部长度
    uint64_t payload_len = message->payload_len;
    
    if (payload_len > 125) {
        if (payload_len <= 0xFFFF) {
            header_len += 2;
        } else {
            header_len += 8;
        }
    }
    
    // 检查缓冲区大小
    if (buffer_len < header_len + payload_len) {
        return ESP_ERR_NO_MEM;
    }
    
    // 设置基本头部
    buffer[0] = (message->is_final ? 0x80 : 0) | (message->opcode & 0x0F);
    
    // 设置长度
    if (payload_len <= 125) {
        buffer[1] = payload_len;
    } else if (payload_len <= 0xFFFF) {
        buffer[1] = 126;
        buffer[2] = (payload_len >> 8) & 0xFF;
        buffer[3] = payload_len & 0xFF;
    } else {
        buffer[1] = 127;
        for (int i = 0; i < 8; i++) {
            buffer[2 + i] = (payload_len >> ((7 - i) * 8)) & 0xFF;
        }
    }
    
    // 复制载荷
    if (payload_len > 0 && message->payload) {
        memcpy(buffer + header_len, message->payload, payload_len);
    }
    
    *frame_len = header_len + payload_len;
    return ESP_OK;
}

// 解码 WebSocket 帧
esp_err_t ws_decode_frame(const uint8_t *buffer, size_t buffer_len,
                         ws_message_t *message, size_t *frame_len) {
    if (!buffer || !message || !frame_len || buffer_len < 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 解析基本头部
    ws_frame_header_t header;
    header.fin = (buffer[0] >> 7) & 0x01;
    header.rsv1 = (buffer[0] >> 6) & 0x01;
    header.rsv2 = (buffer[0] >> 5) & 0x01;
    header.rsv3 = (buffer[0] >> 4) & 0x01;
    header.opcode = buffer[0] & 0x0F;
    header.mask = (buffer[1] >> 7) & 0x01;
    header.payload_len = buffer[1] & 0x7F;
    
    // 检查保留位
    if (header.rsv1 || header.rsv2 || header.rsv3) {
        ESP_LOGE(TAG, "Reserved bits must be 0");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 计算头部长度
    size_t header_len = 2;
    uint64_t payload_len = header.payload_len;
    
    if (payload_len == 126) {
        if (buffer_len < 4) return ESP_ERR_INVALID_SIZE;
        payload_len = (buffer[2] << 8) | buffer[3];
        header_len += 2;
    } else if (payload_len == 127) {
        if (buffer_len < 10) return ESP_ERR_INVALID_SIZE;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | buffer[2 + i];
        }
        header_len += 8;
    }
    
    // 处理掩码
    uint8_t mask_key[4];
    if (header.mask) {
        if (buffer_len < header_len + 4) return ESP_ERR_INVALID_SIZE;
        memcpy(mask_key, buffer + header_len, 4);
        header_len += 4;
    }
    
    // 检查缓冲区大小
    if (buffer_len < header_len + payload_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 设置消息属性
    message->is_final = header.fin;
    message->opcode = header.opcode;
    message->payload_len = payload_len;
    
    // 分配并复制载荷
    if (payload_len > 0) {
        message->payload = malloc(payload_len);
        if (!message->payload) {
            return ESP_ERR_NO_MEM;
        }
        
        memcpy(message->payload, buffer + header_len, payload_len);
        
        // 应用掩码
        if (header.mask) {
            for (size_t i = 0; i < payload_len; i++) {
                message->payload[i] ^= mask_key[i % 4];
            }
        }
    } else {
        message->payload = NULL;
    }
    
    *frame_len = header_len + payload_len;
    return ESP_OK;
} 