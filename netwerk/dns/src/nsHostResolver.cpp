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
#include "pldhash.h"
#include "plstr.h"

//----------------------------------------------------------------------------

#define MAX_THREADS 8
#define THREAD_IDLE_TIMEOUT PR_SecondsToInterval(5)

//----------------------------------------------------------------------------

//#define DEBUG_HOST_RESOLVER
#ifdef DEBUG_HOST_RESOLVER
#define LOG(args) printf args
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

class nsHostRecord : public PRCList
{
public:
    NET_DECL_REFCOUNTED_THREADSAFE

    nsHostRecord(const char *h)
        : host(PL_strdup(h))
        , expireTime(NowInMinutes())
        , mRefCount(0)
    {
        PR_INIT_CLIST(this);
        PR_INIT_CLIST(&callbacks);
    }
   ~nsHostRecord()
    {
        PL_strfree(host);
    }

    char                 *host;
    nsRefPtr<nsAddrInfo>  addr;
    PRUint32              expireTime; // minutes since epoch
    PRCList               callbacks;

private:
    PRInt32               mRefCount;
};

//----------------------------------------------------------------------------

struct nsHostDBEnt : PLDHashEntryHdr
{
    nsHostRecord *rec;
};

static const void * PR_CALLBACK
HostDB_GetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    nsHostDBEnt *he = NS_STATIC_CAST(nsHostDBEnt *, entry);
    return he->rec->host;
}

static PRBool PR_CALLBACK
HostDB_MatchEntry(PLDHashTable *table,
                  const PLDHashEntryHdr *entry,
                  const void *key)
{
    const nsHostDBEnt *he = NS_STATIC_CAST(const nsHostDBEnt *, entry);
    return !strcmp(he->rec->host, (const char *) key);
}

static void PR_CALLBACK
HostDB_MoveEntry(PLDHashTable *table,
                 const PLDHashEntryHdr *from,
                 PLDHashEntryHdr *to)
{
    NS_STATIC_CAST(nsHostDBEnt *, to)->rec =
            NS_STATIC_CAST(const nsHostDBEnt *, from)->rec;
}

static void PR_CALLBACK
HostDB_ClearEntry(PLDHashTable *table,
                  PLDHashEntryHdr *entry)
{
    nsHostDBEnt *he = NS_STATIC_CAST(nsHostDBEnt *, entry);
#ifdef DEBUG_HOST_RESOLVER
    if (!he->rec->addr)
        LOG(("%s: => null\n", he->rec->host));
    else {
        PRInt32 now = (PRInt32) NowInMinutes();
        PRInt32 diff = (PRInt32) he->rec->expireTime - now;
        LOG(("%s: exp=%d => %s\n",
            he->rec->host, diff,
            PR_GetCanonNameFromAddrInfo(he->rec->addr->get())));
        void *iter = nsnull;
        PRNetAddr addr;
        char buf[64];
        do {
            iter = PR_EnumerateAddrInfo(iter, he->rec->addr->get(), 0, &addr);
            PR_NetAddrToString(&addr, buf, sizeof(buf));
            LOG(("  %s\n", buf));
        } while (iter);
    }
#endif
    NS_RELEASE(he->rec);
}

static PRBool PR_CALLBACK
HostDB_InitEntry(PLDHashTable *table,
                 PLDHashEntryHdr *entry,
                 const void *key)
{
    nsHostDBEnt *he = NS_STATIC_CAST(nsHostDBEnt *, entry);
    he->rec = new nsHostRecord(NS_REINTERPRET_CAST(const char *, key));
    // addref result if initialized correctly; otherwise, leave record
    // null so caller can detect and propagate error.
    if (he->rec) {
        if (he->rec->host)
            NS_ADDREF(he->rec);
        else {
            delete he->rec;
            he->rec = nsnull;
        }
    }
    return PR_TRUE;
}

static PLDHashTableOps gHostDB_ops =
{
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    HostDB_GetKey,
    PL_DHashStringKey,
    HostDB_MatchEntry,
    HostDB_MoveEntry,
    HostDB_ClearEntry,
    PL_DHashFinalizeStub,
    HostDB_InitEntry,
};

static PLDHashOperator PR_CALLBACK
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
    : mRefCount(0)
    , mMaxCacheEntries(maxCacheEntries)
    , mMaxCacheLifetime(maxCacheLifetime)
    , mLock(nsnull)
    , mIdleThreadCV(nsnull)
    , mHaveIdleThread(PR_FALSE)
    , mThreadCount(0)
    , mEvictionQSize(0)
    , mShutdown(PR_TRUE)
{
    mCreationTime = PR_Now();
    PR_INIT_CLIST(&mPendingQ);
    PR_INIT_CLIST(&mEvictionQ);
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
    return NS_OK;
}

