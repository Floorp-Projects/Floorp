/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * secport.c - portability interfaces for security libraries
 *
 * This file abstracts out libc functionality that libsec depends on
 *
 * NOTE - These are not public interfaces
 */

#include "seccomon.h"
#include "prmem.h"
#include "prerror.h"
#include "plarena.h"
#include "secerr.h"
#include "prmon.h"
#include "nssilock.h"
#include "secport.h"
#include "prenv.h"
#include "prinit.h"

#include <stdint.h>

#ifdef DEBUG
#define THREADMARK
#endif /* DEBUG */

#ifdef THREADMARK
#include "prthread.h"
#endif /* THREADMARK */

#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
#include <stdlib.h>
#else
#include "wtypes.h"
#endif

#define SET_ERROR_CODE /* place holder for code to set PR error code. */

#ifdef THREADMARK
typedef struct threadmark_mark_str {
    struct threadmark_mark_str *next;
    void *mark;
} threadmark_mark;

#endif /* THREADMARK */

/* The value of this magic must change each time PORTArenaPool changes. */
#define ARENAPOOL_MAGIC 0xB8AC9BDF

#define CHEAP_ARENAPOOL_MAGIC 0x3F16BB09

typedef struct PORTArenaPool_str {
    PLArenaPool arena;
    PRUint32 magic;
    PRLock *lock;
#ifdef THREADMARK
    PRThread *marking_thread;
    threadmark_mark *first_mark;
#endif
} PORTArenaPool;

/* locations for registering Unicode conversion functions.
 * XXX is this the appropriate location?  or should they be
 *     moved to client/server specific locations?
 */
PORTCharConversionFunc ucs4Utf8ConvertFunc;
PORTCharConversionFunc ucs2Utf8ConvertFunc;
PORTCharConversionWSwapFunc ucs2AsciiConvertFunc;

/* NSPR memory allocation functions (PR_Malloc, PR_Calloc, and PR_Realloc)
 * use the PRUint32 type for the size parameter. Before we pass a size_t or
 * unsigned long size to these functions, we need to ensure it is <= half of
 * the maximum PRUint32 value to avoid truncation and catch a negative size.
 */
#define MAX_SIZE (PR_UINT32_MAX >> 1)

void *
PORT_Alloc(size_t bytes)
{
    void *rv = NULL;

    if (bytes <= MAX_SIZE) {
        /* Always allocate a non-zero amount of bytes */
        rv = PR_Malloc(bytes ? bytes : 1);
    }
    if (!rv) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
    }
    return rv;
}

void *
PORT_Realloc(void *oldptr, size_t bytes)
{
    void *rv = NULL;

    if (bytes <= MAX_SIZE) {
        rv = PR_Realloc(oldptr, bytes);
    }
    if (!rv) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
    }
    return rv;
}

void *
PORT_ZAlloc(size_t bytes)
{
    void *rv = NULL;

    if (bytes <= MAX_SIZE) {
        /* Always allocate a non-zero amount of bytes */
        rv = PR_Calloc(1, bytes ? bytes : 1);
    }
    if (!rv) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
    }
    return rv;
}

/* aligned_alloc is C11. This is an alternative to get aligned memory. */
void *
PORT_ZAllocAligned(size_t bytes, size_t alignment, void **mem)
{
    size_t x = alignment - 1;

    /* This only works if alignment is a power of 2. */
    if ((alignment == 0) || (alignment & (alignment - 1))) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    if (!mem) {
        return NULL;
    }

    /* Always allocate a non-zero amount of bytes */
    *mem = PORT_ZAlloc((bytes ? bytes : 1) + x);
    if (!*mem) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    return (void *)(((uintptr_t)*mem + x) & ~(uintptr_t)x);
}

void *
PORT_ZAllocAlignedOffset(size_t size, size_t alignment, size_t offset)
{
    PORT_Assert(offset < size);
    if (offset > size) {
        return NULL;
    }

    void *mem = NULL;
    void *v = PORT_ZAllocAligned(size, alignment, &mem);
    if (!v) {
        return NULL;
    }

    PORT_Assert(mem);
    *((void **)((uintptr_t)v + offset)) = mem;
    return v;
}

