#ifndef LINUX_XCOMPAT_H
#define LINUX_XCOMPAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "ral.h"
#include "opal.h"

/* Linux Basic Types */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u_char;
typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;

#define EOVERFLOW 75
#define OPAL_AUTH_KEY "opal_auth_key"

typedef intptr_t ssize_t;
#define min(a, b) ((a) < (b) ? (a) : (b))
#define ERR_PTR(err) ((void *)(intptr_t)(err))

#define __packed __attribute__((packed))
#define __user

/* Linux System/Module macros */
#define __init
#define __exit
#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)
#define MODULE_AUTHOR(x)
#define MODULE_LICENSE(x)
#define MODULE_DESCRIPTION(x)
#define late_initcall(x)

/* Linux Memory Allocation */
#define kmalloc(sz, flags) ral_malloc(sz)
#define kzalloc(sz, flags) ral_calloc(1, sz)
#define kfree(p) ral_free((void *)(p))
#define kcalloc(n, sz, flags) ral_calloc(n, sz)
#define kzalloc_obj(type) ral_calloc(1, sizeof(type))
#define kmalloc_obj(type) ral_malloc(sizeof(type))
static inline void *kmemdup(const void *src, size_t len, int gfp) {
	void *p = ral_malloc(len);
	if (p) memcpy(p, src, len);
	return p;
}
#define GFP_KERNEL 0
#define GFP_ATOMIC 0

/* Logging */
#undef pr_fmt
#define pr_fmt(fmt) "OPAL: " fmt
#define pr_debug(fmt, ...) RAL_DEBUG(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) RAL_ERR(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn(fmt, ...) RAL_ERR(pr_fmt(fmt), ##__VA_ARGS__)
#define dev_dbg(dev, fmt, ...) RAL_DEBUG(pr_fmt(fmt), ##__VA_ARGS__)

/* Synchronization */
struct mutex { ral_mutex_t* mtx; };
static inline void mutex_lock(struct mutex *m) { ral_mutex_lock(m->mtx); }
static inline void mutex_unlock(struct mutex *m) { ral_mutex_unlock(m->mtx); }
static inline void mutex_init(struct mutex *m) { m->mtx = ral_mutex_create(); }
static inline void mutex_destroy(struct mutex *m) { ral_mutex_destroy(m->mtx); }

/* Endianness */
#define cpu_to_be16 ral_cpu_to_be16
#define cpu_to_be32 ral_cpu_to_be32
#define cpu_to_be64 ral_cpu_to_be64
#define be16_to_cpu ral_be16_to_cpu
#define be32_to_cpu ral_be32_to_cpu
#define be64_to_cpu ral_be64_to_cpu
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le64(x) (x)
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) (x)

/* Fls */
static inline int fls64(uint64_t x)
{
	int r = 64;
	if (!x) return 0;
	if (!(x & 0xffffffff00000000ull)) {
		x <<= 32;
		r -= 32;
	}
	if (!(x & 0xffff000000000000ull)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff00000000000000ull)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf000000000000000ull)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc000000000000000ull)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x8000000000000000ull)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

/* Lists */
struct list_head {
	struct list_head *next, *prev;
};

static inline void INIT_LIST_HEAD(struct list_head *list) {
	list->next = list;
	list->prev = list;
}

static inline void list_add_tail(struct list_head *new_node, struct list_head *head) {
	new_node->prev = head->prev;
	new_node->next = head;
	head->prev->next = new_node;
	head->prev = new_node;
}

static inline void list_del(struct list_head *entry) {
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
}

#define list_for_each_entry(pos, head, member) \
	for (pos = container_of((head)->next, typeof(*pos), member); \
	     &pos->member != (head); \
	     pos = container_of(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member) \
	for (pos = container_of((head)->next, typeof(*pos), member), \
		n = container_of(pos->member.next, typeof(*pos), member); \
	     &pos->member != (head); \
	     pos = n, n = container_of(n->member.next, typeof(*n), member))

/* Keyrings */
struct key;
typedef void* key_ref_t;
struct key_type {
	int (*read)(const struct key *, char __user *, size_t);
};
struct key {
	struct key_type *type;
	size_t datalen;
	void* sem;
};

extern struct key_type key_type_user;

#define KEY_USR_VIEW 0
#define KEY_USR_SEARCH 0
#define KEY_USR_WRITE 0
#define KEY_ALLOC_NOT_IN_QUOTA 0
#define KEY_ALLOC_BUILT_IN 0
#define KEY_ALLOC_BYPASS_RESTRICTION 0

static inline key_ref_t make_key_ref(const struct key *key, bool possessed) { return NULL; }
static inline key_ref_t key_create_or_update(key_ref_t keyring, const char *type, const char *description,
			       const void *payload, size_t plen, unsigned perm,
			       unsigned flags) { return NULL; }
static inline void key_ref_put(key_ref_t key_ref) {}
static inline key_ref_t keyring_search(key_ref_t keyring, struct key_type *type, const char *description, bool recycle) { return (key_ref_t)-ENOENT; }
static inline void down_read(void* sem) {}
static inline void up_read(void* sem) {}
static inline int key_validate(const struct key *key) { return -1; }
static inline void *key_ref_to_ptr(const key_ref_t key_ref) { return NULL; }

/* Misc */
#define print_hex_dump_bytes(prefix, type, ptr, len)
#define DUMP_PREFIX_OFFSET 0

/* User space copy */
static inline int copy_to_user(void *dst, const void *src, size_t size) {
	memcpy(dst, src, size);
	return 0;
}
static inline int copy_from_user(void *dst, const void *src, size_t size) {
	memcpy(dst, src, size);
	return 0;
}

#define KBUILD_MODNAME "sed-opal"

#endif /* LINUX_XCOMPAT_H */
