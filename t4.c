/**
 * Copyright (c) 2014, Zhiyong Liu <Neesenk at gmail dot com>
 * All rights reserved.
 */
#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "ringbuffer.h"

typedef struct { uint32_t a, b, c[62]; } entry_t;
int sleep1 = 0;
int sleep2 = 0;

#define MAXVAL UINT32_MAX
static size_t MAXSEQ = 40000000;
void *read_thread(void *arg)
{
    struct ringbuffer *buffer = (struct ringbuffer *) arg;
    const entry_t *entry;

    size_t total = 0;
    struct timeval t[2];
    gettimeofday(&t[0], NULL);
    uint64_t p = 0;
    do {
        size_t n = 0;
        struct reader_result res;
        if (ringbuffer_reader_parpare(buffer, &res) == 0) {
            sleep1++;
            usleep(10);
            continue;
        }

        while ((entry = (const entry_t *)ringbuffer_result_next(&res, &n)) != NULL) {
            assert(n >= sizeof(entry_t));
            if (entry->a == MAXVAL)
                goto out;

            assert(entry->a + 1 == entry->b);
            assert((p == 0 && entry->a == 0) || p + 1 == entry->a);
            p = entry->a;
            total++;
        }

        ringbuffer_reader_commit(buffer, &res);
    } while (1);
  out:
    gettimeofday(&t[1], NULL);
    printf("read time = %lf, total = %d, sleep %d\n",
           (double)t[1].tv_sec * 1000000 + t[1].tv_usec - (double)t[0].tv_sec * 1000000 -
           t[0].tv_usec, (int)total, sleep1);
    return NULL;
}

// #define MAXSEQ 2500
void *write_thread(void *ptr)
{
    struct ringbuffer *ring = (struct ringbuffer *) ptr;
    entry_t *entry;
    uint64_t seq = 0;
    struct timeval t[2];
    int len[100] = {0};
    int i = 0;
    int to = 0;
    for (i = 0; i < 100; i++) {
        len[i] = (random() % 1024 + 63) / 64 * 64;
        to += len[i];
    }

    printf("avg %d\n", to / 100);
    gettimeofday(&t[0], NULL);
    for (;;) {
        if ((entry = (entry_t *)ringbuffer_writer_parpare(ring, sizeof(entry_t) + len[seq % 100])) == NULL) {
            sleep2++;
            usleep(1);
            continue;
        }

        memset(entry, 0, sizeof(*entry));
        entry->a = seq;
        entry->b = seq + 1;
        seq++;
        ringbuffer_writer_commit(ring, entry);
        if (seq < MAXSEQ)
            continue;

        while ((entry = (entry_t *)ringbuffer_writer_parpare(ring, sizeof(entry_t))) == NULL) {
            sleep2++;
            usleep(1);
        }
        entry->a = MAXVAL;
        entry->b = 0;
        ringbuffer_writer_commit(ring, entry);
        break;
    }

    gettimeofday(&t[1], NULL);
    printf("write time = %lf,sleep %d, total %d\n",
           (double)t[1].tv_sec * 1000000 + t[1].tv_usec - (double)t[0].tv_sec * 1000000 -
           t[0].tv_usec, sleep2, (int)seq);
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t pub, sub;
    double u = 0;

    struct ringbuffer *r = ringbuffer_create(6553600, sizeof(entry_t) + 2000);
    struct timeval t[2];

    if (argc > 1)
        MAXSEQ = atoll(argv[1]);

    gettimeofday(&t[0], NULL);
    pthread_create(&pub, NULL, write_thread, r);
    pthread_create(&sub, NULL, read_thread, r);

    pthread_join(pub, NULL);
    printf("join pub\n");

    pthread_join(sub, NULL);
    printf("join sub\n");

    gettimeofday(&t[1], NULL);

    u = (double)t[1].tv_sec + (double)t[1].tv_usec / 1000000 - (double)t[0].tv_sec -
        (double)t[0].tv_usec / 1000000;

    printf("time = %lfs, %lf/s\n", u, MAXSEQ / u);
    ringbuffer_destroy(r);

    return 0;
}
