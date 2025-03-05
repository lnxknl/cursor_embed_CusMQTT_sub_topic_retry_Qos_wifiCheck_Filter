#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stdint.h>
#include "esp_err.h"

// 内存块大小定义
#define MEMORY_BLOCK_SIZE 128
#define MEMORY_POOL_BLOCKS 100

typedef struct memory_pool_t memory_pool_t;

// 内存池API
esp_err_t memory_pool_init(void);
void *memory_pool_alloc(size_t size);
void memory_pool_free(void *ptr);
void memory_pool_stats(size_t *total, size_t *used, size_t *peak);

#endif /* MEMORY_POOL_H */ 