void
PORT_Free(void *ptr)
{
    if (ptr) {
        PR_Free(ptr);
    }
}

void
PORT_ZFree(void *ptr, size_t len)
{
    if (ptr) {
        memset(ptr, 0, len);
        PR_Free(ptr);
    }
}

char *
PORT_Strdup(const char *str)
{
    size_t len = PORT_Strlen(str) + 1;
    char *newstr;

    newstr = (char *)PORT_Alloc(len);
    if (newstr) {
        PORT_Memcpy(newstr, str, len);
    }
    return newstr;
}

void
PORT_SetError(int value)
{
    PR_SetError(value, 0);
    return;
}

int
PORT_GetError(void)
{
    return (PR_GetError());
}

/********************* Arena code follows *****************************
 * ArenaPools are like heaps.  The memory in them consists of large blocks,
 * called arenas, which are allocated from the/a system heap.  Inside an
 * ArenaPool, the arenas are organized as if they were in a stack.  Newly
 * allocated arenas are "pushed" on that stack.  When you attempt to
 * allocate memory from an ArenaPool, the code first looks to see if there
 * is enough unused space in the top arena on the stack to satisfy your
 * request, and if so, your request is satisfied from that arena.
 * Otherwise, a new arena is allocated (or taken from NSPR's list of freed
 * arenas) and pushed on to the stack.  The new arena is always big enough
 * to satisfy the request, and is also at least a minimum size that is
 * established at the time that the ArenaPool is created.
 *
 * The ArenaMark function returns the address of a marker in the arena at
 * the top of the arena stack.  It is the address of the place in the arena
 * on the top of the arena stack from which the next block of memory will
 * be allocated.  Each ArenaPool has its own separate stack, and hence
 * marks are only relevant to the ArenaPool from which they are gotten.
 * Marks may be nested.  That is, a thread can get a mark, and then get
 * another mark.
 *
 * It is intended that all the marks in an ArenaPool may only be owned by a
 * single thread.  In DEBUG builds, this is enforced.  In non-DEBUG builds,
 * it is not.  In DEBUG builds, when a thread gets a mark from an
 * ArenaPool, no other thread may acquire a mark in that ArenaPool while
 * that mark exists, that is, until that mark is unmarked or released.
 * Therefore, it is important that every mark be unmarked or released when
 * the creating thread has no further need for exclusive ownership of the
 * right to manage the ArenaPool.
 *
 * The ArenaUnmark function discards the ArenaMark at the address given,
 * and all marks nested inside that mark (that is, acquired from that same
 * ArenaPool while that mark existed).   It is an error for a thread other
 * than the mark's creator to try to unmark it.  When a thread has unmarked
 * all its marks from an ArenaPool, then another thread is able to set
 * marks in that ArenaPool.  ArenaUnmark does not deallocate (or "pop") any
 * memory allocated from the ArenaPool since the mark was created.
 *
 * ArenaRelease "pops" the stack back to the mark, deallocating all the
 * memory allocated from the arenas in the ArenaPool since that mark was
 * created, and removing any arenas from the ArenaPool that have no
 * remaining active allocations when that is done.  It implicitly releases
 * any marks nested inside the mark being explicitly released.  It is the
 * only operation, other than destroying the arenapool, that potentially
 * reduces the number of arenas on the stack.  Otherwise, the stack grows
 * until the arenapool is destroyed, at which point all the arenas are
 * freed or returned to a "free arena list", depending on their sizes.
 */
PLArenaPool *
PORT_NewArena(unsigned long chunksize)
{
    PORTArenaPool *pool;

    if (chunksize > MAX_SIZE) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }
    pool = PORT_ZNew(PORTArenaPool);
    if (!pool) {
        return NULL;
    }
    pool->magic = ARENAPOOL_MAGIC;
    pool->lock = PZ_NewLock(nssILockArena);
    if (!pool->lock) {
        PORT_Free(pool);
        return NULL;
    }
    PL_InitArenaPool(&pool->arena, "security", chunksize, sizeof(double));
    return (&pool->arena);
}

