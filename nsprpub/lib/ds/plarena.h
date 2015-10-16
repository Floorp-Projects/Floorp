/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef plarena_h___
#define plarena_h___
/*
 * Lifetime-based fast allocation, inspired by much prior art, including
 * "Fast Allocation and Deallocation of Memory Based on Object Lifetimes"
 * David R. Hanson, Software -- Practice and Experience, Vol. 20(1).
 *
 * Also supports LIFO allocation (PL_ARENA_MARK/PL_ARENA_RELEASE).
 */
#include "prtypes.h"
#include "plarenas.h"

PR_BEGIN_EXTERN_C

typedef struct PLArena          PLArena;

struct PLArena {
    PLArena     *next;          /* next arena for this lifetime */
    PRUword     base;           /* aligned base address, follows this header */
    PRUword     limit;          /* one beyond last byte in arena */
    PRUword     avail;          /* points to next available byte */
};

#ifdef PL_ARENAMETER
typedef struct PLArenaStats PLArenaStats;

struct PLArenaStats {
    PLArenaStats  *next;        /* next in arenaStats list */
    char          *name;        /* name for debugging */
    PRUint32      narenas;      /* number of arenas in pool */
    PRUint32      nallocs;      /* number of PL_ARENA_ALLOCATE() calls */
    PRUint32      nreclaims;    /* number of reclaims from freeArenas */
    PRUint32      nmallocs;     /* number of malloc() calls */
    PRUint32      ndeallocs;    /* number of lifetime deallocations */
    PRUint32      ngrows;       /* number of PL_ARENA_GROW() calls */
    PRUint32      ninplace;     /* number of in-place growths */
    PRUint32      nreleases;    /* number of PL_ARENA_RELEASE() calls */
    PRUint32      nfastrels;    /* number of "fast path" releases */
    PRUint32      nbytes;       /* total bytes allocated */
    PRUint32      maxalloc;     /* maximum allocation size in bytes */
    PRFloat64     variance;     /* size variance accumulator */
};
#endif

struct PLArenaPool {
    PLArena     first;          /* first arena in pool list */
    PLArena     *current;       /* arena from which to allocate space */
    PRUint32    arenasize;      /* net exact size of a new arena */
    PRUword     mask;           /* alignment mask (power-of-2 - 1) */
#ifdef PL_ARENAMETER
    PLArenaStats stats;
#endif
};

/*
 * WARNING: The PL_MAKE_MEM_ macros are for internal use by NSPR. Do NOT use
 * them in your code.
 *
 * NOTE: Valgrind support to be added.
 *
 * The PL_MAKE_MEM_ macros are modeled after the MOZ_MAKE_MEM_ macros in
 * Mozilla's mfbt/MemoryChecking.h. Only AddressSanitizer is supported now.
 *
 * Provides a common interface to the ASan (AddressSanitizer) and Valgrind
 * functions used to mark memory in certain ways. In detail, the following
 * three macros are provided:
 *
 *   PL_MAKE_MEM_NOACCESS  - Mark memory as unsafe to access (e.g. freed)
 *   PL_MAKE_MEM_UNDEFINED - Mark memory as accessible, with content undefined
 *   PL_MAKE_MEM_DEFINED - Mark memory as accessible, with content defined
 *
 * With Valgrind in use, these directly map to the three respective Valgrind
 * macros. With ASan in use, the NOACCESS macro maps to poisoning the memory,
 * while the UNDEFINED/DEFINED macros unpoison memory.
 *
 * With no memory checker available, all macros expand to the empty statement.
 */

/* WARNING: PL_SANITIZE_ADDRESS is for internal use by this header. Do NOT
 * define or test this macro in your code.
 */
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define PL_SANITIZE_ADDRESS 1
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define PL_SANITIZE_ADDRESS 1
#endif

#if defined(PL_SANITIZE_ADDRESS)

/* These definitions are usually provided through the
 * sanitizer/asan_interface.h header installed by ASan.
 * See https://code.google.com/p/address-sanitizer/wiki/ManualPoisoning
 */

void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);

#define PL_MAKE_MEM_NOACCESS(addr, size) \
    __asan_poison_memory_region((addr), (size))

#define PL_MAKE_MEM_UNDEFINED(addr, size) \
    __asan_unpoison_memory_region((addr), (size))

#define PL_MAKE_MEM_DEFINED(addr, size) \
    __asan_unpoison_memory_region((addr), (size))

#else

#define PL_MAKE_MEM_NOACCESS(addr, size)
#define PL_MAKE_MEM_UNDEFINED(addr, size)
#define PL_MAKE_MEM_DEFINED(addr, size)

#endif

/*
 * If the including .c file uses only one power-of-2 alignment, it may define
 * PL_ARENA_CONST_ALIGN_MASK to the alignment mask and save a few instructions
 * per ALLOCATE and GROW.
 */
#ifdef PL_ARENA_CONST_ALIGN_MASK
#define PL_ARENA_ALIGN(pool, n) (((PRUword)(n) + PL_ARENA_CONST_ALIGN_MASK) \
                                & ~PL_ARENA_CONST_ALIGN_MASK)

#define PL_INIT_ARENA_POOL(pool, name, size) \
        PL_InitArenaPool(pool, name, size, PL_ARENA_CONST_ALIGN_MASK + 1)
#else
#define PL_ARENA_ALIGN(pool, n) (((PRUword)(n) + (pool)->mask) & ~(pool)->mask)
#endif

