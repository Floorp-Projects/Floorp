/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
#include "prmon.h"
#include "prinit.h"

static PLArena *arena_freelist;

#ifdef PL_ARENAMETER
static PLArenaStats *arena_stats_list;

#define COUNT(pool,what)  (pool)->stats.what++
#else
#define COUNT(pool,what)  /* nothing */
#endif

#define PL_ARENA_DEFAULT_ALIGN  sizeof(double)

static PRMonitor    *arenaLock;
static PRCallOnceType once;

/*
** InitializeArenas() -- Initialize arena operations.
**
** InitializeArenas() is called exactly once and only once from 
** LockArena(). This function creates the arena protection 
** monitor: arenaLock.
**
** Note: If the arenaLock cannot be created, InitializeArenas()
** fails quietly, returning only PR_FAILURE. This percolates up
** to the application using the Arena API. He gets no arena
** from PL_ArenaAllocate(). It's up to him to fail gracefully
** or recover.
**
*/
static PRStatus InitializeArenas( void )
{
    PR_ASSERT( arenaLock == NULL );
    arenaLock = PR_NewMonitor();
    if ( arenaLock == NULL )
        return PR_FAILURE;
    else
        return PR_SUCCESS;
} /* end ArenaInitialize() */

static PRStatus LockArena( void )
{
    PRStatus rc = PR_CallOnce( &once, InitializeArenas );

    if ( PR_FAILURE != rc )
        PR_EnterMonitor( arenaLock );
    return(rc);
} /* end LockArena() */

static void UnlockArena( void )
{
    PR_ExitMonitor( arenaLock );
    return;
} /* end UnlockArena() */

PR_IMPLEMENT(void) PL_InitArenaPool(
    PLArenaPool *pool, const char *name, PRUint32 size, PRUint32 align)
{
#if defined(XP_MAC)
#pragma unused (name)
#endif

    if (align == 0)
        align = PL_ARENA_DEFAULT_ALIGN;
    pool->mask = PR_BITMASK(PR_CeilingLog2(align));
    pool->first.next = NULL;
    pool->first.base = pool->first.avail = pool->first.limit =
        (PRUword)PL_ARENA_ALIGN(pool, &pool->first + 1);
    pool->current = &pool->first;
    pool->arenasize = size;
#ifdef PL_ARENAMETER
    memset(&pool->stats, 0, sizeof pool->stats);
    pool->stats.name = strdup(name);
    pool->stats.next = arena_stats_list;
    arena_stats_list = &pool->stats;
#endif
}

PR_IMPLEMENT(void *) PL_ArenaAllocate(PLArenaPool *pool, PRUint32 nb)
{
    PLArena **ap, *a, *b;
    PRUint32 sz;
    void *p;

    PR_ASSERT((nb & pool->mask) == 0);
#if defined(WIN16)
    if (nb >= 60000U)
        return 0;
#endif  /* WIN16 */
    if ( PR_FAILURE == LockArena())
        return(0);
    ap = &arena_freelist;
    for (a = pool->current; a->avail + nb > a->limit; pool->current = a) {
        if (a->next) {                          /* move to next arena */
            a = a->next;
            continue;
        }
        while ((b = *ap) != 0) {                /* reclaim a free arena */
            if (b->limit - b->base == pool->arenasize) {
                *ap = b->next;
                b->next = 0;
                a = a->next = b;
                COUNT(pool, nreclaims);
                goto claim;
            }
            ap = &b->next;
        }
        sz = PR_MAX(pool->arenasize, nb);        /* allocate a new arena */
        sz += sizeof *a + pool->mask;           /* header and alignment slop */
        b = (PLArena*)PR_MALLOC(sz);
        if (!b)
        {
            UnlockArena();
            return 0;
        }
        a = a->next = b;
        a->next = 0;
        a->limit = (PRUword)a + sz;
        PL_COUNT_ARENA(pool,++);
        COUNT(pool, nmallocs);
    claim:
        a->base = a->avail = (PRUword)PL_ARENA_ALIGN(pool, a + 1);
    }
    UnlockArena();
    p = (void *)a->avail;
    a->avail += nb;
    return p;
}

PR_IMPLEMENT(void *) PL_ArenaGrow(
    PLArenaPool *pool, void *p, PRUint32 size, PRUint32 incr)
{
    void *newp;

    PL_ARENA_ALLOCATE(newp, pool, size + incr);
    if (newp)
        memcpy(newp, p, size);
    return newp;
}

/*
 * Free tail arenas linked after head, which may not be the true list head.
 * Reset pool->current to point to head in case it pointed at a tail arena.
 */
static void FreeArenaList(PLArenaPool *pool, PLArena *head, PRBool reallyFree)
{
    PLArena **ap, *a;

    ap = &head->next;
    a = *ap;
    if (!a)
        return;

#ifdef DEBUG
    do {
        PR_ASSERT(a->base <= a->avail && a->avail <= a->limit);
        a->avail = a->base;
        PL_CLEAR_UNUSED(a);
    } while ((a = a->next) != 0);
    a = *ap;
#endif

    if (reallyFree) {
        do {
            *ap = a->next;
            PL_CLEAR_ARENA(a);
            PL_COUNT_ARENA(pool,--);
            PR_DELETE(a);
        } while ((a = *ap) != 0);
    } else {
        /* Insert the whole arena chain at the front of the freelist. */
        do {
            ap = &(*ap)->next;
        } while (*ap);
        LockArena();
        *ap = arena_freelist;
        arena_freelist = a;
        head->next = 0;
        UnlockArena();
    }

    pool->current = head;
}

PR_IMPLEMENT(void) PL_ArenaRelease(PLArenaPool *pool, char *mark)
{
    PLArena *a;

    for (a = pool->first.next; a; a = a->next) {
        if (PR_UPTRDIFF(mark, a) < PR_UPTRDIFF(a->avail, a)) {
            a->avail = (PRUword)PL_ARENA_ALIGN(pool, mark);
            FreeArenaList(pool, a, PR_TRUE);
            return;
        }
    }
}

PR_IMPLEMENT(void) PL_FreeArenaPool(PLArenaPool *pool)
{
    FreeArenaList(pool, &pool->first, PR_FALSE);
    COUNT(pool, ndeallocs);
}

PR_IMPLEMENT(void) PL_FinishArenaPool(PLArenaPool *pool)
{
    FreeArenaList(pool, &pool->first, PR_TRUE);
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
#if XP_MAC
#pragma unused (ap)
#if 0
    PRArena *curr = &(ap->first);
    while (curr) {
        reallocSmaller(curr, curr->avail - (uprword_t)curr);
        curr->limit = curr->avail;
        curr = curr->next;
    }
#endif
#endif
}

PR_IMPLEMENT(void) PL_ArenaFinish()
{
    PLArena *a, *next;

    LockArena();
    for (a = arena_freelist; a; a = next) {
        next = a->next;
        PR_DELETE(a);
    }
    arena_freelist = NULL;
    UnlockArena();
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