void
PORT_InitCheapArena(PORTCheapArenaPool *pool, unsigned long chunksize)
{
    pool->magic = CHEAP_ARENAPOOL_MAGIC;
    PL_InitArenaPool(&pool->arena, "security", chunksize, sizeof(double));
}

void *
PORT_ArenaAlloc(PLArenaPool *arena, size_t size)
{
    void *p = NULL;

    PORTArenaPool *pool = (PORTArenaPool *)arena;

    if (size <= 0) {
        size = 1;
    }

    if (size > MAX_SIZE) {
        /* you lose. */
    } else
        /* Is it one of ours?  Assume so and check the magic */
        if (ARENAPOOL_MAGIC == pool->magic) {
        PZ_Lock(pool->lock);
#ifdef THREADMARK
        /* Most likely one of ours.  Is there a thread id? */
        if (pool->marking_thread &&
            pool->marking_thread != PR_GetCurrentThread()) {
            /* Another thread holds a mark in this arena */
            PZ_Unlock(pool->lock);
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            PORT_Assert(0);
            return NULL;
        } /* tid != null */
#endif    /* THREADMARK */
        PL_ARENA_ALLOCATE(p, arena, size);
        PZ_Unlock(pool->lock);
    } else {
        PL_ARENA_ALLOCATE(p, arena, size);
    }

    if (!p) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
    }

    return (p);
}

void *
PORT_ArenaZAlloc(PLArenaPool *arena, size_t size)
{
    void *p;

    if (size <= 0)
        size = 1;

    p = PORT_ArenaAlloc(arena, size);

    if (p) {
        PORT_Memset(p, 0, size);
    }

    return (p);
}

static PRCallOnceType setupUseFreeListOnce;
static PRBool useFreeList;

static PRStatus
SetupUseFreeList(void)
{
    useFreeList = (PR_GetEnvSecure("NSS_DISABLE_ARENA_FREE_LIST") == NULL);
    return PR_SUCCESS;
}

/*
 * If zero is true, zeroize the arena memory before freeing it.
 */
void
PORT_FreeArena(PLArenaPool *arena, PRBool zero)
{
    PORTArenaPool *pool = (PORTArenaPool *)arena;
    PRLock *lock = (PRLock *)0;
    size_t len = sizeof *arena;

    if (!pool)
        return;
    if (ARENAPOOL_MAGIC == pool->magic) {
        len = sizeof *pool;
        lock = pool->lock;
        PZ_Lock(lock);
    }
    if (zero) {
        PL_ClearArenaPool(arena, 0);
    }
    (void)PR_CallOnce(&setupUseFreeListOnce, &SetupUseFreeList);
    if (useFreeList) {
        PL_FreeArenaPool(arena);
    } else {
        PL_FinishArenaPool(arena);
    }
    PORT_ZFree(arena, len);
    if (lock) {
        PZ_Unlock(lock);
        PZ_DestroyLock(lock);
    }
}

void
PORT_DestroyCheapArena(PORTCheapArenaPool *pool)
{
    (void)PR_CallOnce(&setupUseFreeListOnce, &SetupUseFreeList);
    if (useFreeList) {
        PL_FreeArenaPool(&pool->arena);
    } else {
        PL_FinishArenaPool(&pool->arena);
    }
}

void *
PORT_ArenaGrow(PLArenaPool *arena, void *ptr, size_t oldsize, size_t newsize)
{
    PORTArenaPool *pool = (PORTArenaPool *)arena;
    PORT_Assert(newsize >= oldsize);

    if (newsize > MAX_SIZE) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    if (ARENAPOOL_MAGIC == pool->magic) {
        PZ_Lock(pool->lock);
        /* Do we do a THREADMARK check here? */
        PL_ARENA_GROW(ptr, arena, oldsize, (newsize - oldsize));
        PZ_Unlock(pool->lock);
    } else {
        PL_ARENA_GROW(ptr, arena, oldsize, (newsize - oldsize));
    }

    return (ptr);
}

