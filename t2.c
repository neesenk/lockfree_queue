/**
 * Copyright (c) 2014, Zhiyong Liu <Neesenk at gmail dot com>
 * All rights reserved.
 */
#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include "mqueue.h"

static size_t MAXSEQ = 10000000;
#define N 3 // 写线程数
#define M 1 // 读线程数
#define Q N

typedef struct { uint64_t a, b,c[61]; } entry_t;
volatile int n = 1, m = 1;

void *read_write(void)
{
    struct mqueue *buffer = mqueue_create(655360, 60);
    entry_t *entry;

    size_t total = 0;
    int s = 0;
    struct timeval t[2];
    gettimeofday(&t[0], NULL);
    size_t seq = 0;
    do {
        size_t n = 0, i;
        struct reader_result res;
        if ((n = mqueue_reader_parpare(buffer, &res)) > 0) {
            for (i = 0; i < n; i++) {
                entry = (entry_t *)mqueue_reader_next(&res);
                assert(entry != NULL);
                if (entry->a == UINT64_MAX)
                    goto out;
                assert(entry->a + 1 == entry->b);
                total++;
            }

            mqueue_reader_commit(buffer, &res);
        }

        for (;;) {
            if ((entry = (entry_t *)mqueue_writer_parpare(buffer)) == NULL)
                break;
            if (seq >= MAXSEQ)
                seq = UINT64_MAX;
            entry->a = seq;
            entry->b = seq + 1;
            seq++;
            mqueue_writer_commit(buffer, (void *)entry);
            if (seq >= MAXSEQ)
                break;
        }
    } while (1);
  out:
    gettimeofday(&t[1], NULL);
    s = (double)t[1].tv_sec * 1000000 + t[1].tv_usec - (double)t[0].tv_sec * 1000000 - t[0].tv_usec;
    printf("read and write time = %d, total = %d, %lfM/s\n", s, (int)total, total * 2.0 /s);
    mqueue_destroy(buffer);
    return NULL;
}

void *read_thread(void *arg)
{
    struct mqueue **buffer = (struct mqueue **) arg;
    const entry_t *entry;

    size_t total = 0;
    struct timeval t[2];
    gettimeofday(&t[0], NULL);
    int flag[Q] = { 0 };
    do {
        size_t n = 0, i, j;
        struct reader_result res;
        int qn = 0;
        int en = 0;
        for (j = 0; j < Q; j++) {
            if (flag[j]) {
                qn++;
                continue;
            }
            if ((n = mqueue_reader_parpare(buffer[j], &res)) == 0) {
                en++;
                continue;
            }

            for (i = 0; i < n; i++) {
                entry = (entry_t *)mqueue_reader_next(&res);
                assert(entry != NULL);
                if (entry->a == UINT64_MAX) {
                    flag[j] = 1;
                    break;
                }
                assert(entry->a + 1 == entry->b);
                total++;
            }
            mqueue_reader_commit(buffer[j], &res);
        }

        if (qn == Q)
            goto out;
        if (en == Q)
            usleep(10);
    } while (1);
  out:
    gettimeofday(&t[1], NULL);
    printf("read time = %lf, total = %d\n",
           (double)t[1].tv_sec * 1000000 + t[1].tv_usec - (double)t[0].tv_sec * 1000000 -
           t[0].tv_usec, (int)total);
    return NULL;
}

void *write_thread(void *ptr)
{
    struct mqueue *ring = (struct mqueue *) ptr;
    entry_t *entry;
    uint64_t seq = 0;
    struct timeval t[2];
    gettimeofday(&t[0], NULL);

    for (;;) {
        if ((entry = (entry_t *)mqueue_writer_parpare(ring)) == NULL) {
            usleep(1);
            continue;
        }

        entry->a = seq;
        entry->b = seq + 1;
        seq++;

        mqueue_writer_commit(ring, (void *)entry);
        if (seq >= MAXSEQ)
            break;
    }

    gettimeofday(&t[1], NULL);
    printf("write time = %lf\n",
           (double)t[1].tv_sec * 1000000 + t[1].tv_usec - (double)t[0].tv_sec * 1000000 -
           t[0].tv_usec);
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t pub[N], sub[M];
    double u = 0;

    int i,j = 0;
    entry_t *entry;
    struct timeval t[2];
    struct mqueue *r[Q] = { NULL };

    for (i = 0; i < Q; i++)
        r[i] = mqueue_create(655360, sizeof(*entry));


    if (argc > 1)
        MAXSEQ = atoll(argv[1]);

    // read_write();

    gettimeofday(&t[0], NULL);
    for (i = 0; i < M; i++)
        pthread_create(&sub[i], NULL, read_thread, r);

    for (i = 0; i < N; i++)
        pthread_create(&pub[i], NULL, write_thread, r[0]);

    for (i = 0; i < N; i++)
        pthread_join(pub[i], NULL);

    for (i = 0; i < 100; i++) {
        for (j = 0; j < Q; j++) {
            while ((entry = (entry_t *)mqueue_writer_parpare(r[j])) == NULL) {
            }
            entry->a = UINT64_MAX;
            entry->b = 0;
            mqueue_writer_commit(r[j], entry);
        }
        usleep(10);
    }

    for (i = 0; i < M; i++)
        pthread_join(sub[i], NULL);
    gettimeofday(&t[1], NULL);

    u = (double)t[1].tv_sec + (double)t[1].tv_usec / 1000000 - (double)t[0].tv_sec -
        (double)t[0].tv_usec / 1000000 - 1000.0/1000000;

    printf("time = %lfs, %lf/s\n", u, MAXSEQ * N / u);

    __sync_synchronize();
    for (j = 0; j < Q; j++)
        mqueue_destroy(r[j]);

    return 0;
}
