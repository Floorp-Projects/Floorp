/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Quick arena hack for xpt. */

/* XXX This exists because we don't want to drag in NSPR. It *seemed*
*  to make more sense to write a quick and dirty arena than to clone
*  plarena (like js/src did). This is not optimal, but it works. 
*  Half of the code here is instrumentation.
*/

#include "xpt_arena.h"
#include "mozilla/MemoryReporting.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*************************/
/* logging stats support */

#if 0 && defined(DEBUG_jband)
#define XPT_ARENA_LOGGING 1
#endif

#ifdef XPT_ARENA_LOGGING

#define LOG_MALLOC(_a, _req, _used)                             \
    do{                                                         \
    XPT_ASSERT((_a));                                           \
    ++(_a)->LOG_MallocCallCount;                                \
    (_a)->LOG_MallocTotalBytesRequested += (_req);              \
    (_a)->LOG_MallocTotalBytesUsed += (_used);                  \
    } while(0)

#define LOG_REAL_MALLOC(_a, _size)                              \
    do{                                                         \
    XPT_ASSERT((_a));                                           \
    ++(_a)->LOG_RealMallocCallCount;                            \
    (_a)->LOG_RealMallocTotalBytesRequested += (_size);         \
    } while(0)

#define LOG_FREE(_a)                                            \
    do{                                                         \
    XPT_ASSERT((_a));                                           \
    ++(_a)->LOG_FreeCallCount;                                  \
    } while(0)

#define LOG_DONE_LOADING(_a)                                    \
    do{                                                         \
    XPT_ASSERT((_a));                                           \
    (_a)->LOG_LoadingFreeCallCount = (_a)->LOG_FreeCallCount;   \
    } while(0)

#define PRINT_STATS(_a)       xpt_DebugPrintArenaStats((_a))
static void xpt_DebugPrintArenaStats(XPTArena *arena);

#else /* !XPT_ARENA_LOGGING */

#define LOG_MALLOC(_a, _req, _used)   ((void)0)
#define LOG_REAL_MALLOC(_a, _size)    ((void)0)
#define LOG_FREE(_a)                  ((void)0)

#define LOG_DONE_LOADING(_a)          ((void)0)        
#define PRINT_STATS(_a)               ((void)0)

#endif /* XPT_ARENA_LOGGING */

/****************************************************/

/* Block header for each block in the arena */
typedef struct BLK_HDR BLK_HDR;
struct BLK_HDR
{
    BLK_HDR *next;
    size_t   size;
};

#define XPT_MIN_BLOCK_SIZE 32

/* XXX this is lame. Should clone the code to do this bitwise */
#define ALIGN_RND(s,a) ((a)==1?(s):((((s)+(a)-1)/(a))*(a)))

struct XPTArena
{
    BLK_HDR *first;
    uint8_t *next;
    size_t   space;
    size_t   alignment;
    size_t   block_size;
    char    *name;

#ifdef XPT_ARENA_LOGGING
    uint32_t LOG_MallocCallCount;
    uint32_t LOG_MallocTotalBytesRequested;
    uint32_t LOG_MallocTotalBytesUsed;
    uint32_t LOG_FreeCallCount;
    uint32_t LOG_LoadingFreeCallCount;
    uint32_t LOG_RealMallocCallCount;
    uint32_t LOG_RealMallocTotalBytesRequested;
#endif /* XPT_ARENA_LOGGING */
};

XPT_PUBLIC_API(XPTArena *)
XPT_NewArena(uint32_t block_size, size_t alignment, const char* name)
{
    XPTArena *arena = (XPTArena*)calloc(1, sizeof(XPTArena));
    if (arena) {
        XPT_ASSERT(alignment);
        if (alignment > sizeof(double))
            alignment = sizeof(double);
        arena->alignment = alignment;

        if (block_size < XPT_MIN_BLOCK_SIZE)
            block_size = XPT_MIN_BLOCK_SIZE;
        arena->block_size = ALIGN_RND(block_size, alignment);

        /* must have room for at least one item! */
        XPT_ASSERT(arena->block_size >= 
                   ALIGN_RND(sizeof(BLK_HDR), alignment) +
                   ALIGN_RND(1, alignment));

        if (name) {
            arena->name = XPT_STRDUP(arena, name);           
#ifdef XPT_ARENA_LOGGING
            /* fudge the stats since we are using space in the arena */
            arena->LOG_MallocCallCount = 0;
            arena->LOG_MallocTotalBytesRequested = 0;
            arena->LOG_MallocTotalBytesUsed = 0;
#endif /* XPT_ARENA_LOGGING */
        }
    }
    return arena;        
}

XPT_PUBLIC_API(void)
XPT_DestroyArena(XPTArena *arena)
{
    BLK_HDR* cur;
    BLK_HDR* next;
        
    cur = arena->first;
    while (cur) {
        next = cur->next;
        free(cur);
        cur = next;
    }
    free(arena);
}

XPT_PUBLIC_API(void)
XPT_DumpStats(XPTArena *arena)
{
    PRINT_STATS(arena);
}        


/* 
* Our alignment rule is that we always round up the size of each allocation 
* so that the 'arena->next' pointer one will point to properly aligned space.
*/

