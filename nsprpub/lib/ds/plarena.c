/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Lifetime-based fast allocation, inspired by much prior art, including
 * "Fast Allocation and Deallocation of Memory Based on Object Lifetimes"
 * David R. Hanson, Software -- Practice and Experience, Vol. 20(1).
 */
#include <stdlib.h>
#include <string.h>
#include "plarena.h"
#include "prmem.h"
#include "prbit.h"
#include "prlog.h"
#include "prlock.h"
#include "prinit.h"

#ifdef PL_ARENAMETER
static PLArenaStats *arena_stats_list;

#define COUNT(pool,what)  (pool)->stats.what++
#else
#define COUNT(pool,what)  /* nothing */
#endif

#define PL_ARENA_DEFAULT_ALIGN  sizeof(double)

PR_IMPLEMENT(void) PL_InitArenaPool(
    PLArenaPool *pool, const char *name, PRUint32 size, PRUint32 align)
{
    /*
     * Look-up table of PR_BITMASK(PR_CeilingLog2(align)) values for
     * align = 1 to 32.
     */
    static const PRUint8 pmasks[33] = {
         0,                                               /*  not used */
         0, 1, 3, 3, 7, 7, 7, 7,15,15,15,15,15,15,15,15,  /*  1 ... 16 */
        31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31}; /* 17 ... 32 */

    if (align == 0)
        align = PL_ARENA_DEFAULT_ALIGN;

    if (align < sizeof(pmasks)/sizeof(pmasks[0]))
        pool->mask = pmasks[align];
    else
        pool->mask = PR_BITMASK(PR_CeilingLog2(align));

    pool->first.next = NULL;
    /* Set all three addresses in pool->first to the same dummy value.
     * These addresses are only compared with each other, but never
     * dereferenced. */
    pool->first.base = pool->first.avail = pool->first.limit =
        (PRUword)PL_ARENA_ALIGN(pool, &pool->first + 1);
    pool->current = &pool->first;
    /*
     * Compute the net size so that each arena's gross size is |size|.
     * sizeof(PLArena) + pool->mask is the header and alignment slop
     * that PL_ArenaAllocate adds to the net size.
     */
    if (size > sizeof(PLArena) + pool->mask)
        pool->arenasize = size - (sizeof(PLArena) + pool->mask);
    else
        pool->arenasize = size;
#ifdef PL_ARENAMETER
    memset(&pool->stats, 0, sizeof pool->stats);
    pool->stats.name = strdup(name);
    pool->stats.next = arena_stats_list;
    arena_stats_list = &pool->stats;
#endif
}


/*
** PL_ArenaAllocate() -- allocate space from an arena pool
** 
** Description: PL_ArenaAllocate() allocates space from an arena
** pool. 
**
** First, try to satisfy the request from arenas starting at
** pool->current. Then try to allocate a new arena from the heap.
**
** Returns: pointer to allocated space or NULL
** 
** Notes: The original implementation had some difficult to
** solve bugs; the code was difficult to read. Sometimes it's
** just easier to rewrite it. I did that. larryh.
**
** See also: bugzilla: 45343.
**
*/

PR_IMPLEMENT(void *) PL_ArenaAllocate(PLArenaPool *pool, PRUint32 nb)
{
    PLArena *a;   
    char *rp;     /* returned pointer */
    PRUint32 nbOld;

    PR_ASSERT((nb & pool->mask) == 0);
    
    nbOld = nb;
    nb = (PRUword)PL_ARENA_ALIGN(pool, nb); /* force alignment */
    if (nb < nbOld)
        return NULL;

    /* attempt to allocate from arenas at pool->current */
    {
        a = pool->current;
        do {
            if ( nb <= a->limit - a->avail )  {
                pool->current = a;
                rp = (char *)a->avail;
                a->avail += nb;
                return rp;
            }
        } while( NULL != (a = a->next) );
    }

    /* attempt to allocate from the heap */ 
    {  
        PRUint32 sz = PR_MAX(pool->arenasize, nb);
        if (PR_UINT32_MAX - sz < sizeof *a + pool->mask) {
            a = NULL;
        } else {
            sz += sizeof *a + pool->mask;  /* header and alignment slop */
            a = (PLArena*)PR_MALLOC(sz);
        }
        if ( NULL != a )  {
            a->limit = (PRUword)a + sz;
            a->base = a->avail = (PRUword)PL_ARENA_ALIGN(pool, a + 1);
            PL_MAKE_MEM_NOACCESS((void*)a->avail, a->limit - a->avail);
            rp = (char *)a->avail;
            a->avail += nb;
            PR_ASSERT(a->avail <= a->limit);
            /* the newly allocated arena is linked after pool->current 
            *  and becomes pool->current */
            a->next = pool->current->next;
            pool->current->next = a;
            pool->current = a;
            if ( NULL == pool->first.next )
                pool->first.next = a;
            PL_COUNT_ARENA(pool,++);
            COUNT(pool, nmallocs);
            return(rp);
        }
    }

    /* we got to here, and there's no memory to allocate */
    return(NULL);
} /* --- end PL_ArenaAllocate() --- */

PR_IMPLEMENT(void *) PL_ArenaGrow(
    PLArenaPool *pool, void *p, PRUint32 size, PRUint32 incr)
{
    void *newp;

    if (PR_UINT32_MAX - size < incr)
        return NULL;
    PL_ARENA_ALLOCATE(newp, pool, size + incr);
    if (newp)
        memcpy(newp, p, size);
    return newp;
}

