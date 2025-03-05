#include "memory_pool.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

typedef struct block_header {
    struct block_header *next;
    size_t size;
    bool is_free;
} block_header_t;

struct memory_pool_t {
    uint8_t *pool;
    block_header_t *first_block;
    SemaphoreHandle_t mutex;
    size_t total_size;
    size_t used_size;
    size_t peak_use;
};

static memory_pool_t g_memory_pool;

esp_err_t memory_pool_init(void) {
    // 分配内存池空间
    g_memory_pool.pool = heap_caps_malloc(MEMORY_POOL_BLOCKS * MEMORY_BLOCK_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!g_memory_pool.pool) {
        return ESP_ERR_NO_MEM;
    }

    // 初始化互斥锁
    g_memory_pool.mutex = xSemaphoreCreateMutex();
    if (!g_memory_pool.mutex) {
        heap_caps_free(g_memory_pool.pool);
        return ESP_ERR_NO_MEM;
    }

    // 初始化第一个块
    g_memory_pool.first_block = (block_header_t *)g_memory_pool.pool;
    g_memory_pool.first_block->next = NULL;
    g_memory_pool.first_block->size = MEMORY_POOL_BLOCKS * MEMORY_BLOCK_SIZE - sizeof(block_header_t);
    g_memory_pool.first_block->is_free = true;

    g_memory_pool.total_size = MEMORY_POOL_BLOCKS * MEMORY_BLOCK_SIZE;
    g_memory_pool.used_size = 0;
    g_memory_pool.peak_use = 0;

    return ESP_OK;
}

void *memory_pool_alloc(size_t size) {
    if (size == 0) return NULL;

    xSemaphoreTake(g_memory_pool.mutex, portMAX_DELAY);

    // 对齐大小到8字节边界
    size = (size + 7) & ~7;

    block_header_t *current = g_memory_pool.first_block;
    block_header_t *best_fit = NULL;
    size_t min_size_diff = SIZE_MAX;

    // 查找最佳匹配块
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            size_t size_diff = current->size - size;
            if (size_diff < min_size_diff) {
                min_size_diff = size_diff;
                best_fit = current;
                if (size_diff == 0) break;
            }
        }
        current = current->next;
    }

    if (!best_fit) {
        xSemaphoreGive(g_memory_pool.mutex);
        return NULL;
    }

    // 如果剩余空间足够大，分割块
    if (min_size_diff >= sizeof(block_header_t) + 8) {
        block_header_t *new_block = (block_header_t *)((uint8_t *)best_fit + sizeof(block_header_t) + size);
        new_block->next = best_fit->next;
        new_block->size = min_size_diff - sizeof(block_header_t);
        new_block->is_free = true;

        best_fit->next = new_block;
        best_fit->size = size;
    }

    best_fit->is_free = false;
    g_memory_pool.used_size += best_fit->size + sizeof(block_header_t);
    if (g_memory_pool.used_size > g_memory_pool.peak_use) {
        g_memory_pool.peak_use = g_memory_pool.used_size;
    }

    xSemaphoreGive(g_memory_pool.mutex);
    return (void *)((uint8_t *)best_fit + sizeof(block_header_t));
}

void memory_pool_free(void *ptr) {
    if (!ptr) return;

    xSemaphoreTake(g_memory_pool.mutex, portMAX_DELAY);

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    block->is_free = true;
    g_memory_pool.used_size -= (block->size + sizeof(block_header_t));

    // 合并相邻的空闲块
    block_header_t *current = g_memory_pool.first_block;
    while (current != NULL) {
        if (current->is_free && current->next && current->next->is_free) {
            current->size += current->next->size + sizeof(block_header_t);
            current->next = current->next->next;
            continue;
        }
        current = current->next;
    }

    xSemaphoreGive(g_memory_pool.mutex);
}

void memory_pool_stats(size_t *total, size_t *used, size_t *peak) {
    xSemaphoreTake(g_memory_pool.mutex, portMAX_DELAY);
    if (total) *total = g_memory_pool.total_size;
    if (used) *used = g_memory_pool.used_size;
    if (peak) *peak = g_memory_pool.peak_use;
    xSemaphoreGive(g_memory_pool.mutex);
} 