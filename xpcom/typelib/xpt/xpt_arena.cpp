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
struct BLK_HDR
{
    BLK_HDR *next;
};

#define XPT_MIN_BLOCK_SIZE 32

/* XXX this is lame. Should clone the code to do this bitwise */
#define ALIGN_RND(s,a) ((a)==1?(s):((((s)+(a)-1)/(a))*(a)))

struct XPTSubArena
{
    BLK_HDR *first;
    uint8_t *next;
    size_t   space;
    size_t   block_size;
};

struct XPTArena
{
    // We have one sub-arena with 8-byte alignment for most allocations, and
    // one with 1-byte alignment for C string allocations. The latter sub-arena
    // avoids significant amounts of unnecessary padding between C strings.
    XPTSubArena subarena8;
    XPTSubArena subarena1;
};

XPT_PUBLIC_API(XPTArena *)
XPT_NewArena(size_t block_size8, size_t block_size1)
{
    XPTArena *arena = static_cast<XPTArena*>(calloc(1, sizeof(XPTArena)));
    if (arena) {
        if (block_size8 < XPT_MIN_BLOCK_SIZE)
            block_size8 = XPT_MIN_BLOCK_SIZE;
        arena->subarena8.block_size = ALIGN_RND(block_size8, 8);

        if (block_size1 < XPT_MIN_BLOCK_SIZE)
            block_size1 = XPT_MIN_BLOCK_SIZE;
        arena->subarena1.block_size = block_size1;
    }
    return arena;
}

static void
DestroySubArena(XPTSubArena *subarena)
{
    BLK_HDR* cur = subarena->first;
    while (cur) {
        BLK_HDR* next = cur->next;
        free(cur);
        cur = next;
    }
}

XPT_PUBLIC_API(void)
XPT_DestroyArena(XPTArena *arena)
{
    DestroySubArena(&arena->subarena8);
    DestroySubArena(&arena->subarena1);
    free(arena);
}

/*
* Our alignment rule is that we always round up the size of each allocation
* so that the 'arena->next' pointer one will point to properly aligned space.
*/

XPT_PUBLIC_API(void *)
XPT_ArenaCalloc(XPTArena *arena, size_t size, size_t alignment)
{
    if (!size)
        return NULL;

    if (!arena) {
        XPT_ASSERT(0);
        return NULL;
    }

    XPTSubArena *subarena;
    if (alignment == 8) {
        subarena = &arena->subarena8;
    } else if (alignment == 1) {
        subarena = &arena->subarena1;
    } else {
        XPT_ASSERT(0);
        return NULL;
    }

    size_t bytes = ALIGN_RND(size, alignment);

    if (bytes > subarena->space) {
        BLK_HDR* new_block;
        size_t block_header_size = ALIGN_RND(sizeof(BLK_HDR), alignment);
        size_t new_space = subarena->block_size;

        while (bytes > new_space - block_header_size)
            new_space += subarena->block_size;

        new_block =
            static_cast<BLK_HDR*>(calloc(new_space / alignment, alignment));
        if (!new_block) {
            subarena->next = NULL;
            subarena->space = 0;
            return NULL;
        }

        /* link block into the list of blocks for use when we destroy */
        new_block->next = subarena->first;
        subarena->first = new_block;

        /* set info for current block */
        subarena->next =
            reinterpret_cast<uint8_t*>(new_block) + block_header_size;
        subarena->space = new_space - block_header_size;

#ifdef DEBUG
        /* mark block for corruption check */
        memset(subarena->next, 0xcd, subarena->space);
#endif
    }

#ifdef DEBUG
    {
        /* do corruption check */
        size_t i;
        for (i = 0; i < bytes; ++i) {
            XPT_ASSERT(subarena->next[i] == 0xcd);
        }
        /* we guarantee that the block will be filled with zeros */
        memset(subarena->next, 0, bytes);
    }
#endif

    uint8_t* p = subarena->next;
    subarena->next  += bytes;
    subarena->space -= bytes;

    return p;
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

static size_t
SizeOfSubArenaExcludingThis(XPTSubArena *subarena, MozMallocSizeOf mallocSizeOf)
{
    size_t n = 0;

    BLK_HDR* cur = subarena->first;
    while (cur) {
        BLK_HDR* next = cur->next;
        n += mallocSizeOf(cur);
        cur = next;
    }

    return n;
}

XPT_PUBLIC_API(size_t)
XPT_SizeOfArenaIncludingThis(XPTArena *arena, MozMallocSizeOf mallocSizeOf)
{
    size_t n = mallocSizeOf(arena);
    n += SizeOfSubArenaExcludingThis(&arena->subarena8, mallocSizeOf);
    n += SizeOfSubArenaExcludingThis(&arena->subarena1, mallocSizeOf);
    return n;
}
