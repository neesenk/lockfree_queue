/**
 * Copyright (c) 2014, Zhiyong Liu <Neesenk at gmail dot com>
 * All rights reserved.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include "ringbuffer.h"

#define container_of(ptr, type, member) (type *)((char *)(ptr) - offsetof(type,member))

#define likely(e)   __builtin_expect((e), 1)
#define unlikely(e) __builtin_expect((e), 0)
#define rmb() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define wmb() __atomic_thread_fence(__ATOMIC_RELEASE)
#define mb()  __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define atomic_load(n) __atomic_load_n(&(n), __ATOMIC_RELAXED)

#define CACHE_LINE_SIZE 64
#define PAGE_SIZE 4096

typedef uint64_t __attribute__ ((aligned((CACHE_LINE_SIZE)))) aligned_size_t;

struct item {
    volatile  uint32_t size;
    char contents[];
};

struct ringbuffer {
    size_t max;
    size_t size;

    volatile aligned_size_t end;
    volatile aligned_size_t writer_cursor;
    volatile aligned_size_t reader_cursor;

    char buffer[0];
};

static inline unsigned int min_power_of_2(unsigned int in)
{
    unsigned int v = in, r = 1;
    while (v >>= 1)
        r <<= 1;
    return (r != in) ? (r << 1) : r;
}

struct ringbuffer *ringbuffer_create(size_t size, size_t max)
{
    size = min_power_of_2(size);
    struct ringbuffer *ring = NULL;

    if ((ring = calloc(1, sizeof(*ring) + size + max + sizeof(struct item))) == NULL)
        return NULL;

    ring->max   = max;
    ring->size  = size;

    mb();
    return ring;
}

void ringbuffer_destroy(struct ringbuffer *ring)
{
    free(ring);
}

void *ringbuffer_result_next(struct reader_result *res, size_t *size)
{
    struct item *item = NULL;
    struct ringbuffer *ring = res->ring;
    if (res->cur == res->end)
        return NULL;

    item = (struct item *)(ring->buffer + res->cur);
    assert(item->size <= ring->max);

    if (likely(size != NULL))
        *size = item->size;

    res->cur = (res->cur + item->size + sizeof(struct item)) & (~ring->size);
    return item->contents;
}

int ringbuffer_reader_parpare(struct ringbuffer *ring, struct reader_result *res)
{
    uint64_t beg, end;
    rmb();
    beg = ring->reader_cursor, end = ring->writer_cursor;
    if (end == beg)
        return 0;

    res->beg  = beg;
    res->end  = end;
    res->cur  = beg;
    res->ring = ring;

    return 1;
}

void ringbuffer_reader_commit(struct ringbuffer *ring, struct reader_result *res)
{
    assert(res->ring == ring);
    assert(ring->reader_cursor == res->beg);

    ring->reader_cursor = res->cur;
    wmb();
}

void *ringbuffer_writer_parpare(struct ringbuffer *ring, size_t size)
{
    struct item *item = NULL;
    uint64_t incur, seq, left;

    if (unlikely(size > ring->max))
        return NULL;

    rmb();
    incur = ring->writer_cursor, seq = ring->reader_cursor;
    left  = (seq + ring->size - incur - 1) & (~ring->size);
    if (unlikely(size + ring->max + sizeof(struct item) > left))
        return NULL;

    item = (struct item *)(ring->buffer + incur);
    item->size = size;

    return item->contents;
}

void ringbuffer_writer_commit(struct ringbuffer *ring, void *ptr)
{
    struct item *item = container_of(ptr, struct item, contents);
    uint32_t cursor = (char *)item - ring->buffer;
    assert(ring->writer_cursor == cursor);

    ring->writer_cursor = (cursor + item->size + sizeof(struct item)) & (~ring->size);

    wmb();
}
