/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

#include <stdlib.h>
#include <stddef.h>

/* Lock used to lock the monitor cache */
#ifdef _PR_NO_PREEMPT
#define _PR_NEW_LOCK_MCACHE()
#define _PR_DESTROY_LOCK_MCACHE()
#define _PR_LOCK_MCACHE()
#define _PR_UNLOCK_MCACHE()
#else
#ifdef _PR_LOCAL_THREADS_ONLY
#define _PR_NEW_LOCK_MCACHE()
#define _PR_DESTROY_LOCK_MCACHE()
#define _PR_LOCK_MCACHE() { PRIntn _is; _PR_INTSOFF(_is)
#define _PR_UNLOCK_MCACHE() _PR_INTSON(_is); }
#else
PRLock *_pr_mcacheLock;
#define _PR_NEW_LOCK_MCACHE() (_pr_mcacheLock = PR_NewLock())
#define _PR_DESTROY_LOCK_MCACHE()               \
    PR_BEGIN_MACRO                              \
        if (_pr_mcacheLock) {                   \
            PR_DestroyLock(_pr_mcacheLock);     \
            _pr_mcacheLock = NULL;              \
        }                                       \
    PR_END_MACRO
#define _PR_LOCK_MCACHE() PR_Lock(_pr_mcacheLock)
#define _PR_UNLOCK_MCACHE() PR_Unlock(_pr_mcacheLock)
#endif
#endif

/************************************************************************/

typedef struct MonitorCacheEntryStr MonitorCacheEntry;

struct MonitorCacheEntryStr {
    MonitorCacheEntry*  next;
    void*               address;
    PRMonitor*          mon;
    long                cacheEntryCount;
};

/*
** An array of MonitorCacheEntry's, plus a pointer to link these
** arrays together.
*/

typedef struct MonitorCacheEntryBlockStr MonitorCacheEntryBlock;

struct MonitorCacheEntryBlockStr {
    MonitorCacheEntryBlock* next;
    MonitorCacheEntry entries[1];
};

static PRUint32 hash_mask;
static PRUintn num_hash_buckets;
static PRUintn num_hash_buckets_log2;
static MonitorCacheEntry **hash_buckets;
static MonitorCacheEntry *free_entries;
static PRUintn num_free_entries;
static PRBool expanding;
static MonitorCacheEntryBlock *mcache_blocks;

static void (*OnMonitorRecycle)(void *address);

#define HASH(address)                               \
    ((PRUint32) ( ((PRUptrdiff)(address) >> 2) ^    \
                  ((PRUptrdiff)(address) >> 10) )   \
     & hash_mask)

/*
** Expand the monitor cache. This grows the hash buckets and allocates a
** new chunk of cache entries and throws them on the free list. We keep
** as many hash buckets as there are entries.
**
** Because we call malloc and malloc may need the monitor cache, we must
** ensure that there are several free monitor cache entries available for
** malloc to get. FREE_THRESHOLD is used to prevent monitor cache
** starvation during monitor cache expansion.
*/

#define FREE_THRESHOLD  5

