/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PLARENAS_H
#define PLARENAS_H

PR_BEGIN_EXTERN_C

typedef struct PLArenaPool      PLArenaPool;

/*
** Initialize an arena pool with the given name for debugging and metering,
** with a minimum gross size per arena of size bytes.  The net size per arena
** is smaller than the gross size by a header of four pointers plus any
** necessary padding for alignment.
**
** Note: choose a gross size that's a power of two to avoid the heap allocator
** rounding the size up.
**/
PR_EXTERN(void) PL_InitArenaPool(
    PLArenaPool *pool, const char *name, PRUint32 size, PRUint32 align);

/*
** Finish using arenas, freeing all memory associated with them.
**/
PR_EXTERN(void) PL_ArenaFinish(void);

/*
** Free the arenas in pool.  The user may continue to allocate from pool
** after calling this function.  There is no need to call PL_InitArenaPool()
** again unless PL_FinishArenaPool(pool) has been called.
**/
PR_EXTERN(void) PL_FreeArenaPool(PLArenaPool *pool);

/*
** Free the arenas in pool and finish using it altogether.
**/
PR_EXTERN(void) PL_FinishArenaPool(PLArenaPool *pool);

/*
** Compact all of the arenas in a pool so that no space is wasted.
** NOT IMPLEMENTED.  Do not use.
**/
PR_EXTERN(void) PL_CompactArenaPool(PLArenaPool *pool);

/*
** Friend functions used by the PL_ARENA_*() macros.
**
** WARNING: do not call these functions directly. Always use the
** PL_ARENA_*() macros.
**/
PR_EXTERN(void *) PL_ArenaAllocate(PLArenaPool *pool, PRUint32 nb);

PR_EXTERN(void *) PL_ArenaGrow(
    PLArenaPool *pool, void *p, PRUint32 size, PRUint32 incr);

PR_EXTERN(void) PL_ArenaRelease(PLArenaPool *pool, char *mark);

/*
** memset contents of all arenas in pool to pattern
*/
PR_EXTERN(void) PL_ClearArenaPool(PLArenaPool *pool, PRInt32 pattern);

/*
** A function like malloc_size() or malloc_usable_size() that measures the
** size of a heap block.
*/
typedef size_t (*PLMallocSizeFn)(const void *ptr);

/*
** Measure all memory used by a PLArenaPool, excluding the PLArenaPool
** structure.
*/
PR_EXTERN(size_t) PL_SizeOfArenaPoolExcludingPool(
    const PLArenaPool *pool, PLMallocSizeFn mallocSizeOf);

PR_END_EXTERN_C

#endif /* PLARENAS_H */
