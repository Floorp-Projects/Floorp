/* vim:set ts=4 sw=4 sts=4 et cin: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
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

#if defined(MOZ_LOGGING)
#define FORCE_PR_LOG
#endif

#if defined(HAVE_RES_NINIT)
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>   
#include <arpa/nameser.h>
#include <resolv.h>
#define RES_RETRY_ON_FAILURE
#endif

#include <stdlib.h>
#include "nsHostResolver.h"
#include "nsNetError.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"
#include "nsAutoLock.h"
#include "nsAutoPtr.h"
#include "pratom.h"
#include "prthread.h"
#include "prerror.h"
#include "prcvar.h"
#include "prtime.h"
#include "prlong.h"
#include "prlog.h"
#include "pldhash.h"
#include "plstr.h"
#include "nsURLHelper.h"

//----------------------------------------------------------------------------

// Use a persistent thread pool in order to avoid spinning up new threads all the time.
// In particular, thread creation results in a res_init() call from libc which is 
// quite expensive.
//
// The pool dynamically grows between 0 and MAX_RESOLVER_THREADS in size. New requests
// go first to an idle thread. If that cannot be found and there are fewer than MAX_RESOLVER_THREADS
// currently in the pool a new thread is created for high priority requests. If
// the new request is at a lower priority a new thread will only be created if 
// there are fewer than HighThreadThreshold currently outstanding. If a thread cannot be
// created or an idle thread located for the request it is queued.
//
// When the pool is greater than HighThreadThreshold in size a thread will be destroyed after
// ShortIdleTimeoutSeconds of idle time. Smaller pools use LongIdleTimeoutSeconds for a 
// timeout period.

#define MAX_NON_PRIORITY_REQUESTS 150

#define HighThreadThreshold     MAX_RESOLVER_THREADS_FOR_ANY_PRIORITY
#define LongIdleTimeoutSeconds  300           // for threads 1 -> HighThreadThreshold
#define ShortIdleTimeoutSeconds 60            // for threads HighThreadThreshold+1 -> MAX_RESOLVER_THREADS

PR_STATIC_ASSERT (HighThreadThreshold <= MAX_RESOLVER_THREADS);

//----------------------------------------------------------------------------

#if defined(PR_LOGGING)
static PRLogModuleInfo *gHostResolverLog = nsnull;
#define LOG(args) PR_LOG(gHostResolverLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

//----------------------------------------------------------------------------

static inline void
MoveCList(PRCList &from, PRCList &to)
{
    if (!PR_CLIST_IS_EMPTY(&from)) {
        to.next = from.next;
        to.prev = from.prev;
        to.next->prev = &to;
        to.prev->next = &to;
        PR_INIT_CLIST(&from);
    }             
}

static PRUint32
NowInMinutes()
{
    PRTime now = PR_Now(), minutes, factor;
    LL_I2L(factor, 60 * PR_USEC_PER_SEC);
    LL_DIV(minutes, now, factor);
    PRUint32 result;
    LL_L2UI(result, minutes);
    return result;
}

//----------------------------------------------------------------------------

#if defined(RES_RETRY_ON_FAILURE)

// this class represents the resolver state for a given thread.  if we
// encounter a lookup failure, then we can invoke the Reset method on an
// instance of this class to reset the resolver (in case /etc/resolv.conf
// for example changed).  this is mainly an issue on GNU systems since glibc
// only reads in /etc/resolv.conf once per thread.  it may be an issue on
// other systems as well.

class nsResState
{
public:
    nsResState()
        // initialize mLastReset to the time when this object
        // is created.  this means that a reset will not occur
        // if a thread is too young.  the alternative would be
        // to initialize this to the beginning of time, so that
        // the first failure would cause a reset, but since the
        // thread would have just started up, it likely would
        // already have current /etc/resolv.conf info.
        : mLastReset(PR_IntervalNow())
    {
    }

    PRBool Reset()
    {
        // reset no more than once per second
        if (PR_IntervalToSeconds(PR_IntervalNow() - mLastReset) < 1)
            return PR_FALSE;

        LOG(("calling res_ninit\n"));

        mLastReset = PR_IntervalNow();
        return (res_ninit(&_res) == 0);
    }

private:
    PRIntervalTime mLastReset;
};

#endif // RES_RETRY_ON_FAILURE

//----------------------------------------------------------------------------

// this macro filters out any flags that are not used when constructing the
// host key.  the significant flags are those that would affect the resulting
// host record (i.e., the flags that are passed down to PR_GetAddrInfoByName).
#define RES_KEY_FLAGS(_f) ((_f) & nsHostResolver::RES_CANON_NAME)

nsresult
nsHostRecord::Create(const nsHostKey *key, nsHostRecord **result)
{
    PRLock *lock = PR_NewLock();
    if (!lock)
        return NS_ERROR_OUT_OF_MEMORY;

    size_t hostLen = strlen(key->host) + 1;
    size_t size = hostLen + sizeof(nsHostRecord);

    nsHostRecord *rec = (nsHostRecord*) ::operator new(size);
    if (!rec) {
        PR_DestroyLock(lock);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rec->host = ((char *) rec) + sizeof(nsHostRecord);
    rec->flags = key->flags;
    rec->af = key->af;

    rec->_refc = 1; // addref
    NS_LOG_ADDREF(rec, 1, "nsHostRecord", sizeof(nsHostRecord));
    rec->addr_info_lock = lock;
    rec->addr_info = nsnull;
    rec->addr_info_gencnt = 0;
    rec->addr = nsnull;
    rec->expiration = NowInMinutes();
    rec->resolving = PR_FALSE;
    rec->onQueue = PR_FALSE;
    PR_INIT_CLIST(rec);
    PR_INIT_CLIST(&rec->callbacks);
    rec->negative = PR_FALSE;
    memcpy((char *) rec->host, key->host, hostLen);

    *result = rec;
    return NS_OK;
}

nsHostRecord::~nsHostRecord()
{
    if (addr_info_lock)
        PR_DestroyLock(addr_info_lock);
    if (addr_info)
        PR_FreeAddrInfo(addr_info);
    if (addr)
        free(addr);
}

//----------------------------------------------------------------------------

struct nsHostDBEnt : PLDHashEntryHdr
{
    nsHostRecord *rec;
};

static PLDHashNumber
HostDB_HashKey(PLDHashTable *table, const void *key)
{
    const nsHostKey *hk = static_cast<const nsHostKey *>(key);
    return PL_DHashStringKey(table, hk->host) ^ RES_KEY_FLAGS(hk->flags) ^ hk->af;
}

static PRBool
HostDB_MatchEntry(PLDHashTable *table,
                  const PLDHashEntryHdr *entry,
                  const void *key)
{
    const nsHostDBEnt *he = static_cast<const nsHostDBEnt *>(entry);
    const nsHostKey *hk = static_cast<const nsHostKey *>(key); 

    return !strcmp(he->rec->host, hk->host) &&
            RES_KEY_FLAGS (he->rec->flags) == RES_KEY_FLAGS(hk->flags) &&
            he->rec->af == hk->af;
}

static void
HostDB_MoveEntry(PLDHashTable *table,
                 const PLDHashEntryHdr *from,
                 PLDHashEntryHdr *to)
{
    static_cast<nsHostDBEnt *>(to)->rec =
            static_cast<const nsHostDBEnt *>(from)->rec;
}

static void
HostDB_ClearEntry(PLDHashTable *table,
                  PLDHashEntryHdr *entry)
{
    LOG(("evicting record\n"));
    nsHostDBEnt *he = static_cast<nsHostDBEnt *>(entry);
#if defined(DEBUG) && defined(PR_LOGGING)
    if (!he->rec->addr_info)
        LOG(("%s: => no addr_info\n", he->rec->host));
    else {
        PRInt32 now = (PRInt32) NowInMinutes();
        PRInt32 diff = (PRInt32) he->rec->expiration - now;
        LOG(("%s: exp=%d => %s\n",
            he->rec->host, diff,
            PR_GetCanonNameFromAddrInfo(he->rec->addr_info)));
        void *iter = nsnull;
        PRNetAddr addr;
        char buf[64];
        for (;;) {
            iter = PR_EnumerateAddrInfo(iter, he->rec->addr_info, 0, &addr);
            if (!iter)
                break;
            PR_NetAddrToString(&addr, buf, sizeof(buf));
            LOG(("  %s\n", buf));
        }
    }
#endif
    NS_RELEASE(he->rec);
}

static PRBool
HostDB_InitEntry(PLDHashTable *table,
                 PLDHashEntryHdr *entry,
                 const void *key)
{
    nsHostDBEnt *he = static_cast<nsHostDBEnt *>(entry);
    nsHostRecord::Create(static_cast<const nsHostKey *>(key), &he->rec);
    return PR_TRUE;
}

static PLDHashTableOps gHostDB_ops =
{
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    HostDB_HashKey,
    HostDB_MatchEntry,
    HostDB_MoveEntry,
    HostDB_ClearEntry,
    PL_DHashFinalizeStub,
    HostDB_InitEntry,
};

static PLDHashOperator
HostDB_RemoveEntry(PLDHashTable *table,
                   PLDHashEntryHdr *hdr,
                   PRUint32 number,
                   void *arg)
{
    return PL_DHASH_REMOVE;
}

//----------------------------------------------------------------------------

nsHostResolver::nsHostResolver(PRUint32 maxCacheEntries,
                               PRUint32 maxCacheLifetime)
    : mMaxCacheEntries(maxCacheEntries)
    , mMaxCacheLifetime(maxCacheLifetime)
    , mLock(nsnull)
    , mIdleThreadCV(nsnull)
    , mNumIdleThreads(0)
    , mThreadCount(0)
    , mAnyPriorityThreadCount(0)
    , mEvictionQSize(0)
    , mPendingCount(0)
    , mShutdown(PR_TRUE)
{
    mCreationTime = PR_Now();
    PR_INIT_CLIST(&mHighQ);
    PR_INIT_CLIST(&mMediumQ);
    PR_INIT_CLIST(&mLowQ);
    PR_INIT_CLIST(&mEvictionQ);

    mHighPriorityInfo.self = this;
    mHighPriorityInfo.onlyHighPriority = PR_TRUE;
    mAnyPriorityInfo.self = this;
    mAnyPriorityInfo.onlyHighPriority = PR_FALSE;

    mLongIdleTimeout  = PR_SecondsToInterval(LongIdleTimeoutSeconds);
    mShortIdleTimeout = PR_SecondsToInterval(ShortIdleTimeoutSeconds);
}

nsHostResolver::~nsHostResolver()
{
    if (mIdleThreadCV)
        PR_DestroyCondVar(mIdleThreadCV);

    if (mLock)
        PR_DestroyLock(mLock);

    PL_DHashTableFinish(&mDB);
}

nsresult
nsHostResolver::Init()
{
    mLock = PR_NewLock();
    if (!mLock)
        return NS_ERROR_OUT_OF_MEMORY;

    mIdleThreadCV = PR_NewCondVar(mLock);
    if (!mIdleThreadCV)
        return NS_ERROR_OUT_OF_MEMORY;

    PL_DHashTableInit(&mDB, &gHostDB_ops, nsnull, sizeof(nsHostDBEnt), 0);

    mShutdown = PR_FALSE;

#if defined(HAVE_RES_NINIT)
    // We want to make sure the system is using the correct resolver settings,
    // so we force it to reload those settings whenever we startup a subsequent
    // nsHostResolver instance.  We assume that there is no reason to do this
    // for the first nsHostResolver instance since that is usually created
    // during application startup.
    static int initCount = 0;
    if (initCount++ > 0) {
        LOG(("calling res_ninit\n"));
        res_ninit(&_res);
    }
#endif
    return NS_OK;
}

void
nsHostResolver::ClearPendingQueue(PRCList *aPendingQ)
{
    // loop through pending queue, erroring out pending lookups.
    if (!PR_CLIST_IS_EMPTY(aPendingQ)) {
        PRCList *node = aPendingQ->next;
        while (node != aPendingQ) {
            nsHostRecord *rec = static_cast<nsHostRecord *>(node);
            node = node->next;
            OnLookupComplete(rec, NS_ERROR_ABORT, nsnull);
        }
    }
}

void
nsHostResolver::Shutdown()
{
    LOG(("nsHostResolver::Shutdown\n"));

    PRCList pendingQHigh, pendingQMed, pendingQLow, evictionQ;
    PR_INIT_CLIST(&pendingQHigh);
    PR_INIT_CLIST(&pendingQMed);
    PR_INIT_CLIST(&pendingQLow);
    PR_INIT_CLIST(&evictionQ);

    {
        nsAutoLock lock(mLock);
        
        mShutdown = PR_TRUE;

        MoveCList(mHighQ, pendingQHigh);
        MoveCList(mMediumQ, pendingQMed);
        MoveCList(mLowQ, pendingQLow);
        MoveCList(mEvictionQ, evictionQ);
        mEvictionQSize = 0;
        mPendingCount = 0;
        
        if (mNumIdleThreads)
            PR_NotifyAllCondVar(mIdleThreadCV);
        
        // empty host database
        PL_DHashTableEnumerate(&mDB, HostDB_RemoveEntry, nsnull);
    }
    
    ClearPendingQueue(&pendingQHigh);
    ClearPendingQueue(&pendingQMed);
    ClearPendingQueue(&pendingQLow);

    if (!PR_CLIST_IS_EMPTY(&evictionQ)) {
        PRCList *node = evictionQ.next;
        while (node != &evictionQ) {
            nsHostRecord *rec = static_cast<nsHostRecord *>(node);
            node = node->next;
            NS_RELEASE(rec);
        }
    }

#ifdef NS_BUILD_REFCNT_LOGGING
    
    // Logically join the outstanding worker threads with a timeout.
    // Use this approach instead of PR_JoinThread() because that does 
    // not allow a timeout which may be necessary for a semi-responsive 
    // shutdown if the thread is blocked on a very slow DNS resolution.
    // mThreadCount is read outside of mLock, but the worst case 
    // scenario for that race is one extra 25ms sleep.

    PRIntervalTime delay = PR_MillisecondsToInterval(25);
    PRIntervalTime stopTime = PR_IntervalNow() + PR_SecondsToInterval(20);
    while (mThreadCount && PR_IntervalNow() < stopTime)
        PR_Sleep(delay);
#endif
}

static inline PRBool
IsHighPriority(PRUint16 flags)
{
    return !(flags & (nsHostResolver::RES_PRIORITY_LOW | nsHostResolver::RES_PRIORITY_MEDIUM));
}

static inline PRBool
IsMediumPriority(PRUint16 flags)
{
    return flags & nsHostResolver::RES_PRIORITY_MEDIUM;
}

static inline PRBool
IsLowPriority(PRUint16 flags)
{
    return flags & nsHostResolver::RES_PRIORITY_LOW;
}

void 
nsHostResolver::MoveQueue(nsHostRecord *aRec, PRCList &aDestQ)
{
    NS_ASSERTION(aRec->onQueue, "Moving Host Record Not Currently Queued");
    
    PR_REMOVE_LINK(aRec);
    PR_APPEND_LINK(aRec, &aDestQ);
}

nsresult
nsHostResolver::ResolveHost(const char            *host,
                            PRUint16               flags,
                            PRUint16               af,
                            nsResolveHostCallback *callback)
{
    NS_ENSURE_TRUE(host && *host, NS_ERROR_UNEXPECTED);

    LOG(("nsHostResolver::ResolveHost [host=%s]\n", host));

    // ensure that we are working with a valid hostname before proceeding.  see
    // bug 304904 for details.
    if (!net_IsValidHostName(nsDependentCString(host)))
        return NS_ERROR_UNKNOWN_HOST;

    // if result is set inside the lock, then we need to issue the
    // callback before returning.
    nsRefPtr<nsHostRecord> result;
    nsresult status = NS_OK, rv = NS_OK;
    {
        nsAutoLock lock(mLock);

        if (mShutdown)
            rv = NS_ERROR_NOT_INITIALIZED;
        else {
            PRNetAddr tempAddr;

            // unfortunately, PR_StringToNetAddr does not properly initialize
            // the output buffer in the case of IPv6 input.  see bug 223145.
            memset(&tempAddr, 0, sizeof(PRNetAddr));
            
            // check to see if there is already an entry for this |host|
            // in the hash table.  if so, then check to see if we can't
            // just reuse the lookup result.  otherwise, if there are
            // any pending callbacks, then add to pending callbacks queue,
            // and return.  otherwise, add ourselves as first pending
            // callback, and proceed to do the lookup.

            nsHostKey key = { host, flags, af };
            nsHostDBEnt *he = static_cast<nsHostDBEnt *>
                                         (PL_DHashTableOperate(&mDB, &key, PL_DHASH_ADD));

            // if the record is null, then HostDB_InitEntry failed.
            if (!he || !he->rec)
                rv = NS_ERROR_OUT_OF_MEMORY;
            // do we have a cached result that we can reuse?
            else if (!(flags & RES_BYPASS_CACHE) &&
                     he->rec->HasResult() &&
                     NowInMinutes() <= he->rec->expiration) {
                LOG(("using cached record\n"));
                // put reference to host record on stack...
                result = he->rec;
                if (he->rec->negative) {
                    status = NS_ERROR_UNKNOWN_HOST;
                    if (!he->rec->resolving) 
                        // return the cached failure to the caller, but try and refresh
                        // the record in the background
                        IssueLookup(he->rec);
                }
            }
            // if the host name is an IP address literal and has been parsed,
            // go ahead and use it.
            else if (he->rec->addr) {
                result = he->rec;
            }
            // try parsing the host name as an IP address literal to short
            // circuit full host resolution.  (this is necessary on some
            // platforms like Win9x.  see bug 219376 for more details.)
            else if (PR_StringToNetAddr(host, &tempAddr) == PR_SUCCESS) {
                // ok, just copy the result into the host record, and be done
                // with it! ;-)
                he->rec->addr = (PRNetAddr *) malloc(sizeof(PRNetAddr));
                if (!he->rec->addr)
                    status = NS_ERROR_OUT_OF_MEMORY;
                else
                    memcpy(he->rec->addr, &tempAddr, sizeof(PRNetAddr));
                // put reference to host record on stack...
                result = he->rec;
            }
            else if (mPendingCount >= MAX_NON_PRIORITY_REQUESTS &&
                     !IsHighPriority(flags) &&
                     !he->rec->resolving) {
                // This is a lower priority request and we are swamped, so refuse it.
                rv = NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
            }
            // otherwise, hit the resolver...
            else {
                // Add callback to the list of pending callbacks.
                PR_APPEND_LINK(callback, &he->rec->callbacks);

                if (!he->rec->resolving) {
                    he->rec->flags = flags;
                    rv = IssueLookup(he->rec);
                    if (NS_FAILED(rv))
                        PR_REMOVE_AND_INIT_LINK(callback);
                }
                else if (he->rec->onQueue) {
                    // Consider the case where we are on a pending queue of 
                    // lower priority than the request is being made at.
                    // In that case we should upgrade to the higher queue.

                    if (IsHighPriority(flags) && !IsHighPriority(he->rec->flags)) {
                        // Move from (low|med) to high.
                        MoveQueue(he->rec, mHighQ);
                        he->rec->flags = flags;
                        ConditionallyCreateThread(he->rec);
                    } else if (IsMediumPriority(flags) && IsLowPriority(he->rec->flags)) {
                        // Move from low to med.
                        MoveQueue(he->rec, mMediumQ);
                        he->rec->flags = flags;
                    }
                }
            }
        }
    }
    if (result)
        callback->OnLookupComplete(this, result, status);
    return rv;
}

void
nsHostResolver::DetachCallback(const char            *host,
                               PRUint16               flags,
                               PRUint16               af,
                               nsResolveHostCallback *callback,
                               nsresult               status)
{
    nsRefPtr<nsHostRecord> rec;
    {
        nsAutoLock lock(mLock);

        nsHostKey key = { host, flags, af };
        nsHostDBEnt *he = static_cast<nsHostDBEnt *>
                                     (PL_DHashTableOperate(&mDB, &key, PL_DHASH_LOOKUP));
        if (he && he->rec) {
            // walk list looking for |callback|... we cannot assume
            // that it will be there!
            PRCList *node = he->rec->callbacks.next;
            while (node != &he->rec->callbacks) {
                if (static_cast<nsResolveHostCallback *>(node) == callback) {
                    PR_REMOVE_LINK(callback);
                    rec = he->rec;
                    break;
                }
                node = node->next;
            }
        }
    }

    // complete callback with the given status code; this would only be done if
    // the record was in the process of being resolved.
    if (rec)
        callback->OnLookupComplete(this, rec, status);
}

nsresult
nsHostResolver::ConditionallyCreateThread(nsHostRecord *rec)
{
    if (mNumIdleThreads) {
        // wake up idle thread to process this lookup
        PR_NotifyCondVar(mIdleThreadCV);
    }
    else if ((mThreadCount < HighThreadThreshold) ||
             (IsHighPriority(rec->flags) && mThreadCount < MAX_RESOLVER_THREADS)) {
        // dispatch new worker thread
        NS_ADDREF_THIS(); // owning reference passed to thread

        struct nsHostResolverThreadInfo *info;
        
        if (mAnyPriorityThreadCount < HighThreadThreshold) {
            info = &mAnyPriorityInfo;
            mAnyPriorityThreadCount++;
        }
        else
            info = &mHighPriorityInfo;

        mThreadCount++;
        PRThread *thr = PR_CreateThread(PR_SYSTEM_THREAD,
                                        ThreadFunc,
                                        info,
                                        PR_PRIORITY_NORMAL,
                                        PR_GLOBAL_THREAD,
                                        PR_UNJOINABLE_THREAD,
                                        0);
        if (!thr) {
            mThreadCount--;
            if (info == &mAnyPriorityInfo)
                mAnyPriorityThreadCount--;
            NS_RELEASE_THIS();
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
#if defined(PR_LOGGING)
    else
      LOG(("lookup waiting for thread - %s ...\n", rec->host));
#endif
    return NS_OK;
}

nsresult
nsHostResolver::IssueLookup(nsHostRecord *rec)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(!rec->resolving, "record is already being resolved"); 

    // Add rec to one of the pending queues, possibly removing it from mEvictionQ.
    // If rec is on mEvictionQ, then we can just move the owning
    // reference over to the new active queue.
    if (rec->next == rec)
        NS_ADDREF(rec);
    else {
        PR_REMOVE_LINK(rec);
        mEvictionQSize--;
    }
    
    if (IsHighPriority(rec->flags))
        PR_APPEND_LINK(rec, &mHighQ);
    else if (IsMediumPriority(rec->flags))
        PR_APPEND_LINK(rec, &mMediumQ);
    else
        PR_APPEND_LINK(rec, &mLowQ);
    mPendingCount++;
    
    rec->resolving = PR_TRUE;
    rec->onQueue = PR_TRUE;

    rv = ConditionallyCreateThread(rec);
    
    LOG (("DNS Thread Counters: total=%d any=%d high=%d idle=%d pending=%d\n",
          mThreadCount,
          mAnyPriorityThreadCount,
          mThreadCount - mAnyPriorityThreadCount,
          mNumIdleThreads,
          mPendingCount));

    return rv;
}

void
nsHostResolver::DeQueue(PRCList &aQ, nsHostRecord **aResult)
{
    *aResult = static_cast<nsHostRecord *>(aQ.next);
    PR_REMOVE_AND_INIT_LINK(*aResult);
    mPendingCount--;
    (*aResult)->onQueue = PR_FALSE;
}

PRBool
nsHostResolver::GetHostToLookup(nsHostRecord **result, struct nsHostResolverThreadInfo *aID)
{
    nsAutoLock lock(mLock);

    PRIntervalTime start = PR_IntervalNow(), timeout;
    
    while (!mShutdown) {
        // remove next record from Q; hand over owning reference. Check high, then med, then low
        
        if (!PR_CLIST_IS_EMPTY(&mHighQ)) {
            DeQueue (mHighQ, result);
            return PR_TRUE;
        }

        if (! aID->onlyHighPriority) {
            if (!PR_CLIST_IS_EMPTY(&mMediumQ)) {
                DeQueue (mMediumQ, result);
                return PR_TRUE;
            }
            
            if (!PR_CLIST_IS_EMPTY(&mLowQ)) {
                DeQueue (mLowQ, result);
                return PR_TRUE;
            }
        }
        
        timeout = (mNumIdleThreads >= HighThreadThreshold) ? mShortIdleTimeout : mLongIdleTimeout;
        // wait for one or more of the following to occur:
        //  (1) the pending queue has a host record to process
        //  (2) the shutdown flag has been set
        //  (3) the thread has been idle for too long
        //
        // PR_WaitCondVar will return when any of these conditions is true.
        // become the idle thread and wait for a lookup
        
        mNumIdleThreads++;
        PR_WaitCondVar(mIdleThreadCV, timeout);
        mNumIdleThreads--;

        PRIntervalTime delta = PR_IntervalNow() - start;
        if (delta >= timeout)
            break;
        timeout -= delta;
        start += delta;
    }

    // tell thread to exit...
    mThreadCount--;
    if (!aID->onlyHighPriority)
        mAnyPriorityThreadCount--;
    return PR_FALSE;
}

void
nsHostResolver::OnLookupComplete(nsHostRecord *rec, nsresult status, PRAddrInfo *result)
{
    // get the list of pending callbacks for this lookup, and notify
    // them that the lookup is complete.
    PRCList cbs;
    PR_INIT_CLIST(&cbs);
    {
        nsAutoLock lock(mLock);

        // grab list of callbacks to notify
        MoveCList(rec->callbacks, cbs);

        // update record fields.  We might have a rec->addr_info already if a
        // previous lookup result expired and we're reresolving it..
        PRAddrInfo  *old_addr_info;
        PR_Lock(rec->addr_info_lock);
        old_addr_info = rec->addr_info;
        rec->addr_info = result;
        rec->addr_info_gencnt++;
        PR_Unlock(rec->addr_info_lock);
        if (old_addr_info)
            PR_FreeAddrInfo(old_addr_info);
        rec->expiration = NowInMinutes();
        if (result) {
            rec->expiration += mMaxCacheLifetime;
            rec->negative = PR_FALSE;
        }
        else {
            rec->expiration += 1;                 /* one minute for negative cache */
            rec->negative = PR_TRUE;
        }
        rec->resolving = PR_FALSE;
        
        if (rec->addr_info && !mShutdown) {
            // add to mEvictionQ
            PR_APPEND_LINK(rec, &mEvictionQ);
            NS_ADDREF(rec);
            if (mEvictionQSize < mMaxCacheEntries)
                mEvictionQSize++;
            else {
                // remove first element on mEvictionQ
                nsHostRecord *head =
                    static_cast<nsHostRecord *>(PR_LIST_HEAD(&mEvictionQ));
                PR_REMOVE_AND_INIT_LINK(head);
                PL_DHashTableOperate(&mDB, (nsHostKey *) head, PL_DHASH_REMOVE);
                // release reference to rec owned by mEvictionQ
                NS_RELEASE(head);
            }
        }
    }

    if (!PR_CLIST_IS_EMPTY(&cbs)) {
        PRCList *node = cbs.next;
        while (node != &cbs) {
            nsResolveHostCallback *callback =
                    static_cast<nsResolveHostCallback *>(node);
            node = node->next;
            callback->OnLookupComplete(this, rec, status);
            // NOTE: callback must not be dereferenced after this point!!
        }
    }

    NS_RELEASE(rec);
}

