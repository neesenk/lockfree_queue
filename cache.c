#include <assert.h>
#include <stdlib.h>

struct circlecache
{
    size_t max;
    size_t size;
    size_t read_cursor;       // 缓存的首地址偏移
    size_t writer_cursor;     // 缓存的尾地址偏移

    char buffer[];
};

struct item
{
    unsigned size;
    char contents[];
};

static inline unsigned int min_power_of_2(unsigned int in)
{
    unsigned int v = in, r = 1;
    while (v >>= 1)
        r <<= 1;
    return (r != in) ? (r << 1) : r;
}

struct circlecache *circlecache_create(size_t size, size_t max)
{
    struct circlecache *ret = NULL;

    size = min_power_of_2(size);
    if ((ret = calloc(1, sizeof(*ret) + max + size + sizeof(struct item))) == NULL)
        return NULL;

    ret->size = size;
    ret->max  = max;

    return ret;
}

void circlecache_destroy(struct circlecache *cache)
{
    if (cache)
        free(cache);
}

void *circlecache_alloc(struct circlecache *cache, size_t size)
{
    struct item *item = NULL;
    size_t left = (cache->read_cursor + size - cache->writer_cursor - 1) & (~cache->size);

    if (size > cache->max)
        return NULL;
    if (size + cache->max + sizeof(struct item) > left)
        return NULL;

    item = (struct item *)(cache->buffer + cache->writer_cursor);
    cache->writer_cursor = (cache->writer_cursor + size + sizeof(struct item)) & (~cache->size); //移动尾部指针

    item->size = size;
    return item->contents;
}

void *circlecache_read(struct circlecache *cache, size_t *size)
{
    struct item *item = (struct item *)(cache->buffer + cache->read_cursor);
    if (cache->read_cursor == cache->writer_cursor)
        return 0;
    assert(item->size <= cache->max);
    cache->read_cursor = (cache->read_cursor + item->size + sizeof(struct item)) & (~cache->size); // 移动头部指针
    *size = item->size;

    return item->contents;
}

void circlecache_clear(struct circlecache *cache)
{
    cache->read_cursor = 0;
    cache->writer_cursor = 0;
}
