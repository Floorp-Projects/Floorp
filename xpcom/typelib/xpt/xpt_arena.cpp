/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Quick arena hack for xpt. */

/* XXX This exists because we don't want to drag in NSPR. It *seemed*
*  to make more sense to write a quick and dirty arena than to clone
*  plarena (like js/src did). This is not optimal, but it works. 
*/

#include "xpt_arena.h"
#include "mozilla/MemoryReporting.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
};

XPT_PUBLIC_API(XPTArena *)
XPT_NewArena(uint32_t block_size, size_t alignment)
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

    BLK_HDR* cur = arena->first;
    while (cur) {
        BLK_HDR* next = cur->next;
        n += mallocSizeOf(cur);
        cur = next;
    }

    return n;
}
