/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "primpl.h"

#include <stdlib.h>
#include <stddef.h>

/* Lock used to lock the monitor cache */
#ifdef _PR_NO_PREEMPT
#define _PR_NEW_LOCK_MCACHE()
#define _PR_LOCK_MCACHE()
#define _PR_UNLOCK_MCACHE()
#else
#ifdef _PR_LOCAL_THREADS_ONLY
#define _PR_NEW_LOCK_MCACHE()
#define _PR_LOCK_MCACHE() { PRIntn _is; _PR_INTSOFF(_is)
#define _PR_UNLOCK_MCACHE() _PR_INTSON(_is); }
#else
PRLock *_pr_mcacheLock;
#define _PR_NEW_LOCK_MCACHE() (_pr_mcacheLock = PR_NewLock())
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

static PRUint32 hash_mask;
static PRUintn num_hash_buckets;
static PRUintn num_hash_buckets_log2;
static MonitorCacheEntry **hash_buckets;
static MonitorCacheEntry *free_entries;
static PRUintn num_free_entries;
static PRBool expanding;
int _pr_mcache_ready;

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
    MonitorCacheEntry **new_hash_buckets, *new_entries;

    entries = 1L << new_size_log2;

    /*
    ** Expand the monitor-cache-entry free list
    */
    new_entries = (MonitorCacheEntry*)
        PR_CALLOC(entries * sizeof(MonitorCacheEntry));
    if (NULL == new_entries) return PR_FAILURE;

    /*
    ** Allocate system monitors for the new monitor cache entries. If we
    ** run out of system monitors, break out of the loop.
    */
    for (i = 0, added = 0, p = new_entries; i < entries; i++, p++, added++) {
        p->mon = PR_NewMonitor();
        if (!p->mon)
            break;
    }
    if (added != entries) {
        if (added == 0) {
            /* Totally out of system monitors. Lossage abounds */
            PR_DELETE(new_entries);
            return PR_FAILURE;
        }

        /*
        ** We were able to allocate some of the system monitors. Use
        ** realloc to shrink down the new_entries memory
        */
        p = (MonitorCacheEntry*)
            PR_REALLOC(new_entries, added * sizeof(MonitorCacheEntry));
        if (p == 0) {
            /*
            ** Total lossage. We just leaked a bunch of system monitors
            ** all over the floor. This should never ever happen.
            */
            PR_ASSERT(p != 0);
            return PR_FAILURE;
        }
    }

    /*
    ** Now that we have allocated all of the system monitors, build up
    ** the new free list. We can just update the free_list because we own
    ** the mcache-lock and we aren't calling anyone who might want to use
    ** it.
    */
    for (i = 0, p = new_entries; i < added - 1; i++, p++)
        p->next = p + 1;
    p->next = free_entries;
    free_entries = new_entries;
    num_free_entries += added;

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
            if (p->cacheEntryCount > 0)
                return pp;
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
        if (p->address == address) goto gotit;

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
            if (PR_FAILURE == rv)  return NULL;

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
    if (OnMonitorRecycle && p->address)
        OnMonitorRecycle(p->address);
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
    _pr_mcache_ready = 1;
}

/*
** Create monitor for address. Don't enter the monitor while we have the
** mcache locked because we might block!
*/
PR_IMPLEMENT(PRMonitor*) PR_CEnterMonitor(void *address)
{
    PRMonitor *mon;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    _PR_LOCK_MCACHE();
    mon = CreateMonitor(address);
    _PR_UNLOCK_MCACHE();

    if (!mon) return NULL;

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

    if (mon == NULL)
        return PR_FAILURE;
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

    if (mon == NULL)
        return PR_FAILURE;
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

    if (mon == NULL)
        return PR_FAILURE;
    return PR_NotifyAll(mon);
}

PR_IMPLEMENT(void)
PR_CSetOnMonitorRecycle(void (*callback)(void *address))
{
    OnMonitorRecycle = callback;
}
