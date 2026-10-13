#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Fixed-size circular byte buffer. No dynamic allocation: the caller supplies
 * the storage. One slot is kept empty to tell "full" from "empty", so a
 * buffer built on N bytes of storage holds at most N-1 bytes.
 *
 * Single-producer / single-consumer safe: only the producer writes `head`,
 * only the consumer writes `tail` (e.g. UART ISR pushes, main loop pops).
 * On a real MCU, add the appropriate memory barriers for your core.
 */
typedef struct {
    uint8_t *storage;
    size_t size;
    volatile size_t head; /* next write index (producer) */
    volatile size_t tail; /* next read index (consumer)  */
} ring_buffer_t;

bool   rb_init(ring_buffer_t *rb, uint8_t *storage, size_t size);
bool   rb_push(ring_buffer_t *rb, uint8_t byte);   /* false if full  */
bool   rb_pop(ring_buffer_t *rb, uint8_t *byte);   /* false if empty */
size_t rb_count(const ring_buffer_t *rb);
size_t rb_capacity(const ring_buffer_t *rb);       /* usable bytes   */
bool   rb_is_empty(const ring_buffer_t *rb);
bool   rb_is_full(const ring_buffer_t *rb);
void   rb_clear(ring_buffer_t *rb);

#endif