#define PL_ARENA_ALLOCATE(p, pool, nb) \
    PR_BEGIN_MACRO \
        PLArena *_a = (pool)->current; \
        PRUint32 _nb = PL_ARENA_ALIGN(pool, nb); \
        PRUword _p = _a->avail; \
        PRUword _q = _p + _nb; \
        if (_q > _a->limit) { \
            _p = (PRUword)PL_ArenaAllocate(pool, _nb); \
        } else { \
            _a->avail = _q; \
        } \
        p = (void *)_p; \
        PL_MAKE_MEM_UNDEFINED(p, nb); \
        PL_ArenaCountAllocation(pool, nb); \
    PR_END_MACRO

#define PL_ARENA_GROW(p, pool, size, incr) \
    PR_BEGIN_MACRO \
        PLArena *_a = (pool)->current; \
        PRUint32 _incr = PL_ARENA_ALIGN(pool, incr); \
        PRUword _p = _a->avail; \
        PRUword _q = _p + _incr; \
        if (_p == (PRUword)(p) + PL_ARENA_ALIGN(pool, size) && \
            _q <= _a->limit) { \
            PL_MAKE_MEM_UNDEFINED((unsigned char *)(p) + size, incr); \
            _a->avail = _q; \
            PL_ArenaCountInplaceGrowth(pool, size, incr); \
        } else { \
            p = PL_ArenaGrow(pool, p, size, incr); \
        } \
        PL_ArenaCountGrowth(pool, size, incr); \
    PR_END_MACRO

#define PL_ARENA_MARK(pool) ((void *) (pool)->current->avail)
#define PR_UPTRDIFF(p,q) ((PRUword)(p) - (PRUword)(q))

#define PL_CLEAR_UNUSED_PATTERN(a, pattern) \
    PR_BEGIN_MACRO \
        PR_ASSERT((a)->avail <= (a)->limit); \
        PL_MAKE_MEM_UNDEFINED((void*)(a)->avail, (a)->limit - (a)->avail); \
        memset((void*)(a)->avail, (pattern), (a)->limit - (a)->avail); \
    PR_END_MACRO
#ifdef DEBUG
#define PL_FREE_PATTERN 0xDA
#define PL_CLEAR_UNUSED(a) PL_CLEAR_UNUSED_PATTERN((a), PL_FREE_PATTERN)
#define PL_CLEAR_ARENA(a) \
    PR_BEGIN_MACRO \
        PL_MAKE_MEM_UNDEFINED((void*)(a), (a)->limit - (PRUword)(a)); \
        memset((void*)(a), PL_FREE_PATTERN, (a)->limit - (PRUword)(a)); \
    PR_END_MACRO
#else
#define PL_CLEAR_UNUSED(a)
#define PL_CLEAR_ARENA(a)
#endif

#define PL_ARENA_RELEASE(pool, mark) \
    PR_BEGIN_MACRO \
        char *_m = (char *)(mark); \
        PLArena *_a = (pool)->current; \
        if (PR_UPTRDIFF(_m, _a->base) <= PR_UPTRDIFF(_a->avail, _a->base)) { \
            _a->avail = (PRUword)PL_ARENA_ALIGN(pool, _m); \
            PL_CLEAR_UNUSED(_a); \
            PL_MAKE_MEM_NOACCESS((void*)_a->avail, _a->limit - _a->avail); \
            PL_ArenaCountRetract(pool, _m); \
        } else { \
            PL_ArenaRelease(pool, _m); \
        } \
        PL_ArenaCountRelease(pool, _m); \
    PR_END_MACRO

#ifdef PL_ARENAMETER
#define PL_COUNT_ARENA(pool,op) ((pool)->stats.narenas op)
#else
#define PL_COUNT_ARENA(pool,op)
#endif

#define PL_ARENA_DESTROY(pool, a, pnext) \
    PR_BEGIN_MACRO \
        PL_COUNT_ARENA(pool,--); \
        if ((pool)->current == (a)) (pool)->current = &(pool)->first; \
        *(pnext) = (a)->next; \
        PL_CLEAR_ARENA(a); \
        free(a); \
        (a) = 0; \
    PR_END_MACRO

#ifdef PL_ARENAMETER

#include <stdio.h>

PR_EXTERN(void) PL_ArenaCountAllocation(PLArenaPool *pool, PRUint32 nb);

PR_EXTERN(void) PL_ArenaCountInplaceGrowth(
    PLArenaPool *pool, PRUint32 size, PRUint32 incr);

PR_EXTERN(void) PL_ArenaCountGrowth(
    PLArenaPool *pool, PRUint32 size, PRUint32 incr);

PR_EXTERN(void) PL_ArenaCountRelease(PLArenaPool *pool, char *mark);

PR_EXTERN(void) PL_ArenaCountRetract(PLArenaPool *pool, char *mark);

PR_EXTERN(void) PL_DumpArenaStats(FILE *fp);

#else  /* !PL_ARENAMETER */

#define PL_ArenaCountAllocation(ap, nb)                 /* nothing */
#define PL_ArenaCountInplaceGrowth(ap, size, incr)      /* nothing */
#define PL_ArenaCountGrowth(ap, size, incr)             /* nothing */
#define PL_ArenaCountRelease(ap, mark)                  /* nothing */
#define PL_ArenaCountRetract(ap, mark)                  /* nothing */

#endif /* !PL_ARENAMETER */

PR_END_EXTERN_C

#endif /* plarena_h___ */