void *
PORT_ArenaMark(PLArenaPool *arena)
{
    void *result;

    PORTArenaPool *pool = (PORTArenaPool *)arena;
    if (ARENAPOOL_MAGIC == pool->magic) {
        PZ_Lock(pool->lock);
#ifdef THREADMARK
        {
            threadmark_mark *tm, **pw;
            PRThread *currentThread = PR_GetCurrentThread();

            if (!pool->marking_thread) {
                /* First mark */
                pool->marking_thread = currentThread;
            } else if (currentThread != pool->marking_thread) {
                PZ_Unlock(pool->lock);
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                PORT_Assert(0);
                return NULL;
            }

            result = PL_ARENA_MARK(arena);
            PL_ARENA_ALLOCATE(tm, arena, sizeof(threadmark_mark));
            if (!tm) {
                PZ_Unlock(pool->lock);
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                return NULL;
            }

            tm->mark = result;
            tm->next = (threadmark_mark *)NULL;

            pw = &pool->first_mark;
            while (*pw) {
                pw = &(*pw)->next;
            }

            *pw = tm;
        }
#else  /* THREADMARK */
        result = PL_ARENA_MARK(arena);
#endif /* THREADMARK */
        PZ_Unlock(pool->lock);
    } else {
        /* a "pure" NSPR arena */
        result = PL_ARENA_MARK(arena);
    }
    return result;
}

/*
 * This function accesses the internals of PLArena, which is why it needs
 * to use the NSPR internal macro PL_MAKE_MEM_UNDEFINED before the memset
 * calls.
 *
 * We should move this function to NSPR as PL_ClearArenaAfterMark or add
 * a PL_ARENA_CLEAR_AND_RELEASE macro.
 *
 * TODO: remove the #ifdef PL_MAKE_MEM_UNDEFINED tests when NSPR 4.10+ is
 * widely available.
 */
static void
port_ArenaZeroAfterMark(PLArenaPool *arena, void *mark)
{
    PLArena *a = arena->current;
    if (a->base <= (PRUword)mark && (PRUword)mark <= a->avail) {
/* fast path: mark falls in the current arena */
#ifdef PL_MAKE_MEM_UNDEFINED
        PL_MAKE_MEM_UNDEFINED(mark, a->avail - (PRUword)mark);
#endif
        memset(mark, 0, a->avail - (PRUword)mark);
    } else {
        /* slow path: need to find the arena that mark falls in */
        for (a = arena->first.next; a; a = a->next) {
            PR_ASSERT(a->base <= a->avail && a->avail <= a->limit);
            if (a->base <= (PRUword)mark && (PRUword)mark <= a->avail) {
#ifdef PL_MAKE_MEM_UNDEFINED
                PL_MAKE_MEM_UNDEFINED(mark, a->avail - (PRUword)mark);
#endif
                memset(mark, 0, a->avail - (PRUword)mark);
                a = a->next;
                break;
            }
        }
        for (; a; a = a->next) {
            PR_ASSERT(a->base <= a->avail && a->avail <= a->limit);
#ifdef PL_MAKE_MEM_UNDEFINED
            PL_MAKE_MEM_UNDEFINED((void *)a->base, a->avail - a->base);
#endif
            memset((void *)a->base, 0, a->avail - a->base);
        }
    }
}

