#include <stddef.h>             // for size_t
#include <stdlib.h>             // for malloc, free
#include "mozalloc.h"
#ifdef MOZ_MEMORY_ANDROID
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "wrap", args)
#endif
#include <malloc.h>
     
#ifdef __malloc_hook
static void* moz_malloc_hook(size_t size, const void *caller)
{
  return moz_malloc(size);
}

static void* moz_realloc_hook(void *ptr, size_t size, const void *caller)
{
  return moz_realloc(ptr, size);
}

static void moz_free_hook(void *ptr, const void *caller)
{
  moz_free(ptr);
}

static void* moz_memalign_hook(size_t align, size_t size, const void *caller)
{
  return moz_memalign(align, size);
}

static void
moz_malloc_init_hook (void)
{
  __malloc_hook = moz_malloc_hook;
  __realloc_hook = moz_realloc_hook;
  __free_hook = moz_free_hook;
  __memalign_hook = moz_memalign_hook;
}

/* Override initializing hook from the C library. */
void (*__malloc_initialize_hook) (void) = moz_malloc_init_hook;
#endif

inline void  __wrap_free(void* ptr)
{
  moz_free(ptr);
}

inline void* __wrap_malloc(size_t size)
{
  return moz_malloc(size);
}

inline void* __wrap_realloc(void* ptr, size_t size)
{
  return moz_realloc(ptr, size);
}

inline void* __wrap_calloc(size_t num, size_t size)
{
  return moz_calloc(num, size);
}

inline void* __wrap_valloc(size_t size)
{
  return moz_valloc(size);
}

inline void* __wrap_memalign(size_t align, size_t size)
{
  return moz_memalign(align, size);
}

inline char* __wrap_strdup(char* str)
{
  return moz_strdup(str);
}

inline char* __wrap_strndup(const char* str, size_t size) {
  return moz_strndup(str, size);
}


inline void  __wrap_PR_Free(void* ptr)
{
  moz_free(ptr);
}

inline void* __wrap_PR_Malloc(size_t size)
{
  return moz_malloc(size);
}

inline void* __wrap_PR_Realloc(void* ptr, size_t size)
{
  return moz_realloc(ptr, size);
}

inline void* __wrap_PR_Calloc(size_t num, size_t size)
{
  return moz_calloc(num, size);
}