XPT_PUBLIC_API(void *)
XPT_ArenaMalloc(XPTArena *arena, size_t size)
{
    uint8_t *cur;
    size_t bytes;

    if (!size)
        return NULL;

    if (!arena) {
        XPT_ASSERT(0);
        return NULL;
    }

    bytes = ALIGN_RND(size, arena->alignment);
    
    LOG_MALLOC(arena, size, bytes);

    if (bytes > arena->space) {
        BLK_HDR* new_block;
        size_t block_header_size = ALIGN_RND(sizeof(BLK_HDR), arena->alignment);
        size_t new_space = arena->block_size;
         
        while (bytes > new_space - block_header_size)
            new_space += arena->block_size;

        new_block = (BLK_HDR*) calloc(new_space/arena->alignment, 
                                      arena->alignment);
        if (!new_block) {
            arena->next = NULL;
            arena->space = 0;
            return NULL;
        }

        LOG_REAL_MALLOC(arena, new_space);

        /* link block into the list of blocks for use when we destroy */
        new_block->next = arena->first;
        arena->first = new_block;

        /* save other block header info */
        new_block->size = new_space;

        /* set info for current block */
        arena->next  = ((uint8_t*)new_block) + block_header_size;
        arena->space = new_space - block_header_size;

#ifdef DEBUG
        /* mark block for corruption check */
        memset(arena->next, 0xcd, arena->space);
#endif
    } 
    
#ifdef DEBUG
    {
        /* do corruption check */
        size_t i;
        for (i = 0; i < bytes; ++i) {
            XPT_ASSERT(arena->next[i] == 0xcd);        
        }
        /* we guarantee that the block will be filled with zeros */
        memset(arena->next, 0, bytes);
    }        
#endif

    cur = arena->next;
    arena->next  += bytes;
    arena->space -= bytes;
    
    return cur;    
}


XPT_PUBLIC_API(char *)
XPT_ArenaStrDup(XPTArena *arena, const char * s)
{
    size_t len;
    char* cur;

    if (!s)
        return NULL;

    len = strlen(s)+1;
    cur = (char*)XPT_ArenaMalloc(arena, len);
    memcpy(cur, s, len);
    return cur;
}

XPT_PUBLIC_API(void)
XPT_NotifyDoneLoading(XPTArena *arena)
{
#ifdef XPT_ARENA_LOGGING
    if (arena) {
        LOG_DONE_LOADING(arena);        
    }
#endif
}

XPT_PUBLIC_API(void)
XPT_ArenaFree(XPTArena *arena, void *block)
{
    LOG_FREE(arena);
}

#ifdef XPT_ARENA_LOGGING
static void xpt_DebugPrintArenaStats(XPTArena *arena)
{
    printf("()()()()()()()()()()()()()()()()()()()()()()()()()()()()()()()\n");
    printf("Start xpt arena stats for \"%s\"\n", 
            arena->name ? arena->name : "unnamed arena");
    printf("\n");
    printf("%d times arena malloc called\n", (int) arena->LOG_MallocCallCount);       
    printf("%d total bytes requested from arena malloc\n", (int) arena->LOG_MallocTotalBytesRequested);       
    printf("%d average bytes requested per call to arena malloc\n", (int)arena->LOG_MallocCallCount ? (arena->LOG_MallocTotalBytesRequested/arena->LOG_MallocCallCount) : 0);
    printf("%d average bytes used per call (accounts for alignment overhead)\n", (int)arena->LOG_MallocCallCount ? (arena->LOG_MallocTotalBytesUsed/arena->LOG_MallocCallCount) : 0);
    printf("%d average bytes used per call (accounts for all overhead and waste)\n", (int)arena->LOG_MallocCallCount ? (arena->LOG_RealMallocTotalBytesRequested/arena->LOG_MallocCallCount) : 0);
    printf("\n");
    printf("%d during loading times arena free called\n", (int) arena->LOG_LoadingFreeCallCount);       
    printf("%d during loading approx total bytes not freed\n", (int) arena->LOG_LoadingFreeCallCount * (int) (arena->LOG_MallocCallCount ? (arena->LOG_MallocTotalBytesUsed/arena->LOG_MallocCallCount) : 0));       
    printf("\n");
    printf("%d total times arena free called\n", (int) arena->LOG_FreeCallCount);       
    printf("%d approx total bytes not freed until arena destruction\n", (int) arena->LOG_FreeCallCount * (int) (arena->LOG_MallocCallCount ? (arena->LOG_MallocTotalBytesUsed/arena->LOG_MallocCallCount) : 0 ));       
    printf("\n");
    printf("%d times arena called system malloc\n", (int) arena->LOG_RealMallocCallCount);       
    printf("%d total bytes arena requested from system\n", (int) arena->LOG_RealMallocTotalBytesRequested);       
    printf("%d byte block size specified at arena creation time\n", (int) arena->block_size);       
    printf("%d byte block alignment specified at arena creation time\n", (int) arena->alignment);       
    printf("\n");
    printf("End xpt arena stats\n");
    printf("()()()()()()()()()()()()()()()()()()()()()()()()()()()()()()()\n");
}        
#endif

/***************************************************************************/

#ifdef DEBUG
XPT_PUBLIC_API(void)
XPT_AssertFailed(const char *s, const char *file, uint32_t lineno)
{
    fprintf(stderr, "Assertion failed: %s, file %s, line %d\n",
            s, file, lineno);
    abort();
}        
#endif

XPT_PUBLIC_API(size_t)
XPT_SizeOfArena(XPTArena *arena, MozMallocSizeOf mallocSizeOf)
{
    size_t n = mallocSizeOf(arena);

    /*
     * We don't measure arena->name separately because it's allocated out of
     * the arena itself.
     */

    BLK_HDR* cur;
    BLK_HDR* next;
        
    cur = arena->first;
    while (cur) {
        next = cur->next;
        n += mallocSizeOf(cur);
        cur = next;
    }

    return n;
}
