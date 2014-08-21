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
#include <string.h>
#include "squeue.h"

#define likely(e)   __builtin_expect((e), 1)
#define unlikely(e) __builtin_expect((e), 0)
#define rmb() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define wmb() __atomic_thread_fence(__ATOMIC_RELEASE)
#define mb()  __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define atomic_load(n) __atomic_load_n(&(n), __ATOMIC_RELAXED)

#define CACHE_LINE_SIZE 64
#define PAGE_SIZE 4096

typedef uint64_t __attribute__ ((aligned((CACHE_LINE_SIZE)))) aligned_size_t;
struct squeue{
    size_t nmemb;
    size_t size;

    volatile aligned_size_t end;
    volatile aligned_size_t writer_cursor;
    volatile aligned_size_t reader_cursor;

    char buffer[0];
} __attribute__ ((aligned((PAGE_SIZE))));

static inline unsigned int min_power_of_2(unsigned int in)
{
    unsigned int v = in, r = 1;
    while (v >>= 1)
        r <<= 1;
    return (r != in) ? (r << 1) : r;
}

struct squeue *squeue_create(size_t nmemb, size_t size)
{
    nmemb = min_power_of_2(nmemb);
    struct squeue *queue = NULL;
    if (posix_memalign((void **)&queue, PAGE_SIZE, sizeof(struct squeue) + nmemb * size))
        return NULL;
    memset(queue, 0, sizeof(struct squeue) + nmemb * size);
    queue->nmemb = nmemb - 1;
    queue->size  = size;

    mb();
    return queue;
}

void squeue_destroy(struct squeue *queue)
{
    free(queue);
}

void *squeue_result_next(struct reader_result *res)
{
    struct squeue *queue = res->queue;
    uint64_t cursor = res->cur;
    if (cursor == res->end)
        return NULL;

    res->cur++;
    return &queue->buffer[(queue->nmemb & cursor) * queue->size];
}

size_t squeue_reader_parpare(struct squeue *queue, struct reader_result *res)
{
    uint64_t beg, end;
    rmb();

    beg = atomic_load(queue->reader_cursor), end = atomic_load(queue->writer_cursor);
    if (end == beg)
        return 0;

    res->beg  = beg;
    res->cur  = beg;
    res->end  = end;
    res->queue = queue;
    return end - beg;
}

void squeue_reader_commit(struct squeue *queue, struct reader_result *res)
{
    assert(res->queue == queue);
    assert(queue->reader_cursor == res->beg);
    queue->reader_cursor = res->cur;
    wmb();
}

void *squeue_writer_parpare(struct squeue *queue)
{
    uint64_t wc, rc;
    rmb();

    wc = atomic_load(queue->writer_cursor), rc = atomic_load(queue->reader_cursor);
    if (unlikely(wc - rc >= queue->nmemb))
        return NULL;
    return &queue->buffer[(queue->nmemb & wc) * queue->size];
}

void squeue_writer_commit(struct squeue *queue, void *ptr)
{
    assert(&queue->buffer[(queue->nmemb & queue->writer_cursor) * queue->size] == ptr);
    queue->writer_cursor++;
    wmb();
}
