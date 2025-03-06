#ifndef WS_COMPRESSION_H
#define WS_COMPRESSION_H

#include "esp_err.h"
#include <stdint.h>
#include <zlib.h>

// 压缩级别
typedef enum {
    WS_COMPRESS_NONE = 0,
    WS_COMPRESS_FAST = 1,
    WS_COMPRESS_DEFAULT = 6,
    WS_COMPRESS_BEST = 9
} ws_compress_level_t;

// 压缩配置
typedef struct {
    ws_compress_level_t level;     // 压缩级别
    bool context_takeover;         // 是否保持压缩上下文
    uint32_t max_window_bits;      // 最大窗口大小
} ws_compression_config_t;

// 压缩上下文
typedef struct {
    z_stream deflate_stream;       // 压缩流
    z_stream inflate_stream;       // 解压流
    bool is_initialized;           // 是否已初始化
    ws_compression_config_t config;// 配置
} ws_compression_ctx_t;

// 压缩API
esp_err_t ws_compression_init(ws_compression_ctx_t *ctx, 
                            const ws_compression_config_t *config);
esp_err_t ws_compression_compress(ws_compression_ctx_t *ctx,
                                const uint8_t *input, size_t input_len,
                                uint8_t *output, size_t *output_len);
esp_err_t ws_compression_decompress(ws_compression_ctx_t *ctx,
                                  const uint8_t *input, size_t input_len,
                                  uint8_t *output, size_t *output_len);
esp_err_t ws_compression_reset(ws_compression_ctx_t *ctx);
esp_err_t ws_compression_destroy(ws_compression_ctx_t *ctx);

#endif /* WS_COMPRESSION_H */ 