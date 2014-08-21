#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "squeue.h"

typedef struct { uint32_t a, b, c[6]; } entry_t;
int sleep1 = 0;
int sleep2 = 0;

#define MAXVAL UINT32_MAX
static size_t MAXSEQ = 40000000;
void *read_thread(void *arg)
{
    struct squeue *buffer = (struct squeue *) arg;
    const entry_t *entry;

    size_t total = 0;
    struct timeval t[2];
    gettimeofday(&t[0], NULL);
    uint64_t p = 0;
    do {
        size_t n = 0, i = 0;
        struct reader_result res;
        if ((n = squeue_reader_parpare(buffer, &res)) == 0) {
            sleep1++;
            usleep(10);
            continue;
        }

        for (i = 0; i < n; i++) {
            entry = squeue_result_next(&res);
            assert(entry != NULL);
            if (entry->a == MAXVAL)
                goto out;
            assert(entry->a + 1 == entry->b);
            assert((p == 0 && entry->a == 0) || p + 1 == entry->a);
            p = entry->a;
            total++;
        }
        squeue_reader_commit(buffer, &res);
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
    struct squeue *ring = (struct squeue *) ptr;
    entry_t *entry;
    uint64_t seq = 0;
    struct timeval t[2];
    gettimeofday(&t[0], NULL);
    for (;;) {
        if ((entry = (entry_t *)squeue_writer_parpare(ring)) == NULL) {
            sleep2++;
            usleep(1);
            continue;
        }

        memset(entry, 0, sizeof(*entry));
        entry->a = seq;
        entry->b = seq + 1;
        seq++;

        squeue_writer_commit(ring, entry);
        if (seq < MAXSEQ)
            continue;

        while ((entry = (entry_t *)squeue_writer_parpare(ring)) == NULL) {
            sleep2++;
            usleep(1);
        }

        entry->a = MAXVAL;
        entry->b = 0;
        squeue_writer_commit(ring, entry);
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

    struct squeue *r = squeue_create(65536, sizeof(entry_t));
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
    squeue_destroy(r);

    return 0;
}
