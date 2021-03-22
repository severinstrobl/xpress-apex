#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <memory.h>

// Assume 4K pages unless we know otherwise.
// We cannot determine this at runtime because it must be known during
// the bootstrap process and it would be unsafe to make any system calls there.
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

// Size of heap memory for library wrapper bootstrapping
#ifdef __APPLE__
// Starting on macOS 11, PAGE_SIZE is not constant on macOS
// Apple recommends using PAGE_MAX_SIZE instead.
// see https://developer.apple.com/videos/play/wwdc2020/10214/?time=549
#define BOOTSTRAP_HEAP_SIZE (3*PAGE_MAX_SIZE)
#else
#define BOOTSTRAP_HEAP_SIZE (3*PAGE_SIZE)
#endif

typedef void* (*malloc_p)(size_t);
typedef void  (*free_p)(void*);
typedef int   (*puts_p)(const char*);
#if 0
typedef void* (*calloc_p)(size_t, size_t);
typedef void* (*realloc_p)(void*, size_t);
#if defined(memalign)
typedef void* (*memalign_p)(void*, size_t, size_t);
#endif
#if defined(reallocarray)
typedef void* (*reallocarray_p)(void*, size_t, size_t);
#endif
#if defined(reallocf)
typedef void* (*reallocf_p)(void*, size_t);
#endif
#if defined(valloc)
typedef void* (*valloc_p)(size_t);
#endif
#if defined(malloc_usable_size)
typedef size_t (*valloc_p)(void*);
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

void* apex_malloc_wrapper(malloc_p malloc_call, size_t size);
void  apex_free_wrapper(free_p free_call, void* ptr);
int   apex_puts_wrapper(const char* s);
void  apex_memory_wrapper_init(void);
void  apex_report_leaks(void);
#if 0
void* apex_calloc_wrapper(calloc_p calloc_call, size_t nmemb, size_t size);
#if defined(memalign)
void* apex_memalign_wrapper(memalign_p calloc_call, size_t align, size_t size);
#endif
void* apex_realloc_wrapper(realloc_p realloc_call, void* ptr, size_t size);
#if defined(reallocarray)
void* apex_reallocarray_wrapper(reallocarray_p reallocarray_call, void* ptr, size_t nmemb, size_t size);
#endif
#if defined(reallocf)
void* apex_reallocf_wrapper(reallocf_p reallocf_call, void* ptr, size_t size);
#endif
#if defined(valloc)
void* apex_valloc_wrapper(valloc_p valloc_call, size_t size);
#endif
#if defined(malloc_usable_size)
void* apex_malloc_usable_size_wrapper(malloc_usable_size_p malloc_usable_size_call, void* ptr);
#endif
#endif

#ifdef __cplusplus
}
#endif

#define APEX_PRELOAD_LIB