void
nsHostResolver::Shutdown()
{
    LOG(("nsHostResolver::Shutdown\n"));

    PRCList pendingQ;
    PR_INIT_CLIST(&pendingQ);
    {
        nsAutoLock lock(mLock);
        
        mShutdown = PR_TRUE;

        MoveCList(mPendingQ, pendingQ);

        if (mHaveIdleThread)
            PR_NotifyCondVar(mIdleThreadCV);
        
        // empty host database
        PL_DHashTableEnumerate(&mDB, HostDB_RemoveEntry, nsnull);
    }

    // loop through pending queue, erroring out pending lookups.
    if (!PR_CLIST_IS_EMPTY(&pendingQ)) {
        PRCList *node = pendingQ.next;
        while (node != &pendingQ) {
            nsHostRecord *rec = NS_STATIC_CAST(nsHostRecord *, node);
            node = node->next;
            OnLookupComplete(rec, NS_ERROR_ABORT, nsnull);
        }
    }
}

nsresult
nsHostResolver::ResolveHost(const char      *host,
                            PRBool           bypassCache,
                            nsResolveHostCB *callback)
{
    NS_ENSURE_TRUE(host && *host, NS_ERROR_UNEXPECTED);

    nsAutoLock lock(mLock);

    if (mShutdown)
        return NS_ERROR_NOT_INITIALIZED;
    
    // check to see if there is already an entry for this |host|
    // in the hash table.  if so, then check to see if we can't
    // just reuse the lookup result.  otherwise, if there are
    // any pending callbacks, then add to pending callbacks queue,
    // and return.  otherwise, add ourselves as first pending
    // callback, and proceed to do the lookup.

    PLDHashEntryHdr *hdr;

    hdr = PL_DHashTableOperate(&mDB, host, PL_DHASH_ADD);
    if (!hdr)
        return NS_ERROR_OUT_OF_MEMORY;

    nsHostDBEnt *he = NS_STATIC_CAST(nsHostDBEnt *, hdr);
    if (!he->rec)
        return NS_ERROR_OUT_OF_MEMORY;
    else if (!bypassCache &&
             he->rec->addr && NowInMinutes() <= he->rec->expireTime) {
        // ok, we can reuse this result.  but, since we are
        // making a callback, we must only do so outside the
        // lock, and that requires holding an owning reference
        // to the addrinfo structure.
        nsRefPtr<nsAddrInfo> ai = he->rec->addr;
        lock.unlock();
        callback->OnLookupComplete(this, host, NS_OK, ai);
        lock.lock();
        return NS_OK;
    }

    PRBool doLookup = PR_CLIST_IS_EMPTY(&he->rec->callbacks);

    // add callback to the list of pending callbacks
    PR_APPEND_LINK(callback, &he->rec->callbacks);

    nsresult rv = NS_OK;
    if (doLookup) {
        rv = IssueLookup(he->rec);
        if (NS_FAILED(rv))
            PR_REMOVE_AND_INIT_LINK(callback);
    }
    return rv;
}

void
nsHostResolver::DetachCallback(const char      *host,
                               nsResolveHostCB *callback)
{
    PRBool doCallback = PR_FALSE;
    {
        nsAutoLock lock(mLock);

        nsHostDBEnt *he = NS_STATIC_CAST(nsHostDBEnt *,
                PL_DHashTableOperate(&mDB, host, PL_DHASH_LOOKUP));
        if (he && he->rec) {
            // walk list looking for |callback|... we cannot assume
            // that it will be there!
            PRCList *node = he->rec->callbacks.next;
            while (node != &he->rec->callbacks) {
                if (NS_STATIC_CAST(nsResolveHostCB *, node) == callback) {
                    PR_REMOVE_LINK(callback);
                    doCallback = PR_TRUE;
                    break;
                }
                node = node->next;
            }
        }
    }

    if (doCallback)
        callback->OnLookupComplete(this, host, NS_ERROR_ABORT, nsnull);
}

