#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t *buffer;      // 缓冲区指针
    uint32_t head;        // 写指针
    uint32_t tail;        // 读指针
    uint32_t size;        // 缓冲区大小
} ring_buffer_t;

// 函数声明
void ring_buffer_init(ring_buffer_t *rb, uint8_t *buf, uint32_t size);
void ring_buffer_clear(ring_buffer_t *rb);
bool ring_buffer_is_empty(ring_buffer_t *rb);
uint32_t ring_buffer_available(ring_buffer_t *rb);  // 获取可读数据量


// 读写函数
uint32_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, uint32_t len);
uint32_t ring_buffer_read(ring_buffer_t *rb, uint8_t *out_buf, uint32_t max_len);

#endif