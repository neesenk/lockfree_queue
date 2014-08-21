/**
 * Copyright (c) 2014, Zhiyong Liu <Neesenk at gmail dot com>
 * All rights reserved.
 */
#ifndef __SQUEUE_H__
#define __SQUEUE_H__
#include <stdint.h>

/**
 * 无锁squeue， 只支持一个消费者和生产者
 *
 * example:
 *
 * struct item { uint64_t a; uint64_t b; };
 * create:
 * struct squeue *queue = squeue_create(65536, sizeof(struct item));
 *
 * reader:
 * struct reader_result res;
 * size_t n = squeue_reader_parpare(queue, &res);
 * size_t i = 0;
 * for (i = 0; i < n; i++) {
 *     struct item *e = (struct item *)squeue_result_next(&res);
 *     assert(e != NULL);
 *     // <code>
 * }
 * if (n > 0)
 *     squeue_reader_commit(queue, &res);
 *
 * writer:
 * struct item *e = squeue_writer_parpare(queue);
 * if (e) {
 *     // <code>
 *     squeue_writer_commit(queue, e);
 * }
 *
 * destroy:
 * squeue_destroy(queue);
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

struct squeue; // squeue 对象

// 读者保存的结果的上下文
struct reader_result {
    uint64_t cur;
    uint64_t end;
    uint64_t beg;

    struct squeue *queue;
};

/**
 * squeue_create - 创建一个squeue
 * @nmemb squeue最大能够保存的对象的个数
 * @size  squeue保存的对象占用的内存大小
 * @return 返回squeue， 如果内存不足返回NULL
 */
struct squeue *squeue_create(size_t nmemb, size_t size);

/**
 * squeue_destroy - 释放squeue占用的内存
 * @queue squeue对象
 */
void   squeue_destroy(struct squeue *queue);

// 获取下一个结果
void  *squeue_result_next(struct reader_result *res);
size_t squeue_reader_parpare(struct squeue *queue, struct reader_result *res);
void   squeue_reader_commit(struct squeue *queue, struct reader_result *res);
void  *squeue_writer_parpare(struct squeue *queue);
void   squeue_writer_commit(struct squeue *queue, void *ptr);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif /* __SQUEUE_H__ */