static PRStatus ExpandMonitorCache(PRUintn new_size_log2)
{
    MonitorCacheEntry **old_hash_buckets, *p;
    PRUintn i, entries, old_num_hash_buckets, added;
    MonitorCacheEntry **new_hash_buckets;
    MonitorCacheEntryBlock *new_block;

    entries = 1L << new_size_log2;

    /*
    ** Expand the monitor-cache-entry free list
    */
    new_block = (MonitorCacheEntryBlock*)
                PR_CALLOC(sizeof(MonitorCacheEntryBlock)
                          + (entries - 1) * sizeof(MonitorCacheEntry));
    if (NULL == new_block) {
        return PR_FAILURE;
    }

    /*
    ** Allocate system monitors for the new monitor cache entries. If we
    ** run out of system monitors, break out of the loop.
    */
    for (i = 0, p = new_block->entries; i < entries; i++, p++) {
        p->mon = PR_NewMonitor();
        if (!p->mon) {
            break;
        }
    }
    added = i;
    if (added != entries) {
        MonitorCacheEntryBlock *realloc_block;

        if (added == 0) {
            /* Totally out of system monitors. Lossage abounds */
            PR_DELETE(new_block);
            return PR_FAILURE;
        }

        /*
        ** We were able to allocate some of the system monitors. Use
        ** realloc to shrink down the new_block memory. If that fails,
        ** carry on with the too-large new_block.
        */
        realloc_block = (MonitorCacheEntryBlock*)
                        PR_REALLOC(new_block, sizeof(MonitorCacheEntryBlock)
                                   + (added - 1) * sizeof(MonitorCacheEntry));
        if (realloc_block) {
            new_block = realloc_block;
        }
    }

    /*
    ** Now that we have allocated all of the system monitors, build up
    ** the new free list. We can just update the free_list because we own
    ** the mcache-lock and we aren't calling anyone who might want to use
    ** it.
    */
    for (i = 0, p = new_block->entries; i < added - 1; i++, p++) {
        p->next = p + 1;
    }
    p->next = free_entries;
    free_entries = new_block->entries;
    num_free_entries += added;
    new_block->next = mcache_blocks;
    mcache_blocks = new_block;

    /* Try to expand the hash table */
    new_hash_buckets = (MonitorCacheEntry**)
                       PR_CALLOC(entries * sizeof(MonitorCacheEntry*));
    if (NULL == new_hash_buckets) {
        /*
        ** Partial lossage. In this situation we don't get any more hash
        ** buckets, which just means that the table lookups will take
        ** longer. This is bad, but not fatal
        */
        PR_LOG(_pr_cmon_lm, PR_LOG_WARNING,
               ("unable to grow monitor cache hash buckets"));
        return PR_SUCCESS;
    }

    /*
    ** Compute new hash mask value. This value is used to mask an address
    ** until it's bits are in the right spot for indexing into the hash
    ** table.
    */
    hash_mask = entries - 1;

    /*
    ** Expand the hash table. We have to rehash everything in the old
    ** table into the new table.
    */
    old_hash_buckets = hash_buckets;
    old_num_hash_buckets = num_hash_buckets;
    for (i = 0; i < old_num_hash_buckets; i++) {
        p = old_hash_buckets[i];
        while (p) {
            MonitorCacheEntry *next = p->next;

            /* Hash based on new table size, and then put p in the new table */
            PRUintn hash = HASH(p->address);
            p->next = new_hash_buckets[hash];
            new_hash_buckets[hash] = p;

            p = next;
        }
    }

    /*
    ** Switch over to new hash table and THEN call free of the old
    ** table. Since free might re-enter _pr_mcache_lock, things would
    ** break terribly if it used the old hash table.
    */
    hash_buckets = new_hash_buckets;
    num_hash_buckets = entries;
    num_hash_buckets_log2 = new_size_log2;
    PR_DELETE(old_hash_buckets);

    PR_LOG(_pr_cmon_lm, PR_LOG_NOTICE,
           ("expanded monitor cache to %d (buckets %d)",
            num_free_entries, entries));

    return PR_SUCCESS;
}  /* ExpandMonitorCache */

/*
** Lookup a monitor cache entry by address. Return a pointer to the
** pointer to the monitor cache entry on success, null on failure.
*/
static MonitorCacheEntry **LookupMonitorCacheEntry(void *address)
{
    PRUintn hash;
    MonitorCacheEntry **pp, *p;

    hash = HASH(address);
    pp = hash_buckets + hash;
    while ((p = *pp) != 0) {
        if (p->address == address) {
            if (p->cacheEntryCount > 0) {
                return pp;
            }
            return NULL;
        }
        pp = &p->next;
    }
    return NULL;
}

/*
** Try to create a new cached monitor. If it's already in the cache,
** great - return it. Otherwise get a new free cache entry and set it
** up. If the cache free space is getting low, expand the cache.
*/
static PRMonitor *CreateMonitor(void *address)
{
    PRUintn hash;
    MonitorCacheEntry **pp, *p;

    hash = HASH(address);
    pp = hash_buckets + hash;
    while ((p = *pp) != 0) {
        if (p->address == address) {
            goto gotit;
        }

        pp = &p->next;
    }

    /* Expand the monitor cache if we have run out of free slots in the table */
    if (num_free_entries < FREE_THRESHOLD) {
        /* Expand monitor cache */

        /*
        ** This function is called with the lock held. So what's the 'expanding'
        ** boolean all about? Seems a bit redundant.
        */
        if (!expanding) {
            PRStatus rv;

            expanding = PR_TRUE;
            rv = ExpandMonitorCache(num_hash_buckets_log2 + 1);
            expanding = PR_FALSE;
            if (PR_FAILURE == rv) {
                return NULL;
            }

            /* redo the hash because it'll be different now */
            hash = HASH(address);
        } else {
            /*
            ** We are in process of expanding and we need a cache
            ** monitor.  Make sure we have enough!
            */
            PR_ASSERT(num_free_entries > 0);
        }
    }

    /* Make a new monitor */
    p = free_entries;
    free_entries = p->next;
    num_free_entries--;
    if (OnMonitorRecycle && p->address) {
        OnMonitorRecycle(p->address);
    }
    p->address = address;
    p->next = hash_buckets[hash];
    hash_buckets[hash] = p;
    PR_ASSERT(p->cacheEntryCount == 0);

gotit:
    p->cacheEntryCount++;
    return p->mon;
}