PR_IMPLEMENT(void) PL_ClearArenaPool(PLArenaPool *pool, PRInt32 pattern)
{
    PLArena *a;

    for (a = pool->first.next; a; a = a->next) {
        PR_ASSERT(a->base <= a->avail && a->avail <= a->limit);
        a->avail = a->base;
        PL_CLEAR_UNUSED_PATTERN(a, pattern);
        PL_MAKE_MEM_NOACCESS((void*)a->avail, a->limit - a->avail);
    }
}

/*
 * Free tail arenas linked after head, which may not be the true list head.
 * Reset pool->current to point to head in case it pointed at a tail arena.
 */
static void FreeArenaList(PLArenaPool *pool, PLArena *head)
{
    PLArena *a = head->next;
    if (!a)
        return;

    head->next = NULL;

    do {
        PLArena *tmp = a;
        a = a->next;
        PL_CLEAR_ARENA(tmp);
        PL_COUNT_ARENA(pool,--);
        PR_DELETE(tmp);
    } while (a);

    pool->current = head;
}

PR_IMPLEMENT(void) PL_ArenaRelease(PLArenaPool *pool, char *mark)
{
    PLArena *a;

    for (a = &pool->first; a; a = a->next) {
        if (PR_UPTRDIFF(mark, a->base) <= PR_UPTRDIFF(a->avail, a->base)) {
            a->avail = (PRUword)PL_ARENA_ALIGN(pool, mark);
            FreeArenaList(pool, a);
            return;
        }
    }
}

PR_IMPLEMENT(void) PL_FreeArenaPool(PLArenaPool *pool)
{
    FreeArenaList(pool, &pool->first);
    COUNT(pool, ndeallocs);
}

PR_IMPLEMENT(void) PL_FinishArenaPool(PLArenaPool *pool)
{
    FreeArenaList(pool, &pool->first);
#ifdef PL_ARENAMETER
    {
        PLArenaStats *stats, **statsp;

        if (pool->stats.name)
            PR_DELETE(pool->stats.name);
        for (statsp = &arena_stats_list; (stats = *statsp) != 0;
             statsp = &stats->next) {
            if (stats == &pool->stats) {
                *statsp = stats->next;
                return;
            }
        }
    }
#endif
}

PR_IMPLEMENT(void) PL_CompactArenaPool(PLArenaPool *ap)
{
}

PR_IMPLEMENT(void) PL_ArenaFinish(void)
{
}

PR_IMPLEMENT(size_t) PL_SizeOfArenaPoolExcludingPool(
    const PLArenaPool *pool, PLMallocSizeFn mallocSizeOf)
{
    /*
     * The first PLArena is within |pool|, so don't measure it.  Subsequent
     * PLArenas are separate and must be measured.
     */
    size_t size = 0;
    const PLArena *arena = pool->first.next;
    while (arena) {
        size += mallocSizeOf(arena);
        arena = arena->next;
    }
    return size;
}

#ifdef PL_ARENAMETER
PR_IMPLEMENT(void) PL_ArenaCountAllocation(PLArenaPool *pool, PRUint32 nb)
{
    pool->stats.nallocs++;
    pool->stats.nbytes += nb;
    if (nb > pool->stats.maxalloc)
        pool->stats.maxalloc = nb;
    pool->stats.variance += nb * nb;
}

PR_IMPLEMENT(void) PL_ArenaCountInplaceGrowth(
    PLArenaPool *pool, PRUint32 size, PRUint32 incr)
{
    pool->stats.ninplace++;
}

PR_IMPLEMENT(void) PL_ArenaCountGrowth(
    PLArenaPool *pool, PRUint32 size, PRUint32 incr)
{
    pool->stats.ngrows++;
    pool->stats.nbytes += incr;
    pool->stats.variance -= size * size;
    size += incr;
    if (size > pool->stats.maxalloc)
        pool->stats.maxalloc = size;
    pool->stats.variance += size * size;
}

PR_IMPLEMENT(void) PL_ArenaCountRelease(PLArenaPool *pool, char *mark)
{
    pool->stats.nreleases++;
}

PR_IMPLEMENT(void) PL_ArenaCountRetract(PLArenaPool *pool, char *mark)
{
    pool->stats.nfastrels++;
}

#include <math.h>
#include <stdio.h>

PR_IMPLEMENT(void) PL_DumpArenaStats(FILE *fp)
{
    PLArenaStats *stats;
    double mean, variance;

    for (stats = arena_stats_list; stats; stats = stats->next) {
        if (stats->nallocs != 0) {
            mean = (double)stats->nbytes / stats->nallocs;
            variance = fabs(stats->variance / stats->nallocs - mean * mean);
        } else {
            mean = variance = 0;
        }

        fprintf(fp, "\n%s allocation statistics:\n", stats->name);
        fprintf(fp, "              number of arenas: %u\n", stats->narenas);
        fprintf(fp, "         number of allocations: %u\n", stats->nallocs);
        fprintf(fp, " number of free arena reclaims: %u\n", stats->nreclaims);
        fprintf(fp, "        number of malloc calls: %u\n", stats->nmallocs);
        fprintf(fp, "       number of deallocations: %u\n", stats->ndeallocs);
        fprintf(fp, "  number of allocation growths: %u\n", stats->ngrows);
        fprintf(fp, "    number of in-place growths: %u\n", stats->ninplace);
        fprintf(fp, "number of released allocations: %u\n", stats->nreleases);
        fprintf(fp, "       number of fast releases: %u\n", stats->nfastrels);
        fprintf(fp, "         total bytes allocated: %u\n", stats->nbytes);
        fprintf(fp, "          mean allocation size: %g\n", mean);
        fprintf(fp, "            standard deviation: %g\n", sqrt(variance));
        fprintf(fp, "       maximum allocation size: %u\n", stats->maxalloc);
    }
}
#endif /* PL_ARENAMETER */
