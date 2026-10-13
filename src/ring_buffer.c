#include "ring_buffer.h"

bool rb_init(ring_buffer_t *rb, uint8_t *storage, size_t size)
{
    if (rb == NULL || storage == NULL || size < 2u) {
        return false;
    }
    rb->storage = storage;
    rb->size = size;
    rb->head = 0u;
    rb->tail = 0u;
    return true;
}

bool rb_push(ring_buffer_t *rb, uint8_t byte)
{
    size_t next = (rb->head + 1u) % rb->size;
    if (next == rb->tail) {
        return false; /* full: reject, never overwrite unread data */
    }
    rb->storage[rb->head] = byte;
    rb->head = next;
    return true;
}

bool rb_pop(ring_buffer_t *rb, uint8_t *byte)
{
    if (rb->head == rb->tail) {
        return false;
    }
    *byte = rb->storage[rb->tail];
    rb->tail = (rb->tail + 1u) % rb->size;
    return true;
}

size_t rb_count(const ring_buffer_t *rb)
{
    return (rb->head + rb->size - rb->tail) % rb->size;
}

size_t rb_capacity(const ring_buffer_t *rb)
{
    return rb->size - 1u;
}

bool rb_is_empty(const ring_buffer_t *rb)
{
    return rb->head == rb->tail;
}

bool rb_is_full(const ring_buffer_t *rb)
{
    return ((rb->head + 1u) % rb->size) == rb->tail;
}

void rb_clear(ring_buffer_t *rb)
{
    rb->head = 0u;
    rb->tail = 0u;
}