static void
port_ArenaRelease(PLArenaPool *arena, void *mark, PRBool zero)
{
    PORTArenaPool *pool = (PORTArenaPool *)arena;
    if (ARENAPOOL_MAGIC == pool->magic) {
        PZ_Lock(pool->lock);
#ifdef THREADMARK
        {
            threadmark_mark **pw;

            if (PR_GetCurrentThread() != pool->marking_thread) {
                PZ_Unlock(pool->lock);
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                PORT_Assert(0);
                return /* no error indication available */;
            }

            pw = &pool->first_mark;
            while (*pw && (mark != (*pw)->mark)) {
                pw = &(*pw)->next;
            }

            if (!*pw) {
                /* bad mark */
                PZ_Unlock(pool->lock);
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                PORT_Assert(0);
                return /* no error indication available */;
            }

            *pw = (threadmark_mark *)NULL;

            if (zero) {
                port_ArenaZeroAfterMark(arena, mark);
            }
            PL_ARENA_RELEASE(arena, mark);

            if (!pool->first_mark) {
                pool->marking_thread = (PRThread *)NULL;
            }
        }
#else  /* THREADMARK */
        if (zero) {
            port_ArenaZeroAfterMark(arena, mark);
        }
        PL_ARENA_RELEASE(arena, mark);
#endif /* THREADMARK */
        PZ_Unlock(pool->lock);
    } else {
        if (zero) {
            port_ArenaZeroAfterMark(arena, mark);
        }
        PL_ARENA_RELEASE(arena, mark);
    }
}

void
PORT_ArenaRelease(PLArenaPool *arena, void *mark)
{
    port_ArenaRelease(arena, mark, PR_FALSE);
}

/*
 * Zeroize the arena memory before releasing it.
 */
void
PORT_ArenaZRelease(PLArenaPool *arena, void *mark)
{
    port_ArenaRelease(arena, mark, PR_TRUE);
}

void
PORT_ArenaUnmark(PLArenaPool *arena, void *mark)
{
#ifdef THREADMARK
    PORTArenaPool *pool = (PORTArenaPool *)arena;
    if (ARENAPOOL_MAGIC == pool->magic) {
        threadmark_mark **pw;

        PZ_Lock(pool->lock);

        if (PR_GetCurrentThread() != pool->marking_thread) {
            PZ_Unlock(pool->lock);
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            PORT_Assert(0);
            return /* no error indication available */;
        }

        pw = &pool->first_mark;
        while (((threadmark_mark *)NULL != *pw) && (mark != (*pw)->mark)) {
            pw = &(*pw)->next;
        }

        if ((threadmark_mark *)NULL == *pw) {
            /* bad mark */
            PZ_Unlock(pool->lock);
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            PORT_Assert(0);
            return /* no error indication available */;
        }

        *pw = (threadmark_mark *)NULL;

        if (!pool->first_mark) {
            pool->marking_thread = (PRThread *)NULL;
        }

        PZ_Unlock(pool->lock);
    }
#endif /* THREADMARK */
}

char *
PORT_ArenaStrdup(PLArenaPool *arena, const char *str)
{
    int len = PORT_Strlen(str) + 1;
    char *newstr;

    newstr = (char *)PORT_ArenaAlloc(arena, len);
    if (newstr) {
        PORT_Memcpy(newstr, str, len);
    }
    return newstr;
}

/********************** end of arena functions ***********************/

/****************** unicode conversion functions ***********************/
/*
 * NOTE: These conversion functions all assume that the multibyte
 * characters are going to be in NETWORK BYTE ORDER, not host byte
 * order.  This is because the only time we deal with UCS-2 and UCS-4
 * are when the data was received from or is going to be sent out
 * over the wire (in, e.g. certificates).
 */

void
PORT_SetUCS4_UTF8ConversionFunction(PORTCharConversionFunc convFunc)
{
    ucs4Utf8ConvertFunc = convFunc;
}

void
PORT_SetUCS2_ASCIIConversionFunction(PORTCharConversionWSwapFunc convFunc)
{
    ucs2AsciiConvertFunc = convFunc;
}

void
PORT_SetUCS2_UTF8ConversionFunction(PORTCharConversionFunc convFunc)
{
    ucs2Utf8ConvertFunc = convFunc;
}

