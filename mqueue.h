#ifndef __MQUEUE_H__
#define __MQUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * 无锁队列实现:
 *
 * mqueue和mqueuebatch支持多个读者和多个写者并发操作的的无锁队列
 *
 * mqueue 接口和mqueuebatch接口的区别:
 *  mqueue能够保证入队和出队一致, mqueuebatch只能保证一个线程下的出队和入队是一致的
 *  mqueuebatch的性能是mqueue的性能的cpu个数倍以上
 *
 * mqueue example:
 *
 * struct data { uint32_t a; uint64_t b; };
 * struct mqueue *q = mqueue_create(655360, sizeof(struct data));
 * assert(q != NULL);
 *
 * writer:
 * struct data *data = NULL;
 * if ((data = (struct data *)mqueue_writer_parpare(q)) != NULL) {
 *      ...
 *      mqueue_writer_commit(q, data);
 * } else {
 *      ...
 * }
 *
 * reader:
 * int n = 0;
 * struct reader_result res;
 * if ((n = mqueue_reader_parpare(q, &res)) > 0) {
 *      int i = 0;
 *      for (i = 0; i < n; i++) {
 *          struct data *data = mqueue_reader_next(&res);
 *          assert(data != NULL);
 *          ...
 *      }
 *
 *      mqueue_reader_commit(q, &res);
 * } else {
 *      ...
 * }
 *
 * mqueue_destroy(q);
 *
 * queuebatch example:
 *
 * struct data { uint32_t a; uint64_t b; };
 * struct mqueuebatch *qs = mqueuebatch_create(655360, sizeof(struct data));
 * assert(qs != NULL);
 *
 * writer:
 * struct data *data = NULL;
 * struct mqueuebatch_writer w;
 * mqueuebatch_writer_init(&w, qs);
 *
 * if ((data = (struct data *)mqueuebatch_writer_parpare(&w)) != NULL) {
 *      ...
 *      mqueue_writer_commit(&w, data);
 * } else {
 *      ...
 * }
 *
 * reader:
 * int n = 0;
 * struct reader_result res;
 * struct mqueuebatch r;
 * mqueuebatch_reader_init(&r, qs);
 * if ((n = mqueuebatch_reader_parpare(&r, &res)) > 0) {
 *      int i = 0;
 *      for (i = 0; i < n; i++) {
 *          struct data *data = mqueuebatch_reader_next(&res);
 *          assert(data != NULL);
 *          ...
 *      }
 *
 *      mqueuebatch_reader_commit(&r, &res);
 * } else {
 *      ...
 * }
 *
 * mqueuebatch_destroy(qs);
 */

struct item;
struct mqueue;
struct mqueuebatch;

// 读进程获取到的结果
struct reader_result {
    size_t nmemb;       // 包含了多少条记录
    struct mqueue *q;    // 所属的queue
    struct item *header;// 头部
    struct item *tail;  // 尾部
    struct item *curr;  // 当前遍历到的节点
};

struct mqueuebatch_reader {
    int queueid;
    struct mqueuebatch *queuebatch;
};

struct mqueuebatch_writer {
    struct mqueue *queue;
    struct mqueuebatch *queuebatch;
};

struct mqueue *mqueue_create(size_t nmemb, size_t size);
void   mqueue_destroy(struct mqueue *q);
void  *mqueue_writer_parpare(struct mqueue *q);
void   mqueue_writer_commit(struct mqueue *q, void *ptr);
void  *mqueue_reader_next(struct reader_result *res);
size_t mqueue_reader_parpare(struct mqueue *q, struct reader_result *ret);
void   mqueue_reader_commit(struct mqueue *q, struct reader_result *res);

void   mqueuebatch_reader_init(struct mqueuebatch_reader *reader, struct mqueuebatch *qs);
void   mqueuebatch_writer_init(struct mqueuebatch_writer *writer, struct mqueuebatch *qs);

struct mqueuebatch *mqueuebatch_create(size_t nmemb, size_t size);
void   mqueuebatch_destroy(struct mqueuebatch *qs);
void  *mqueuebatch_writer_parpare(struct mqueuebatch_writer *writer);
void   mqueuebatch_writer_commit(struct mqueuebatch_writer *writer, void *ptr);
void  *mqueuebatch_reader_next(struct reader_result *res);
size_t mqueuebatch_reader_parpare(struct mqueuebatch_reader *reader, struct reader_result *ret);
void   mqueuebatch_reader_commit(struct mqueuebatch_reader *reader, struct reader_result *res);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __MQUEUE_H__
