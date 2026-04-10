#ifndef RAL_H
#define RAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory Allocation */
void *ral_malloc(size_t size);
void *ral_calloc(size_t nmemb, size_t size);
void ral_free(void *ptr);

static inline void *ral_memdup(const void *src, size_t len) {
    void *p = ral_malloc(len);
    if (p) memcpy(p, src, len);
    return p;
}


/* Synchronization */
typedef struct ral_mutex ral_mutex_t;

ral_mutex_t *ral_mutex_create(void);
void ral_mutex_lock(ral_mutex_t *mutex);
void ral_mutex_unlock(ral_mutex_t *mutex);
void ral_mutex_destroy(ral_mutex_t *mutex);

/* Logging */
#define RAL_DEBUG(fmt, ...) ral_log_debug(fmt, ##__VA_ARGS__)
#define RAL_ERR(fmt, ...) ral_log_error(fmt, ##__VA_ARGS__)

void ral_log_debug(const char *fmt, ...);
void ral_log_error(const char *fmt, ...);

/* Timing */
void ral_msleep(uint32_t ms);

/* Endianness & Misc */
static inline uint16_t ral_cpu_to_be16(uint16_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap16(x);
#else
    return x;
#endif
}

static inline uint32_t ral_cpu_to_be32(uint32_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap32(x);
#else
    return x;
#endif
}

static inline uint64_t ral_cpu_to_be64(uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap64(x);
#else
    return x;
#endif
}

static inline uint16_t ral_be16_to_cpu(uint16_t x) { return ral_cpu_to_be16(x); }
static inline uint32_t ral_be32_to_cpu(uint32_t x) { return ral_cpu_to_be32(x); }
static inline uint64_t ral_be64_to_cpu(uint64_t x) { return ral_cpu_to_be64(x); }

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#ifndef min_t
#define min_t(type, x, y) ({            \
    type __min1 = (x);                  \
    type __min2 = (y);                  \
    __min1 < __min2 ? __min1 : __min2; })
#endif

/* Linux kernel specific defines that are sprinkled in sed-opal.c */
#define __user
#define PTR_ERR(x) ((long)(x))
#define ERR_PTR(err) ((void *)(intptr_t)(err))
#define IS_ERR(x) ((unsigned long)(x) >= (unsigned long)-MAX_ERRNO)
#define MAX_ERRNO 4095
#define __printf(a, b) __attribute__((format(printf, a, b)))

/* Error codes from linux/errno.h */
#define EPERM        1
#define ENOENT       2
#define ESRCH        3
#define EINTR        4
#define EIO          5
#define ENXIO        6
#define E2BIG        7
#define ENOEXEC      8
#define EBADF        9
#define ECHILD      10
#define EAGAIN      11
#define ENOMEM      12
#define EACCES      13
#define EFAULT      14
#define ENOTBLK     15
#define EBUSY       16
#define EEXIST      17
#define EXDEV       18
#define ENODEV      19
#define ENOTDIR     20
#define EISDIR      21
#define EINVAL      22
#define ENFILE      23
#define EMFILE      24
#define ENOTTY      25
#define ETXTBSY     26
#define EFBIG       27
#define ENOSPC      28
#define ESPIPE      29
#define EROFS       30
#define EMLINK      31
#define EPIPE       32
#define EDOM        33
#define ERANGE      34
#define EOPNOTSUPP  95

#ifdef __cplusplus
}
#endif

#endif /* RAL_H */

#define EOVERFLOW 75

#define min(a,b) ((a) < (b) ? (a) : (b))