PRBool
PORT_UCS4_UTF8Conversion(PRBool toUnicode, unsigned char *inBuf,
                         unsigned int inBufLen, unsigned char *outBuf,
                         unsigned int maxOutBufLen, unsigned int *outBufLen)
{
    if (!ucs4Utf8ConvertFunc) {
        return sec_port_ucs4_utf8_conversion_function(toUnicode,
                                                      inBuf, inBufLen, outBuf, maxOutBufLen, outBufLen);
    }

    return (*ucs4Utf8ConvertFunc)(toUnicode, inBuf, inBufLen, outBuf,
                                  maxOutBufLen, outBufLen);
}

PRBool
PORT_UCS2_UTF8Conversion(PRBool toUnicode, unsigned char *inBuf,
                         unsigned int inBufLen, unsigned char *outBuf,
                         unsigned int maxOutBufLen, unsigned int *outBufLen)
{
    if (!ucs2Utf8ConvertFunc) {
        return sec_port_ucs2_utf8_conversion_function(toUnicode,
                                                      inBuf, inBufLen, outBuf, maxOutBufLen, outBufLen);
    }

    return (*ucs2Utf8ConvertFunc)(toUnicode, inBuf, inBufLen, outBuf,
                                  maxOutBufLen, outBufLen);
}

PRBool
PORT_ISO88591_UTF8Conversion(const unsigned char *inBuf,
                             unsigned int inBufLen, unsigned char *outBuf,
                             unsigned int maxOutBufLen, unsigned int *outBufLen)
{
    return sec_port_iso88591_utf8_conversion_function(inBuf, inBufLen,
                                                      outBuf, maxOutBufLen, outBufLen);
}

PRBool
PORT_UCS2_ASCIIConversion(PRBool toUnicode, unsigned char *inBuf,
                          unsigned int inBufLen, unsigned char *outBuf,
                          unsigned int maxOutBufLen, unsigned int *outBufLen,
                          PRBool swapBytes)
{
    if (!ucs2AsciiConvertFunc) {
        return PR_FALSE;
    }

    return (*ucs2AsciiConvertFunc)(toUnicode, inBuf, inBufLen, outBuf,
                                   maxOutBufLen, outBufLen, swapBytes);
}

/* Portable putenv.  Creates/replaces an environment variable of the form
 *  envVarName=envValue
 */
int
NSS_PutEnv(const char *envVarName, const char *envValue)
{
    SECStatus result = SECSuccess;
    char *encoded;
    int putEnvFailed;
#ifdef _WIN32
    PRBool setOK;

    setOK = SetEnvironmentVariable(envVarName, envValue);
    if (!setOK) {
        SET_ERROR_CODE
        return SECFailure;
    }
#endif

    encoded = (char *)PORT_ZAlloc(strlen(envVarName) + 2 + strlen(envValue));
    if (!encoded) {
        return SECFailure;
    }
    strcpy(encoded, envVarName);
    strcat(encoded, "=");
    strcat(encoded, envValue);

    putEnvFailed = putenv(encoded); /* adopt. */
    if (putEnvFailed) {
        SET_ERROR_CODE
        result = SECFailure;
        PORT_Free(encoded);
    }
    return result;
}

/*
 * Perform a constant-time compare of two memory regions. The return value is
 * 0 if the memory regions are equal and non-zero otherwise.
 */
int
NSS_SecureMemcmp(const void *ia, const void *ib, size_t n)
{
    const unsigned char *a = (const unsigned char *)ia;
    const unsigned char *b = (const unsigned char *)ib;
    size_t i;
    unsigned char r = 0;

    for (i = 0; i < n; ++i) {
        r |= *a++ ^ *b++;
    }

    return r;
}

/*
 * Perform a constant-time check if a memory region is all 0. The return value
 * is 0 if the memory region is all zero.
 */
unsigned int
NSS_SecureMemcmpZero(const void *mem, size_t n)
{
    PRUint8 zero = 0;
    size_t i;
    for (i = 0; i < n; ++i) {
        zero |= *(PRUint8 *)((uintptr_t)mem + i);
    }
    return zero;
}
