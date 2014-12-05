/**
 * Copyright (c) 2014, Zhiyong Liu <Neesenk at gmail dot com>
 * All rights reserved.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include "mqueue.h"

#define container_of(ptr, type, member) (type *)((char *)(ptr) - offsetof(type,member))

// 平台相关函数
#define likely(e)   __builtin_expect((e), 1)
#define unlikely(e) __builtin_expect((e), 0)
#define compare_and_swap(ptr, old, new) __atomic_compare_exchange_n(ptr, &(old), new, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED)
#define wmb() __atomic_thread_fence(__ATOMIC_RELEASE)
#define rmb() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define mb()  __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define atomic_load(n) __atomic_load_n(&(n), __ATOMIC_RELAXED)
#define atomic_fetch_add(n, num) __atomic_fetch_add(&(n), (num), __ATOMIC_RELAXED)

#define cpu_relax() do { asm volatile("pause\n":::"memory"); } while (0)
#define get_cpunum() get_nprocs()

#define CACHE_LINE_SIZE 64

#define ITEM(q, i) ((struct item *)((q)->items + (i) * (q)->size))
#define ITEM_IDX(q, ptr) (((char *)(ptr) - (q)->items) / (q)->size)

#define TAIL_IDX        0xFFFFFFFF
#define UNUSED_FLAG     0xFFFFFFFE

typedef uint32_t __attribute__ ((aligned((CACHE_LINE_SIZE * 2)))) aligned_uint32_t;
typedef uint64_t __attribute__ ((aligned((CACHE_LINE_SIZE * 2)))) aligned_uint64_t;

struct item {
    volatile uint32_t next;
    char content[];
};

struct mqueue {
    size_t nmemb;   // 队列大小
    size_t size;    // 队列每一项的占内存大小

    volatile aligned_uint32_t head; // 队列的头部

    // free高32位用来表示在数组中的下标，低32位表示设置free的序号
    // 这样做的是为了防止ABA问题产生
    volatile aligned_uint64_t free; // 未使用的元素的个数

    char items[];
};

struct mqueuebatch {
    int wseq;           // writer加入的次数
    int qnum;           // queues的大小
    struct mqueue *queues[];
};

void mqueuebatch_reader_init(struct mqueuebatch_reader *reader, struct mqueuebatch *qs)
{
    reader->queueid = 0;
    reader->queuebatch = qs;
}

void mqueuebatch_writer_init(struct mqueuebatch_writer *writer, struct mqueuebatch *qs)
{
    int seq = atomic_fetch_add(qs->wseq, 1);
    writer->queue = qs->queues[seq % qs->qnum];
    writer->queuebatch = qs;
}

struct mqueue *mqueue_create(size_t nmemb, size_t size)
{
    size_t i = 0;
    struct mqueue *q = 0;

    size = ((size + sizeof(struct item) + 7) / 8) * 8; // 8字节对齐
    q = (struct mqueue*)calloc(1, sizeof(*q) + size * nmemb);

    if (q == NULL)
        return NULL;

    q->nmemb= nmemb;
    q->free = 0;
    q->head = TAIL_IDX;
    q->size = size;

    for (i = 0; i < nmemb - 1; i++)
        ITEM(q, i)->next = i + 1;

    ITEM(q, i)->next = TAIL_IDX;
    mb();

    return q;
}

void mqueue_destroy(struct mqueue *q)
{
    free(q);
}

void mqueuebatch_destroy(struct mqueuebatch *qs)
{
    size_t i = 0;
    for (i = 0; i < qs->qnum; i++) {
        if (qs->queues[i])
            mqueue_destroy(qs->queues[i]);
    }

    free(qs);
}

struct mqueuebatch *mqueuebatch_create(size_t nmemb, size_t size)
{
    size_t i = 0;
    size_t qnum = get_cpunum();
    struct mqueuebatch *qs = (struct mqueuebatch*)calloc(1, sizeof(*qs) + sizeof(struct mqueue *) * qnum);
    if (qs == NULL)
        return NULL;

    assert(qnum > 0);
    nmemb = (nmemb + qnum - 1) / qnum;
    qs->qnum = qnum;
    for (i = 0; i < qnum; i++) {
        if ((qs->queues[i] = mqueue_create(nmemb, size)) == NULL) {
            mqueuebatch_destroy(qs);
            return NULL;
        }
    }

    return qs;
}

void *mqueue_writer_parpare(struct mqueue *q)
{
    struct item *ret = NULL;
    uint64_t oldf, newf;
    uint32_t freen, seq;
    for (;;) {
        rmb();

        oldf  = atomic_load(q->free);
        freen = oldf >> 32;
        seq   = (oldf & 0xFFFFFFFF) + 1;
        if (unlikely(freen == TAIL_IDX))
            return NULL;

        ret  = ITEM(q, freen);
        newf = ((uint64_t)ret->next << 32) | seq;

        if (compare_and_swap(&q->free, oldf, newf)) {
            ret->next = UNUSED_FLAG;
            return ret->content;
        }

        cpu_relax();
    }

    return NULL;
}

void *mqueuebatch_writer_parpare(struct mqueuebatch_writer *writer)
{
    return mqueue_writer_parpare(writer->queue);
}

void mqueue_writer_commit(struct mqueue *q, void *ptr)
{
    uint32_t head = 0;
    struct item *item = container_of(ptr, struct item, content);
    uint32_t idx = ITEM_IDX(q, item);

    assert(item->next == UNUSED_FLAG && idx < q->nmemb);

    for (;;) {
        rmb();
        head = item->next = atomic_load(q->head);
        wmb();

        if (compare_and_swap(&q->head, head, idx))
            return;
        cpu_relax();
    }
}

void mqueuebatch_writer_commit(struct mqueuebatch_writer *writer, void *ptr)
{
    mqueue_writer_commit(writer->queue, ptr);
}

void *mqueue_reader_next(struct reader_result *res)
{
    if (res->curr == NULL) {
        res->curr = res->header;
        return res->curr->content;
    }

    if (res->curr->next == TAIL_IDX) {
        assert(res->curr == res->tail);
        return NULL;
    }

    assert(res->curr->next < res->q->nmemb);
    res->curr = ITEM(res->q, res->curr->next);

    return res->curr->content;
}

void *mqueuebatch_reader_next(struct reader_result *res)
{
    return mqueue_reader_next(res);
}

size_t mqueue_reader_parpare(struct mqueue *q, struct reader_result *ret)
{
    uint32_t head = 0, prev = TAIL_IDX, next = 0;
    struct item *item = NULL, *tail = NULL;
    size_t n = 1;

    for (;;) {
        rmb();
        head = atomic_load(q->head);
        if (head == TAIL_IDX)
            return 0;
        if (compare_and_swap(&q->head, head, TAIL_IDX))
            break;
        cpu_relax();
    }

    assert(head < q->nmemb);
    // 反转链表
    tail = item = ITEM(q, head);
    while (item->next != TAIL_IDX) {
        next       = item->next;
        item->next = prev;
        prev       = head;
        head       = next;
        item       = ITEM(q, head);
        n++;
    }
    item->next = prev;

    ret->nmemb = n;
    ret->q     = q;
    ret->header= item;
    ret->tail  = tail;
    ret->curr  = NULL;
    return n;
}

size_t mqueuebatch_reader_parpare(struct mqueuebatch_reader *reader, struct reader_result *ret)
{
    size_t i = 0, n = 0;
    size_t qnum = reader->queuebatch->qnum;
    for (i = 0; i < qnum; i++) {
        struct mqueue *q = reader->queuebatch->queues[(reader->queueid++) % qnum];
        n = mqueue_reader_parpare(q, ret);
        if (n > 0)
            return n;
    }

    return 0;
}

void mqueue_reader_commit(struct mqueue *q, struct reader_result *res)
{
    uint32_t freen, seq;
    uint64_t oldf, newf;
    uint32_t idx = ITEM_IDX(q, res->header);
    assert(idx < q->nmemb);
    assert(res->q == q);

    for (;;) {
        rmb();

        oldf  = atomic_load(q->free);
        freen = oldf >> 32;
        seq   = (oldf & 0xFFFFFFFF) + 1;
        newf  = ((uint64_t)idx << 32) | seq;

        res->tail->next = freen;
        wmb();

        if (compare_and_swap(&q->free, oldf, newf))
            return;
        cpu_relax();
    }
}

void mqueuebatch_reader_commit(struct mqueuebatch_reader *reader, struct reader_result *res)
{
    return mqueue_reader_commit(res->q, res);
}