/*
** Initialize the monitor cache
*/
void _PR_InitCMon(void)
{
    _PR_NEW_LOCK_MCACHE();
    ExpandMonitorCache(3);
}

/*
** Destroy the monitor cache
*/
void _PR_CleanupCMon(void)
{
    _PR_DESTROY_LOCK_MCACHE();

    while (free_entries) {
        PR_DestroyMonitor(free_entries->mon);
        free_entries = free_entries->next;
    }
    num_free_entries = 0;

    while (mcache_blocks) {
        MonitorCacheEntryBlock *block;

        block = mcache_blocks;
        mcache_blocks = block->next;
        PR_DELETE(block);
    }

    PR_DELETE(hash_buckets);
    hash_mask = 0;
    num_hash_buckets = 0;
    num_hash_buckets_log2 = 0;

    expanding = PR_FALSE;
    OnMonitorRecycle = NULL;
}

/*
** Create monitor for address. Don't enter the monitor while we have the
** mcache locked because we might block!
*/
PR_IMPLEMENT(PRMonitor*) PR_CEnterMonitor(void *address)
{
    PRMonitor *mon;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }

    _PR_LOCK_MCACHE();
    mon = CreateMonitor(address);
    _PR_UNLOCK_MCACHE();

    if (!mon) {
        return NULL;
    }

    PR_EnterMonitor(mon);
    return mon;
}

PR_IMPLEMENT(PRStatus) PR_CExitMonitor(void *address)
{
    MonitorCacheEntry **pp, *p;
    PRStatus status = PR_SUCCESS;

    _PR_LOCK_MCACHE();
    pp = LookupMonitorCacheEntry(address);
    if (pp != NULL) {
        p = *pp;
        if (--p->cacheEntryCount == 0) {
            /*
            ** Nobody is using the system monitor. Put it on the cached free
            ** list. We are safe from somebody trying to use it because we
            ** have the mcache locked.
            */
            p->address = 0;             /* defensive move */
            *pp = p->next;              /* unlink from hash_buckets */
            p->next = free_entries;     /* link into free list */
            free_entries = p;
            num_free_entries++;         /* count it as free */
        }
        status = PR_ExitMonitor(p->mon);
    } else {
        status = PR_FAILURE;
    }
    _PR_UNLOCK_MCACHE();

    return status;
}

PR_IMPLEMENT(PRStatus) PR_CWait(void *address, PRIntervalTime ticks)
{
    MonitorCacheEntry **pp;
    PRMonitor *mon;

    _PR_LOCK_MCACHE();
    pp = LookupMonitorCacheEntry(address);
    mon = pp ? ((*pp)->mon) : NULL;
    _PR_UNLOCK_MCACHE();

    if (mon == NULL) {
        return PR_FAILURE;
    }
    return PR_Wait(mon, ticks);
}

PR_IMPLEMENT(PRStatus) PR_CNotify(void *address)
{
    MonitorCacheEntry **pp;
    PRMonitor *mon;

    _PR_LOCK_MCACHE();
    pp = LookupMonitorCacheEntry(address);
    mon = pp ? ((*pp)->mon) : NULL;
    _PR_UNLOCK_MCACHE();

    if (mon == NULL) {
        return PR_FAILURE;
    }
    return PR_Notify(mon);
}

PR_IMPLEMENT(PRStatus) PR_CNotifyAll(void *address)
{
    MonitorCacheEntry **pp;
    PRMonitor *mon;

    _PR_LOCK_MCACHE();
    pp = LookupMonitorCacheEntry(address);
    mon = pp ? ((*pp)->mon) : NULL;
    _PR_UNLOCK_MCACHE();

    if (mon == NULL) {
        return PR_FAILURE;
    }
    return PR_NotifyAll(mon);
}

PR_IMPLEMENT(void)
PR_CSetOnMonitorRecycle(void (*callback)(void *address))
{
    OnMonitorRecycle = callback;
}