//----------------------------------------------------------------------------

void
nsHostResolver::ThreadFunc(void *arg)
{
    LOG(("nsHostResolver::ThreadFunc entering\n"));
#if defined(RES_RETRY_ON_FAILURE)
    nsResState rs;
#endif
    struct nsHostResolverThreadInfo *info = (struct nsHostResolverThreadInfo *) arg;
    nsHostResolver *resolver = info->self;
    nsHostRecord *rec;
    PRAddrInfo *ai;
    while (resolver->GetHostToLookup(&rec, info)) {
        LOG(("resolving %s ...\n", rec->host));

        PRIntn flags = PR_AI_ADDRCONFIG;
        if (!(rec->flags & RES_CANON_NAME))
            flags |= PR_AI_NOCANONNAME;

        ai = PR_GetAddrInfoByName(rec->host, rec->af, flags);
#if defined(RES_RETRY_ON_FAILURE)
        if (!ai && rs.Reset())
            ai = PR_GetAddrInfoByName(rec->host, rec->af, flags);
#endif

        // convert error code to nsresult.
        nsresult status = ai ? NS_OK : NS_ERROR_UNKNOWN_HOST;
        resolver->OnLookupComplete(rec, status, ai);
        LOG(("lookup complete for %s ...\n", rec->host));
    }
    NS_RELEASE(resolver);
    LOG(("nsHostResolver::ThreadFunc exiting\n"));
}

//----------------------------------------------------------------------------

nsresult
nsHostResolver::Create(PRUint32         maxCacheEntries,
                       PRUint32         maxCacheLifetime,
                       nsHostResolver **result)
{
#if defined(PR_LOGGING)
    if (!gHostResolverLog)
        gHostResolverLog = PR_NewLogModule("nsHostResolver");
#endif

    nsHostResolver *res = new nsHostResolver(maxCacheEntries,
                                             maxCacheLifetime);
    if (!res)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(res);

    nsresult rv = res->Init();
    if (NS_FAILED(rv))
        NS_RELEASE(res);

    *result = res;
    return rv;
}