nsresult
nsHostResolver::IssueLookup(nsHostRecord *rec)
{
    // add rec to mPendingQ, possibly removing it from mEvictionQ.
    // if rec is on mEvictionQ, then we can just move the owning
    // reference over to mPendingQ.
    if (rec->next == rec)
        NS_ADDREF(rec);
    else {
        PR_REMOVE_LINK(rec);
        mEvictionQSize--;
    }
    PR_APPEND_LINK(rec, &mPendingQ);

    if (mHaveIdleThread) {
        // wake up idle thread to process this lookup
        PR_NotifyCondVar(mIdleThreadCV);
    }
    else if (mThreadCount < MAX_THREADS) {
        // dispatch new worker thread
        NS_ADDREF_THIS(); // owning reference passed to thread
        mThreadCount++;
        PRThread *thr = PR_CreateThread(PR_SYSTEM_THREAD,
                                        ThreadFunc,
                                        this,
                                        PR_PRIORITY_NORMAL,
                                        PR_GLOBAL_THREAD,
                                        PR_UNJOINABLE_THREAD,
                                        0);
        if (!thr) {
            mThreadCount--;
            NS_RELEASE_THIS();
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return NS_OK;
}

PRBool
nsHostResolver::GetHostToLookup(nsHostRecord **result)
{
    nsAutoLock lock(mLock);

    while (PR_CLIST_IS_EMPTY(&mPendingQ) && !mHaveIdleThread && !mShutdown) {
        // become the idle thread and wait for a lookup
        mHaveIdleThread = PR_TRUE;
        PR_WaitCondVar(mIdleThreadCV, THREAD_IDLE_TIMEOUT);
        mHaveIdleThread = PR_FALSE;
    }

    if (!PR_CLIST_IS_EMPTY(&mPendingQ)) {
        // remove next record from mPendingQ; hand over owning reference.
        *result = NS_STATIC_CAST(nsHostRecord *, mPendingQ.next);
        PR_REMOVE_AND_INIT_LINK(*result);
        return PR_TRUE;
    }

    // tell thread to exit...
    mThreadCount--;
    return PR_FALSE;
}

void
nsHostResolver::OnLookupComplete(nsHostRecord *rec, nsresult status, PRAddrInfo *result)
{
    nsAddrInfo *ai;
    if (!result)
        ai = nsnull;
    else {
        ai = new nsAddrInfo(result);
        if (!ai) {
            status = NS_ERROR_OUT_OF_MEMORY;
            PR_FreeAddrInfo(result);
        }
    }

    // get the list of pending callbacks for this lookup, and notify
    // them that the lookup is complete.
    PRCList cbs;
    PR_INIT_CLIST(&cbs);
    {
        nsAutoLock lock(mLock);

        MoveCList(rec->callbacks, cbs);
        rec->addr = ai;
        rec->expireTime = NowInMinutes() + mMaxCacheLifetime;
        
        if (rec->addr) {
            // add to mEvictionQ
            PR_APPEND_LINK(rec, &mEvictionQ);
            NS_ADDREF(rec);
            if (mEvictionQSize < mMaxCacheEntries)
                mEvictionQSize++;
            else {
                // remove last element on mEvictionQ
                nsHostRecord *tail =
                    NS_STATIC_CAST(nsHostRecord *, PR_LIST_TAIL(&mEvictionQ));
                PR_REMOVE_AND_INIT_LINK(tail);
                PL_DHashTableOperate(&mDB, tail->host, PL_DHASH_REMOVE);
                // release reference to rec owned by mEvictionQ
                NS_RELEASE(tail);
            }
        }
    }

    if (!PR_CLIST_IS_EMPTY(&cbs)) {
        PRCList *node = cbs.next;
        while (node != &cbs) {
            nsResolveHostCB *callback = NS_STATIC_CAST(nsResolveHostCB *, node);
            node = node->next;
            callback->OnLookupComplete(this, rec->host, status, ai);
            // NOTE: callback must not be dereferenced after this point!!
        }
    }

    NS_RELEASE(rec);
}

//----------------------------------------------------------------------------

void PR_CALLBACK
nsHostResolver::ThreadFunc(void *arg)
{
#if defined(RES_RETRY_ON_FAILURE)
    nsResState rs;
#endif

    nsHostResolver *resolver = (nsHostResolver *) arg;
    nsHostRecord *rec;
    PRAddrInfo *ai;
    while (resolver->GetHostToLookup(&rec)) {
        LOG(("[%p] resolving %s ...\n", (void*)PR_GetCurrentThread(), rec->host));
        ai = PR_GetAddrInfoByName(rec->host, PR_AF_UNSPEC, PR_AI_ADDRCONFIG);
#if defined(RES_RETRY_ON_FAILURE)
        if (!ai && rs.Reset())
            ai = PR_GetAddrInfoByName(rec->host, PR_AF_UNSPEC, PR_AI_ADDRCONFIG);
#endif
        // convert error code to nsresult.
        nsresult status = ai ? NS_OK : NS_ERROR_UNKNOWN_HOST;
        resolver->OnLookupComplete(rec, status, ai);
    }
    NS_RELEASE(resolver);
}

//----------------------------------------------------------------------------

nsresult
nsHostResolver::Create(PRUint32         maxCacheEntries,
                       PRUint32         maxCacheLifetime,
                       nsHostResolver **result)
{
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
