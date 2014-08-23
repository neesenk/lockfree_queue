#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__
/**
 * Copyright (c) 2014, Zhiyong Liu <Neesenk at gmail dot com>
 * All rights reserved.
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

struct ringbuffer;
struct reader_result
{
    size_t beg;
    size_t end;
    size_t cur;

    struct ringbuffer *ring;
};

struct ringbuffer *ringbuffer_create(size_t size, size_t max);
void ringbuffer_destroy(struct ringbuffer *ring);

void *ringbuffer_result_next(struct reader_result *res, size_t *size);
int   ringbuffer_reader_parpare(struct ringbuffer *ring, struct reader_result *res);
void  ringbuffer_reader_commit(struct ringbuffer *ring, struct reader_result *res);
void *ringbuffer_writer_parpare(struct ringbuffer *ring, size_t size);
void  ringbuffer_writer_commit(struct ringbuffer *ring, void *ptr);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __RINGBUFFER_H__
