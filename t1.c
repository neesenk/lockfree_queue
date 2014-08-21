// test --
#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include "mqueue.h"

static size_t MAXSEQ = 10000000;
#define N 3 // 写线程数
#define M 1 // 读线程数
#define Q N

typedef struct { uint64_t a, b,c[61]; } entry_t;
volatile int n = 1, m = 1;

void *read_thread(void *arg)
{
    struct mqueuebatch *buffer = (struct mqueuebatch *) arg;
    struct mqueuebatch_reader r[1];
    const entry_t *entry;

    size_t total = 0;
    struct timeval t[2];
    gettimeofday(&t[0], NULL);

    mqueuebatch_reader_init(r, buffer);
    do {
        size_t n = 0, i;
        struct reader_result res;
        if ((n = mqueuebatch_reader_parpare(r, &res)) == 0) {
            usleep(10);
            continue;
        }

        for (i = 0; i < n; i++) {
            entry = (entry_t *)mqueuebatch_reader_next(&res);
            assert(entry != NULL);
            if (entry->a == UINT64_MAX)
                goto out;
            assert(entry->a + 1 == entry->b);
            total++;
        }

        mqueuebatch_reader_commit(r, &res);
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
    struct mqueuebatch *ring = (struct mqueuebatch *) ptr;
    struct mqueuebatch_writer w[1];
    entry_t *entry;
    uint64_t seq = 0;
    struct timeval t[2];
    gettimeofday(&t[0], NULL);

    mqueuebatch_writer_init(w, ring);

    for (;;) {
        if ((entry = (entry_t *)mqueuebatch_writer_parpare(w)) == NULL) {
            usleep(1);
            continue;
        }

        memset(entry, 0, sizeof(*entry));
        entry->a = seq;
        entry->b = seq + 1;
        seq++;

        mqueuebatch_writer_commit(w, (void *)entry);
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
    struct mqueuebatch *r = mqueuebatch_create(655360, sizeof(*entry));
    struct mqueuebatch_writer w[1];

    if (argc > 1)
        MAXSEQ = atoll(argv[1]);

    // read_write();

    gettimeofday(&t[0], NULL);
    for (i = 0; i < M; i++)
        pthread_create(&sub[i], NULL, read_thread, r);

    for (i = 0; i < N; i++)
        pthread_create(&pub[i], NULL, write_thread, r);

    for (i = 0; i < N; i++)
        pthread_join(pub[i], NULL);

    mqueuebatch_writer_init(w, r);
    usleep(10000);
    for (i = 0; i < 100; i++) {
        for (j = 0; j < Q; j++) {
            while ((entry = (entry_t *)mqueuebatch_writer_parpare(w)) == NULL) {
            }
            entry->a = UINT64_MAX;
            entry->b = 0;
            mqueuebatch_writer_commit(w, entry);
        }
        usleep(10);
    }

    for (i = 0; i < M; i++)
        pthread_join(sub[i], NULL);
    gettimeofday(&t[1], NULL);

    u = (double)t[1].tv_sec + (double)t[1].tv_usec / 1000000 - (double)t[0].tv_sec -
        (double)t[0].tv_usec / 1000000 - 200000.0/1000000;

    printf("time = %lfs, %lf/s\n", u, MAXSEQ * N / u);

    __sync_synchronize();
    mqueuebatch_destroy(r);

    return 0;
}
