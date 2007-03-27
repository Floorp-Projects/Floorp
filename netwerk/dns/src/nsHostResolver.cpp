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

#define MAX_THREADS 8
#define IDLE_TIMEOUT PR_SecondsToInterval(60)

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
    size_t hostLen = strlen(key->host) + 1;
    size_t size = hostLen + sizeof(nsHostRecord);

    nsHostRecord *rec = (nsHostRecord*) ::operator new(size);
    if (!rec)
        return NS_ERROR_OUT_OF_MEMORY;

    rec->host = ((char *) rec) + sizeof(nsHostRecord);
    rec->flags = RES_KEY_FLAGS(key->flags);
    rec->af = key->af;

    rec->_refc = 1; // addref
    NS_LOG_ADDREF(rec, 1, "nsHostRecord", sizeof(nsHostRecord));
    rec->addr_info = nsnull;
    rec->addr = nsnull;
    rec->expiration = NowInMinutes();
    rec->resolving = PR_FALSE;
    PR_INIT_CLIST(rec);
    PR_INIT_CLIST(&rec->callbacks);
    memcpy((char *) rec->host, key->host, hostLen);

    *result = rec;
    return NS_OK;
}

nsHostRecord::~nsHostRecord()
{
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

PR_STATIC_CALLBACK(PLDHashNumber)
HostDB_HashKey(PLDHashTable *table, const void *key)
{
    const nsHostKey *hk = NS_STATIC_CAST(const nsHostKey *, key);
    return PL_DHashStringKey(table, hk->host) ^ hk->flags ^ hk->af;
}

PR_STATIC_CALLBACK(PRBool)
HostDB_MatchEntry(PLDHashTable *table,
                  const PLDHashEntryHdr *entry,
                  const void *key)
{
    const nsHostDBEnt *he = NS_STATIC_CAST(const nsHostDBEnt *, entry);
    const nsHostKey *hk = NS_STATIC_CAST(const nsHostKey *, key); 

    return !strcmp(he->rec->host, hk->host) &&
            he->rec->flags == hk->flags &&
            he->rec->af == hk->af;
}

PR_STATIC_CALLBACK(void)
HostDB_MoveEntry(PLDHashTable *table,
                 const PLDHashEntryHdr *from,
                 PLDHashEntryHdr *to)
{
    NS_STATIC_CAST(nsHostDBEnt *, to)->rec =
            NS_STATIC_CAST(const nsHostDBEnt *, from)->rec;
}

PR_STATIC_CALLBACK(void)
HostDB_ClearEntry(PLDHashTable *table,
                  PLDHashEntryHdr *entry)
{
    LOG(("evicting record\n"));
    nsHostDBEnt *he = NS_STATIC_CAST(nsHostDBEnt *, entry);
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

PR_STATIC_CALLBACK(PRBool)
HostDB_InitEntry(PLDHashTable *table,
                 PLDHashEntryHdr *entry,
                 const void *key)
{
    nsHostDBEnt *he = NS_STATIC_CAST(nsHostDBEnt *, entry);
    nsHostRecord::Create(NS_STATIC_CAST(const nsHostKey *, key), &he->rec);
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

PR_STATIC_CALLBACK(PLDHashOperator)
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
nsHostResolver::Shutdown()
{
    LOG(("nsHostResolver::Shutdown\n"));

    PRCList pendingQ, evictionQ;
    PR_INIT_CLIST(&pendingQ);
    PR_INIT_CLIST(&evictionQ);

    {
        nsAutoLock lock(mLock);
        
        mShutdown = PR_TRUE;

        MoveCList(mPendingQ, pendingQ);
        MoveCList(mEvictionQ, evictionQ);
        mEvictionQSize = 0;

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

    if (!PR_CLIST_IS_EMPTY(&evictionQ)) {
        PRCList *node = evictionQ.next;
        while (node != &evictionQ) {
            nsHostRecord *rec = NS_STATIC_CAST(nsHostRecord *, node);
            node = node->next;
            NS_RELEASE(rec);
        }
    }

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
            nsHostDBEnt *he = NS_STATIC_CAST(nsHostDBEnt *,
                    PL_DHashTableOperate(&mDB, &key, PL_DHASH_ADD));

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
            // otherwise, hit the resolver...
            else {
                // add callback to the list of pending callbacks
                PR_APPEND_LINK(callback, &he->rec->callbacks);

                if (!he->rec->resolving) {
                    rv = IssueLookup(he->rec);
                    if (NS_FAILED(rv))
                        PR_REMOVE_AND_INIT_LINK(callback);
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
        nsHostDBEnt *he = NS_STATIC_CAST(nsHostDBEnt *,
                PL_DHashTableOperate(&mDB, &key, PL_DHASH_LOOKUP));
        if (he && he->rec) {
            // walk list looking for |callback|... we cannot assume
            // that it will be there!
            PRCList *node = he->rec->callbacks.next;
            while (node != &he->rec->callbacks) {
                if (NS_STATIC_CAST(nsResolveHostCallback *, node) == callback) {
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
nsHostResolver::IssueLookup(nsHostRecord *rec)
{
    NS_ASSERTION(!rec->resolving, "record is already being resolved"); 

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
    rec->resolving = PR_TRUE;

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
#if defined(PR_LOGGING)
    else
      LOG(("lookup waiting for thread - %s ...\n", rec->host));
#endif

    return NS_OK;
}

PRBool
nsHostResolver::GetHostToLookup(nsHostRecord **result)
{
    nsAutoLock lock(mLock);

    PRIntervalTime start = PR_IntervalNow(), timeout = IDLE_TIMEOUT;
    //
    // wait for one or more of the following to occur:
    //  (1) the pending queue has a host record to process
    //  (2) the shutdown flag has been set
    //  (3) the thread has been idle for too long
    //
    // PR_WaitCondVar will return when any of these conditions is true.
    //
    while (PR_CLIST_IS_EMPTY(&mPendingQ) && !mHaveIdleThread && !mShutdown) {
        // become the idle thread and wait for a lookup
        mHaveIdleThread = PR_TRUE;
        PR_WaitCondVar(mIdleThreadCV, timeout);
        mHaveIdleThread = PR_FALSE;

        PRIntervalTime delta = PR_IntervalNow() - start;
        if (delta >= timeout)
            break;
        timeout -= delta;
        start += delta;
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
    // get the list of pending callbacks for this lookup, and notify
    // them that the lookup is complete.
    PRCList cbs;
    PR_INIT_CLIST(&cbs);
    {
        nsAutoLock lock(mLock);

        // grab list of callbacks to notify
        MoveCList(rec->callbacks, cbs);

        // update record fields
        rec->addr_info = result;
        rec->expiration = NowInMinutes() + mMaxCacheLifetime;
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
                    NS_STATIC_CAST(nsHostRecord *, PR_LIST_HEAD(&mEvictionQ));
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
                    NS_STATIC_CAST(nsResolveHostCallback *, node);
            node = node->next;
            callback->OnLookupComplete(this, rec, status);
            // NOTE: callback must not be dereferenced after this point!!
        }
    }

    NS_RELEASE(rec);
}

//----------------------------------------------------------------------------

void PR_CALLBACK
nsHostResolver::ThreadFunc(void *arg)
{
    LOG(("nsHostResolver::ThreadFunc entering\n"));
#if defined(RES_RETRY_ON_FAILURE)
    nsResState rs;
#endif

    nsHostResolver *resolver = (nsHostResolver *) arg;
    nsHostRecord *rec;
    PRAddrInfo *ai;
    while (resolver->GetHostToLookup(&rec)) {
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
