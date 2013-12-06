/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsError.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"
#include "nsAutoPtr.h"
#include "prthread.h"
#include "prerror.h"
#include "prtime.h"
#include "prlog.h"
#include "pldhash.h"
#include "plstr.h"
#include "nsURLHelper.h"
#include "nsThreadUtils.h"

#include "mozilla/HashFunctions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/VisualEventTracer.h"
#if defined(XP_WIN)
// See bug 942317 in case you're all "WTF, mate?!"
#include "mozilla/Preferences.h"
#endif

using namespace mozilla;
using namespace mozilla::net;

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

#define HighThreadThreshold     MAX_RESOLVER_THREADS_FOR_ANY_PRIORITY
#define LongIdleTimeoutSeconds  300           // for threads 1 -> HighThreadThreshold
#define ShortIdleTimeoutSeconds 60            // for threads HighThreadThreshold+1 -> MAX_RESOLVER_THREADS

PR_STATIC_ASSERT (HighThreadThreshold <= MAX_RESOLVER_THREADS);

//----------------------------------------------------------------------------

#if defined(PR_LOGGING)
static PRLogModuleInfo *gHostResolverLog = nullptr;
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

    bool Reset()
    {
        // reset no more than once per second
        if (PR_IntervalToSeconds(PR_IntervalNow() - mLastReset) < 1)
            return false;

        LOG(("Calling 'res_ninit'.\n"));

        mLastReset = PR_IntervalNow();
        return (res_ninit(&_res) == 0);
    }

private:
    PRIntervalTime mLastReset;
};

#endif // RES_RETRY_ON_FAILURE

//----------------------------------------------------------------------------

static inline bool
IsHighPriority(uint16_t flags)
{
    return !(flags & (nsHostResolver::RES_PRIORITY_LOW | nsHostResolver::RES_PRIORITY_MEDIUM));
}

static inline bool
IsMediumPriority(uint16_t flags)
{
    return flags & nsHostResolver::RES_PRIORITY_MEDIUM;
}

static inline bool
IsLowPriority(uint16_t flags)
{
    return flags & nsHostResolver::RES_PRIORITY_LOW;
}

//----------------------------------------------------------------------------

// this macro filters out any flags that are not used when constructing the
// host key.  the significant flags are those that would affect the resulting
// host record (i.e., the flags that are passed down to PR_GetAddrInfoByName).
#define RES_KEY_FLAGS(_f) ((_f) & nsHostResolver::RES_CANON_NAME)

nsHostRecord::nsHostRecord(const nsHostKey *key)
    : addr_info_lock("nsHostRecord.addr_info_lock")
    , addr_info_gencnt(0)
    , addr_info(nullptr)
    , addr(nullptr)
    , negative(false)
    , resolving(false)
    , onQueue(false)
    , usingAnyThread(false)
    , mDoomed(false)
{
    host = ((char *) this) + sizeof(nsHostRecord);
    memcpy((char *) host, key->host, strlen(key->host) + 1);
    flags = key->flags;
    af = key->af;

    expiration = TimeStamp::NowLoRes();

    PR_INIT_CLIST(this);
    PR_INIT_CLIST(&callbacks);
}

nsresult
nsHostRecord::Create(const nsHostKey *key, nsHostRecord **result)
{
    size_t hostLen = strlen(key->host) + 1;
    size_t size = hostLen + sizeof(nsHostRecord);

    // Use placement new to create the object with room for the hostname
    // allocated after it.
    void *place = ::operator new(size);
    *result = new(place) nsHostRecord(key);
    NS_ADDREF(*result);

    MOZ_EVENT_TRACER_NAME_OBJECT(*result, key->host);

    return NS_OK;
}

nsHostRecord::~nsHostRecord()
{
    delete addr_info;
    delete addr;
}

bool
nsHostRecord::Blacklisted(NetAddr *aQuery)
{
    // must call locked
    LOG(("Checking blacklist for host [%s], host record [%p].\n", host, this));

    // skip the string conversion for the common case of no blacklist
    if (!mBlacklistedItems.Length()) {
        return false;
    }

    char buf[kIPv6CStrBufSize];
    if (!NetAddrToString(aQuery, buf, sizeof(buf))) {
        return false;
    }
    nsDependentCString strQuery(buf);

    for (uint32_t i = 0; i < mBlacklistedItems.Length(); i++) {
        if (mBlacklistedItems.ElementAt(i).Equals(strQuery)) {
            LOG(("Address [%s] is blacklisted for host [%s].\n", buf, host));
            return true;
        }
    }

    return false;
}

void
nsHostRecord::ReportUnusable(NetAddr *aAddress)
{
    // must call locked
    LOG(("Adding address to blacklist for host [%s], host record [%p].\n", host, this));

    if (negative)
        mDoomed = true;

    char buf[kIPv6CStrBufSize];
    if (NetAddrToString(aAddress, buf, sizeof(buf))) {
        LOG(("Successfully adding address [%s] to blacklist for host [%s].\n", buf, host));
        mBlacklistedItems.AppendElement(nsCString(buf));
    }
}

void
nsHostRecord::ResetBlacklist()
{
    // must call locked
    LOG(("Resetting blacklist for host [%s], host record [%p].\n", host, this));
    mBlacklistedItems.Clear();
}

bool
nsHostRecord::HasUsableResult(uint16_t queryFlags) const
{
    if (mDoomed)
        return false;

    // don't use cached negative results for high priority queries.
    if (negative && IsHighPriority(queryFlags))
        return false;

    return addr_info || addr || negative;
}

static size_t
SizeOfResolveHostCallbackListExcludingHead(const PRCList *head,
                                           MallocSizeOf mallocSizeOf)
{
    size_t n = 0;
    PRCList *curr = head->next;
    while (curr != head) {
        nsResolveHostCallback *callback =
            static_cast<nsResolveHostCallback*>(curr);
        n += callback->SizeOfIncludingThis(mallocSizeOf);
        curr = curr->next;
    }
    return n;
}

size_t
nsHostRecord::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const
{
    size_t n = mallocSizeOf(this);

    // The |host| field (inherited from nsHostKey) actually points to extra
    // memory that is allocated beyond the end of the nsHostRecord (see
    // nsHostRecord::Create()).  So it will be included in the
    // |mallocSizeOf(this)| call above.

    n += SizeOfResolveHostCallbackListExcludingHead(&callbacks, mallocSizeOf);
    n += addr_info ? addr_info->SizeOfIncludingThis(mallocSizeOf) : 0;
    n += mallocSizeOf(addr);

    n += mBlacklistedItems.SizeOfExcludingThis(mallocSizeOf);
    for (size_t i = 0; i < mBlacklistedItems.Length(); i++) {
        n += mBlacklistedItems[i].SizeOfIncludingThisMustBeUnshared(mallocSizeOf);
    }
    return n;
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
    return AddToHash(HashString(hk->host), RES_KEY_FLAGS(hk->flags), hk->af);
}

static bool
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
    nsHostDBEnt *he = static_cast<nsHostDBEnt*>(entry);
    MOZ_ASSERT(he, "nsHostDBEnt is null!");

    nsHostRecord *hr = he->rec;
    MOZ_ASSERT(hr, "nsHostDBEnt has null host record!");

    LOG(("Clearing cache db entry for host [%s].\n", hr->host));
#if defined(DEBUG) && defined(PR_LOGGING)
    {
        MutexAutoLock lock(hr->addr_info_lock);
        if (!hr->addr_info) {
            LOG(("No address info for host [%s].\n", hr->host));
        } else {
            TimeDuration diff = hr->expiration - TimeStamp::NowLoRes();
            LOG(("Record for [%s] expires in %f seconds.\n", hr->host, diff.ToSeconds()));

            NetAddrElement *addrElement = nullptr;
            char buf[kIPv6CStrBufSize];
            do {
                if (!addrElement) {
                    addrElement = hr->addr_info->mAddresses.getFirst();
                } else {
                    addrElement = addrElement->getNext();
                }

                if (addrElement) {
                    NetAddrToString(&addrElement->mAddress, buf, sizeof(buf));
                    LOG(("  [%s]\n", buf));
                }
            }
            while (addrElement);
        }
    }
#endif
    NS_RELEASE(he->rec);
}

static bool
HostDB_InitEntry(PLDHashTable *table,
                 PLDHashEntryHdr *entry,
                 const void *key)
{
    nsHostDBEnt *he = static_cast<nsHostDBEnt *>(entry);
    nsHostRecord::Create(static_cast<const nsHostKey *>(key), &he->rec);
    return true;
}

static const PLDHashTableOps gHostDB_ops =
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
                   uint32_t number,
                   void *arg)
{
    return PL_DHASH_REMOVE;
}

//----------------------------------------------------------------------------

nsHostResolver::nsHostResolver(uint32_t maxCacheEntries,
                               uint32_t maxCacheLifetime,
                               uint32_t lifetimeGracePeriod)
    : mMaxCacheEntries(maxCacheEntries)
    , mMaxCacheLifetime(TimeDuration::FromSeconds(maxCacheLifetime * 60))
    , mGracePeriod(lifetimeGracePeriod)
    , mLock("nsHostResolver.mLock")
    , mIdleThreadCV(mLock, "nsHostResolver.mIdleThreadCV")
    , mNumIdleThreads(0)
    , mThreadCount(0)
    , mActiveAnyThreadCount(0)
    , mEvictionQSize(0)
    , mPendingCount(0)
    , mShutdown(true)
#if defined(XP_WIN)
    // See bug 942317 in case you're all "WTF, mate?!"
    , mExperimentLock("nsHostResolver.mExperimentLock")
    , mHasRunExperiment(false)
    , mNetworkExperimentsOK(true)
    , mDnsExperimentOK(true)
#endif
{
    mCreationTime = PR_Now();
    PR_INIT_CLIST(&mHighQ);
    PR_INIT_CLIST(&mMediumQ);
    PR_INIT_CLIST(&mLowQ);
    PR_INIT_CLIST(&mEvictionQ);

    mLongIdleTimeout  = PR_SecondsToInterval(LongIdleTimeoutSeconds);
    mShortIdleTimeout = PR_SecondsToInterval(ShortIdleTimeoutSeconds);
}

nsHostResolver::~nsHostResolver()
{
    PL_DHashTableFinish(&mDB);
}

nsresult
nsHostResolver::Init()
{
    PL_DHashTableInit(&mDB, &gHostDB_ops, nullptr, sizeof(nsHostDBEnt), 0);

    mShutdown = false;

#if defined(HAVE_RES_NINIT)
    // We want to make sure the system is using the correct resolver settings,
    // so we force it to reload those settings whenever we startup a subsequent
    // nsHostResolver instance.  We assume that there is no reason to do this
    // for the first nsHostResolver instance since that is usually created
    // during application startup.
    static int initCount = 0;
    if (initCount++ > 0) {
        LOG(("Calling 'res_ninit'.\n"));
        res_ninit(&_res);
    }
#endif

#if defined(XP_WIN)
    // See bug 942317 in case you're all "WTF, mate?!"
    Preferences::AddBoolVarCache(&mNetworkExperimentsOK,
                                 "network.allow-experiments",
                                 true);
    Preferences::AddBoolVarCache(&mDnsExperimentOK,
                                 "network.dns.allow-srv-experiment",
#if defined(EARLY_BETA_OR_EARLIER)
                                 true
#else
                                 false
#endif
                                );
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
            OnLookupComplete(rec, NS_ERROR_ABORT, nullptr);
        }
    }
}

void
nsHostResolver::Shutdown()
{
    LOG(("Shutting down host resolver.\n"));

    PRCList pendingQHigh, pendingQMed, pendingQLow, evictionQ;
    PR_INIT_CLIST(&pendingQHigh);
    PR_INIT_CLIST(&pendingQMed);
    PR_INIT_CLIST(&pendingQLow);
    PR_INIT_CLIST(&evictionQ);

    {
        MutexAutoLock lock(mLock);
        
        mShutdown = true;

        MoveCList(mHighQ, pendingQHigh);
        MoveCList(mMediumQ, pendingQMed);
        MoveCList(mLowQ, pendingQLow);
        MoveCList(mEvictionQ, evictionQ);
        mEvictionQSize = 0;
        mPendingCount = 0;
        
        if (mNumIdleThreads)
            mIdleThreadCV.NotifyAll();
        
        // empty host database
        PL_DHashTableEnumerate(&mDB, HostDB_RemoveEntry, nullptr);
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

void 
nsHostResolver::MoveQueue(nsHostRecord *aRec, PRCList &aDestQ)
{
    NS_ASSERTION(aRec->onQueue, "Moving Host Record Not Currently Queued");
    
    PR_REMOVE_LINK(aRec);
    PR_APPEND_LINK(aRec, &aDestQ);
}

nsresult
nsHostResolver::ResolveHost(const char            *host,
                            uint16_t               flags,
                            uint16_t               af,
                            nsResolveHostCallback *callback)
{
    NS_ENSURE_TRUE(host && *host, NS_ERROR_UNEXPECTED);

    LOG(("Resolving host [%s]%s.\n",
         host, flags & RES_BYPASS_CACHE ? " - bypassing cache" : ""));

    // ensure that we are working with a valid hostname before proceeding.  see
    // bug 304904 for details.
    if (!net_IsValidHostName(nsDependentCString(host)))
        return NS_ERROR_UNKNOWN_HOST;

    // if result is set inside the lock, then we need to issue the
    // callback before returning.
    nsRefPtr<nsHostRecord> result;
    nsresult status = NS_OK, rv = NS_OK;
    {
        MutexAutoLock lock(mLock);

        if (mShutdown)
            rv = NS_ERROR_NOT_INITIALIZED;
        else {
            // Used to try to parse to an IP address literal.
            PRNetAddr tempAddr;
            // Unfortunately, PR_StringToNetAddr does not properly initialize
            // the output buffer in the case of IPv6 input. See bug 223145.
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
            if (!he || !he->rec) {
                LOG(("  Out of memory: no cache entry for [%s].\n", host));
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
            // do we have a cached result that we can reuse?
            else if (!(flags & RES_BYPASS_CACHE) &&
                     he->rec->HasUsableResult(flags) &&
                     TimeStamp::NowLoRes() <= (he->rec->expiration + TimeDuration::FromSeconds(mGracePeriod * 60))) {
                LOG(("  Using cached record for host [%s].\n", host));
                // put reference to host record on stack...
                result = he->rec;
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_HIT);

                // For entries that are in the grace period with a failed connect,
                // or all cached negative entries, use the cache but start a new
                // lookup in the background
                ConditionallyRefreshRecord(he->rec, host);
                
                if (he->rec->negative) {
                    LOG(("  Negative cache entry for[%s].\n", host));
                    Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                          METHOD_NEGATIVE_HIT);
                    status = NS_ERROR_UNKNOWN_HOST;
                }
            }
            // if the host name is an IP address literal and has been parsed,
            // go ahead and use it.
            else if (he->rec->addr) {
                LOG(("  Using cached address for IP Literal [%s].\n", host));
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                      METHOD_LITERAL);
                result = he->rec;
            }
            // try parsing the host name as an IP address literal to short
            // circuit full host resolution.  (this is necessary on some
            // platforms like Win9x.  see bug 219376 for more details.)
            else if (PR_StringToNetAddr(host, &tempAddr) == PR_SUCCESS) {
                LOG(("  Host is IP Literal [%s].\n", host));
                // ok, just copy the result into the host record, and be done
                // with it! ;-)
                he->rec->addr = new NetAddr();
                PRNetAddrToNetAddr(&tempAddr, he->rec->addr);
                // put reference to host record on stack...
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                      METHOD_LITERAL);
                result = he->rec;
            }
            else if (mPendingCount >= MAX_NON_PRIORITY_REQUESTS &&
                     !IsHighPriority(flags) &&
                     !he->rec->resolving) {
                LOG(("  Lookup queue full: dropping %s priority request for "
                     "[%s].\n",
                     IsMediumPriority(flags) ? "medium" : "low", host));
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                      METHOD_OVERFLOW);
                // This is a lower priority request and we are swamped, so refuse it.
                rv = NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
            }
            else if (flags & RES_OFFLINE) {
                LOG(("  Offline request for [%s]; ignoring.\n", host));
                rv = NS_ERROR_OFFLINE;
            }

            // If this is an IPV4 or IPV6 specific request, check if there is
            // an AF_UNSPEC entry we can use. Otherwise, hit the resolver...
            else if (!he->rec->resolving) {
                if (!(flags & RES_BYPASS_CACHE) &&
                    ((af == PR_AF_INET) || (af == PR_AF_INET6))) {
                    // First, search for an entry with AF_UNSPEC
                    const nsHostKey unspecKey = { host, flags, PR_AF_UNSPEC };
                    nsHostDBEnt *unspecHe = static_cast<nsHostDBEnt *>
                        (PL_DHashTableOperate(&mDB, &unspecKey, PL_DHASH_LOOKUP));
                    NS_ASSERTION(PL_DHASH_ENTRY_IS_FREE(unspecHe) ||
                                 (PL_DHASH_ENTRY_IS_BUSY(unspecHe) &&
                                  unspecHe->rec),
                                "Valid host entries should contain a record");
                    if (PL_DHASH_ENTRY_IS_BUSY(unspecHe) &&
                        unspecHe->rec &&
                        unspecHe->rec->HasUsableResult(flags) &&
                        TimeStamp::NowLoRes() <= (he->rec->expiration +
                            TimeDuration::FromSeconds(mGracePeriod * 60))) {

                        MOZ_ASSERT(unspecHe->rec->addr_info || unspecHe->rec->negative,
                                   "Entry should be resolved or negative.");

                        LOG(("  Trying AF_UNSPEC entry for [%s] af: %s.\n",
                            host, (af == PR_AF_INET) ? "AF_INET" : "AF_INET6"));

                        he->rec->addr_info = nullptr;
                        if (unspecHe->rec->negative) {
                            he->rec->negative = unspecHe->rec->negative;
                        } else if (unspecHe->rec->addr_info) {
                            // Search for any valid address in the AF_UNSPEC entry
                            // in the cache (not blacklisted and from the right
                            // family).
                            NetAddrElement *addrIter =
                                unspecHe->rec->addr_info->mAddresses.getFirst();
                            while (addrIter) {
                                if ((af == addrIter->mAddress.inet.family) &&
                                     !unspecHe->rec->Blacklisted(&addrIter->mAddress)) {
                                    if (!he->rec->addr_info) {
                                        he->rec->addr_info = new AddrInfo(
                                            unspecHe->rec->addr_info->mHostName,
                                            unspecHe->rec->addr_info->mCanonicalName);
                                    }
                                    he->rec->addr_info->AddAddress(
                                        new NetAddrElement(*addrIter));
                                }
                                addrIter = addrIter->getNext();
                            }
                        }
                        if (he->rec->HasUsableResult(flags)) {
                            result = he->rec;
                            Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                                  METHOD_HIT);
                            ConditionallyRefreshRecord(he->rec, host);
                        }
                        // For AF_INET6, a new lookup means another AF_UNSPEC
                        // lookup. We have already iterated through the
                        // AF_UNSPEC addresses, so we mark this record as
                        // negative.
                        else if (af == PR_AF_INET6) {
                            LOG(("  No AF_INET6 in AF_UNSPEC entry: "
                                 "[%s] unknown host", host));
                            result = he->rec;
                            he->rec->negative = true;
                            status = NS_ERROR_UNKNOWN_HOST;
                            Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                                  METHOD_NEGATIVE_HIT);
                        }
                    }
                }
                // If no valid address was found in the cache or this is an
                // AF_UNSPEC request, then start a new lookup.
                if (!result) {
                    LOG(("  No usable address in cache for [%s]", host));
                    // Add callback to the list of pending callbacks.
                    PR_APPEND_LINK(callback, &he->rec->callbacks);
                    he->rec->flags = flags;
                    rv = IssueLookup(he->rec);
                    Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                          METHOD_NETWORK_FIRST);
                    if (NS_FAILED(rv)) {
                        PR_REMOVE_AND_INIT_LINK(callback);
                    }
                    else {
                        LOG(("  DNS lookup for host [%s] blocking pending "
                             "'getaddrinfo' query: callback [%p]",
                             host, callback));
                    }
                }
            }
            else {
                LOG(("  Host [%s] is being resolved. Appending callback [%p].",
                     host, callback));
                PR_APPEND_LINK(callback, &he->rec->callbacks);
                if (he->rec->onQueue) {
                    Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                          METHOD_NETWORK_SHARED);

                    // Consider the case where we are on a pending queue of
                    // lower priority than the request is being made at.
                    // In that case we should upgrade to the higher queue.

                    if (IsHighPriority(flags) &&
                        !IsHighPriority(he->rec->flags)) {
                        // Move from (low|med) to high.
                        MoveQueue(he->rec, mHighQ);
                        he->rec->flags = flags;
                        ConditionallyCreateThread(he->rec);
                    } else if (IsMediumPriority(flags) &&
                               IsLowPriority(he->rec->flags)) {
                        // Move from low to med.
                        MoveQueue(he->rec, mMediumQ);
                        he->rec->flags = flags;
                        mIdleThreadCV.Notify();
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
                               uint16_t               flags,
                               uint16_t               af,
                               nsResolveHostCallback *callback,
                               nsresult               status)
{
    nsRefPtr<nsHostRecord> rec;
    {
        MutexAutoLock lock(mLock);

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
        mIdleThreadCV.Notify();
    }
    else if ((mThreadCount < HighThreadThreshold) ||
             (IsHighPriority(rec->flags) && mThreadCount < MAX_RESOLVER_THREADS)) {
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
      LOG(("  Unable to find a thread for looking up host [%s].\n", rec->host));
#endif
    return NS_OK;
}

#if defined(XP_WIN)
// See bug 942317 in case you're all "WTF, mate?!"
#include "nsID.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "prlink.h"
#include "Windns.h"
#include "Windows.h"

typedef DNS_STATUS (__stdcall * DnsQueryFunc) (PCTSTR lpstrName, WORD wType,
                                               DWORD Options, PVOID pExtra,
                                               PDNS_RECORD *ppQueryResultsSet,
                                               PVOID *pReserved);

class ExperimentFinishedRunner : public nsRunnable
{
public:
    ExperimentFinishedRunner(nsIThread *thread)
        :mThread(thread)
    { }

    ~ExperimentFinishedRunner()
    { }

    NS_IMETHOD Run() MOZ_OVERRIDE
    {
        mThread->Shutdown();
        return NS_OK;
    }

private:
    nsCOMPtr<nsIThread> mThread;
};

class ExperimentResolver : public nsRunnable
{
public:
    ExperimentResolver(WORD queryType, nsACString &uuid, HANDLE *event,
                       TimeStamp *start, TimeStamp *end, DNS_STATUS *status,
                       DNS_RECORDA **results, DnsQueryFunc dnsQuery)
        :mQueryType(queryType)
        ,mUUID(uuid)
        ,mEvent(event)
        ,mStart(start)
        ,mEnd(end)
        ,mStatus(status)
        ,mResults(results)
        ,mDnsQuery(dnsQuery)
    { }

    ~ExperimentResolver()
    { }

    NS_IMETHOD Run() MOZ_OVERRIDE
    {
        nsAutoCString name;
        if (mQueryType == DNS_TYPE_SRV) {
            name.Assign(NS_LITERAL_CSTRING("_http2tls.srv-"));
        } else {
            name.Assign(NS_LITERAL_CSTRING("a-"));
        }
        name.Append(mUUID);
        name.Append(NS_LITERAL_CSTRING(".http2test.mozilla.org"));

        *mStart = mozilla::TimeStamp::Now();
        *mStatus = mDnsQuery(name.get(), mQueryType, DNS_QUERY_STANDARD,
                             nullptr, mResults, nullptr);
        *mEnd = mozilla::TimeStamp::Now();

        SetEvent(*mEvent);

        return NS_OK;
    }

private:
    WORD mQueryType;
    nsAutoCString mUUID;
    HANDLE *mEvent;
    TimeStamp *mStart;
    TimeStamp *mEnd;
    DNS_STATUS *mStatus;
    DNS_RECORDA **mResults;
    DnsQueryFunc mDnsQuery;
};

class ExperimentRunner : public nsRunnable
{
public:
    ExperimentRunner(nsIThread *experimentThread, nsIThread *resolveAThread,
                     nsIThread *resolveSRVThread)
        :mExperimentThread(experimentThread)
        ,mResolveAThread(resolveAThread)
        ,mResolveSRVThread(resolveSRVThread)
    { }

    ~ExperimentRunner()
    { }

    NS_IMETHOD Run() MOZ_OVERRIDE
    {
        // These are all declared here because compiler warnings about goto.
        DnsQueryFunc dnsQuery;
        TimeStamp startALookup, endALookup, startSRVLookup, endSRVLookup;
        DNS_RECORDA *aResults, *srvResults;
        DNS_STATUS aStatus, srvStatus;
        int32_t experimentStatus;
        Telemetry::ID deltaKey;
        TimeDuration duration;
        double delta;
        uint32_t timeDelta;
        nsresult rv;
        nsID id;
        char uuid[NSID_LENGTH];
        nsAutoCString dnsUUID;
        nsCOMPtr<nsIUUIDGenerator> uuidgen;
        HANDLE events[2];
        bool correctA = true, correctSRV = true;

        PRLibrary *lib = PR_LoadLibrary("Dnsapi.dll");
        if (!lib) {
            goto out;
        }

        dnsQuery = (DnsQueryFunc) PR_FindFunctionSymbol(lib, "DnsQuery_A");
        if (!dnsQuery) {
            goto library_cleanup;
        }

        // Generate hostnames that are subhosts of aus.mozilla.org, as we want
        // to make sure that whatever hostname we lookup will not be cached
        // anywhere. Use separate names for the A and SRV lookups to ensure that
        // Windows doesn't do any odd caching of NXDOMAIN in case the A lookup
        // fails.
        uuidgen = do_GetService("@mozilla.org/uuid-generator;1", &rv);
        if (NS_FAILED(rv)) {
            goto library_cleanup;
        }
        rv = uuidgen->GenerateUUIDInPlace(&id);
        NS_ENSURE_SUCCESS(rv, NS_OK);
        id.ToProvidedString(uuid);
        // Strip off the { and } surrounding the UUID string
        dnsUUID.Assign(Substring(nsDependentCString(uuid), 1, NSID_LENGTH - 3));

        // Create events for A and SRV resolvers
        events[0] = CreateEvent(NULL, TRUE, FALSE, TEXT("FinishedA"));
        if (!events[0]) {
            goto library_cleanup;
        }

        events[1] = CreateEvent(NULL, TRUE, FALSE, TEXT("FinishedSRV"));
        if (!events[1]) {
            goto aevent_cleanup;
        }

        // dispatch A resolver
        mResolveAThread->Dispatch(new ExperimentResolver(DNS_TYPE_A,
                                                         dnsUUID,
                                                         &events[0],
                                                         &startALookup,
                                                         &endALookup,
                                                         &aStatus,
                                                         &aResults,
                                                         dnsQuery),
                                  NS_DISPATCH_NORMAL);

        // dispatch SRV resolver
        mResolveSRVThread->Dispatch(new ExperimentResolver(DNS_TYPE_SRV,
                                                           dnsUUID,
                                                           &events[1],
                                                           &startSRVLookup,
                                                           &endSRVLookup,
                                                           &srvStatus,
                                                           &srvResults,
                                                           dnsQuery),
                                    NS_DISPATCH_NORMAL);

        WaitForMultipleObjects(2, events, TRUE, INFINITE);

        // Ensure we got the expected results
        if (aStatus == DNS_RCODE_NOERROR) {
            if (aResults->Data.A.IpAddress != 0x7F000001) {
                correctA = false;
            }
        }

        if (srvStatus == DNS_RCODE_NOERROR) {
            DNS_SRV_DATA *srvData = &srvResults->Data.Srv;
            if (_stricmp(srvData->pNameTarget, "success.http2test.mozilla.org") ||
                srvData->wPort != 443 ||
                srvData->wPriority != 100 ||
                srvData->wWeight != 100) {
                correctSRV = false;
            }
        }

        if (aStatus == DNS_RCODE_NOERROR && srvStatus == DNS_RCODE_NOERROR) {
            experimentStatus = kBothSucceed;
            deltaKey = Telemetry::SRV_EXPERIMENT_SUCCESS_DELTA;
            Telemetry::Accumulate(Telemetry::SRV_EXPERIMENT_A_CORRECT, correctA);
            Telemetry::Accumulate(Telemetry::SRV_EXPERIMENT_SRV_CORRECT, correctSRV);
        } else if (aStatus == DNS_RCODE_NOERROR) {
            experimentStatus = kSRVFail;
            deltaKey = Telemetry::SRV_EXPERIMENT_SRV_FAIL_DELTA;
            Telemetry::Accumulate(Telemetry::SRV_EXPERIMENT_SRV_CORRECT, correctSRV);
        } else if (srvStatus == DNS_RCODE_NOERROR) {
            experimentStatus = kAFail;
            deltaKey = Telemetry::SRV_EXPERIMENT_A_FAIL_DELTA;
            Telemetry::Accumulate(Telemetry::SRV_EXPERIMENT_A_CORRECT, correctA);
        } else { // aStatus != DNS_RCODE_NOERROR && srvStatus != DNS_RCODE_NOERROR
            experimentStatus = kBothFail;
            deltaKey = Telemetry::SRV_EXPERIMENT_FAIL_DELTA;
            // Neither one succeeded, so our correctness flags are irrelevant
        }

        Telemetry::Accumulate(Telemetry::SRV_EXPERIMENT_STATUS,
                              experimentStatus);
        Telemetry::AccumulateTimeDelta(Telemetry::SRV_EXPERIMENT_SRV_TIME,
                                       startSRVLookup, endSRVLookup);
        Telemetry::AccumulateTimeDelta(Telemetry::SRV_EXPERIMENT_A_TIME,
                                       startALookup, endALookup);

        // Calculate time delta in ms clamped to [0, 2000] where 1000 means both
        // lookups took the same amount of time, 0-999 means the SRV record came
        // back faster, and 1001-2000 means the A record came back faster.
        duration = (endALookup - startALookup) - (endSRVLookup - startSRVLookup);
        delta = duration.ToMilliseconds();
        if (delta < -1000.0) {
            delta = -1000.0;
        } else if (delta > 1000.0) {
            delta = 1000.0;
        }

        timeDelta = static_cast<uint32_t>(delta) + 1000;
        Telemetry::Accumulate(deltaKey, timeDelta);

        mResolveSRVThread->Shutdown();
        mResolveAThread->Shutdown();
        CloseHandle(events[1]);
aevent_cleanup:
        CloseHandle(events[0]);
        // Do the library cleanup here to avoid doing I/O on the main thread
library_cleanup:
        dnsQuery = nullptr;
        PR_UnloadLibrary(lib);
out:
        NS_DispatchToMainThread(new ExperimentFinishedRunner(mExperimentThread));

        return NS_OK;
    }

private:
    nsCOMPtr<nsIThread> mExperimentThread;
    nsCOMPtr<nsIThread> mResolveAThread;
    nsCOMPtr<nsIThread> mResolveSRVThread;
    static const uint32_t kBothSucceed = 0;
    static const uint32_t kSRVFail = 1;
    static const uint32_t kAFail = 2;
    static const uint32_t kBothFail = 3;
};

void
nsHostResolver::RunExperiment()
{
    if (!NS_IsMainThread()) {
        return;
    }

    {
        MutexAutoLock lock(mExperimentLock);
        if (mHasRunExperiment) {
            return;
        }

        mHasRunExperiment = true;
    }

    nsCOMPtr<nsIThread> experimentThread;
    NS_NewNamedThread("SRV Experiment", getter_AddRefs(experimentThread));
    if (!experimentThread) {
        return;
    }

    // Create threads for A and SRV resolvers
    nsCOMPtr<nsIThread> resolveAThread;
    NS_NewNamedThread("Experiment A", getter_AddRefs(resolveAThread));
    if (!resolveAThread) {
        experimentThread->Shutdown();
        return;
    }

    nsCOMPtr<nsIThread> resolveSRVThread;
    NS_NewNamedThread("Experiment SRV", getter_AddRefs(resolveSRVThread));
    if (!resolveSRVThread) {
        resolveAThread->Shutdown();
        experimentThread->Shutdown();
        return;
    }

    experimentThread->Dispatch(new ExperimentRunner(experimentThread,
                                                    resolveAThread,
                                                    resolveSRVThread),
                               NS_DISPATCH_NORMAL);
}
#endif

nsresult
nsHostResolver::IssueLookup(nsHostRecord *rec)
{
    MOZ_EVENT_TRACER_WAIT(rec, "net::dns::resolve");

#if defined(XP_WIN)
    // See bug 942317 in case you're all "WTF, mate?!"
    if (mNetworkExperimentsOK && mDnsExperimentOK && Telemetry::CanRecord() &&
        !mHasRunExperiment) {
        int offset = strlen(rec->host) - strlen(".mozilla.org");
        if ((offset > 0) && (_stricmp(rec->host + offset, ".mozilla.org") == 0)) {
            RunExperiment();
        }
    }
#endif

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
    
    rec->resolving = true;
    rec->onQueue = true;

    rv = ConditionallyCreateThread(rec);
    
    LOG (("  DNS thread counters: total=%d any-live=%d idle=%d pending=%d\n",
          mThreadCount,
          mActiveAnyThreadCount,
          mNumIdleThreads,
          mPendingCount));

    return rv;
}

nsresult
nsHostResolver::ConditionallyRefreshRecord(nsHostRecord *rec, const char *host)
{
    if ((((TimeStamp::NowLoRes() > rec->expiration) &&
        rec->mBlacklistedItems.Length()) ||
        rec->negative) && !rec->resolving) {
        LOG(("  Using %s cache entry for host [%s] but starting async renewal.",
            rec->negative ? "negative" :"positive", host));
        IssueLookup(rec);

        if (!rec->negative) {
            // negative entries are constantly being refreshed, only
            // track positive grace period induced renewals
            Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                METHOD_RENEWAL);
        }
    }
    return NS_OK;
}

void
nsHostResolver::DeQueue(PRCList &aQ, nsHostRecord **aResult)
{
    *aResult = static_cast<nsHostRecord *>(aQ.next);
    PR_REMOVE_AND_INIT_LINK(*aResult);
    mPendingCount--;
    (*aResult)->onQueue = false;
}

bool
nsHostResolver::GetHostToLookup(nsHostRecord **result)
{
    bool timedOut = false;
    PRIntervalTime epoch, now, timeout;
    
    MutexAutoLock lock(mLock);

    timeout = (mNumIdleThreads >= HighThreadThreshold) ? mShortIdleTimeout : mLongIdleTimeout;
    epoch = PR_IntervalNow();

    while (!mShutdown) {
        // remove next record from Q; hand over owning reference. Check high, then med, then low
        
        if (!PR_CLIST_IS_EMPTY(&mHighQ)) {
            DeQueue (mHighQ, result);
            return true;
        }

        if (mActiveAnyThreadCount < HighThreadThreshold) {
            if (!PR_CLIST_IS_EMPTY(&mMediumQ)) {
                DeQueue (mMediumQ, result);
                mActiveAnyThreadCount++;
                (*result)->usingAnyThread = true;
                return true;
            }
            
            if (!PR_CLIST_IS_EMPTY(&mLowQ)) {
                DeQueue (mLowQ, result);
                mActiveAnyThreadCount++;
                (*result)->usingAnyThread = true;
                return true;
            }
        }
        
        // Determining timeout is racy, so allow one cycle through checking the queues
        // before exiting.
        if (timedOut)
            break;

        // wait for one or more of the following to occur:
        //  (1) the pending queue has a host record to process
        //  (2) the shutdown flag has been set
        //  (3) the thread has been idle for too long
        
        mNumIdleThreads++;
        mIdleThreadCV.Wait(timeout);
        mNumIdleThreads--;
        
        now = PR_IntervalNow();
        
        if ((PRIntervalTime)(now - epoch) >= timeout)
            timedOut = true;
        else {
            // It is possible that PR_WaitCondVar() was interrupted and returned early,
            // in which case we will loop back and re-enter it. In that case we want to
            // do so with the new timeout reduced to reflect time already spent waiting.
            timeout -= (PRIntervalTime)(now - epoch);
            epoch = now;
        }
    }
    
    // tell thread to exit...
    mThreadCount--;
    return false;
}

void
nsHostResolver::OnLookupComplete(nsHostRecord *rec, nsresult status, AddrInfo *result)
{
    // get the list of pending callbacks for this lookup, and notify
    // them that the lookup is complete.
    PRCList cbs;
    PR_INIT_CLIST(&cbs);
    {
        MutexAutoLock lock(mLock);

        // grab list of callbacks to notify
        MoveCList(rec->callbacks, cbs);

        // update record fields.  We might have a rec->addr_info already if a
        // previous lookup result expired and we're reresolving it..
        AddrInfo  *old_addr_info;
        {
            MutexAutoLock lock(rec->addr_info_lock);
            old_addr_info = rec->addr_info;
            rec->addr_info = result;
            rec->addr_info_gencnt++;
        }
        delete old_addr_info;

        rec->expiration = TimeStamp::NowLoRes();
        if (result) {
            rec->expiration += mMaxCacheLifetime;
            rec->negative = false;
        }
        else {
            rec->expiration += TimeDuration::FromSeconds(60); /* one minute for negative cache */
            rec->negative = true;
        }
        rec->resolving = false;
        
        if (rec->usingAnyThread) {
            mActiveAnyThreadCount--;
            rec->usingAnyThread = false;
        }

        if (!mShutdown) {
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

                if (!head->negative) {
                    // record the age of the entry upon eviction.
                    TimeDuration age = TimeStamp::NowLoRes() -
                                         (head->expiration - mMaxCacheLifetime);
                    Telemetry::Accumulate(Telemetry::DNS_CLEANUP_AGE,
                                          static_cast<uint32_t>(age.ToSeconds() / 60));
                }

                // release reference to rec owned by mEvictionQ
                NS_RELEASE(head);
            }
        }
    }

    MOZ_EVENT_TRACER_DONE(rec, "net::dns::resolve");

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

void
nsHostResolver::CancelAsyncRequest(const char            *host,
                                   uint16_t               flags,
                                   uint16_t               af,
                                   nsIDNSListener        *aListener,
                                   nsresult               status)

{
    MutexAutoLock lock(mLock);

    // Lookup the host record associated with host, flags & address family
    nsHostKey key = { host, flags, af };
    nsHostDBEnt *he = static_cast<nsHostDBEnt *>
                      (PL_DHashTableOperate(&mDB, &key, PL_DHASH_LOOKUP));
    if (he && he->rec) {
        nsHostRecord* recPtr = nullptr;
        PRCList *node = he->rec->callbacks.next;
        // Remove the first nsDNSAsyncRequest callback which matches the
        // supplied listener object
        while (node != &he->rec->callbacks) {
            nsResolveHostCallback *callback
                = static_cast<nsResolveHostCallback *>(node);
            if (callback && (callback->EqualsAsyncListener(aListener))) {
                // Remove from the list of callbacks
                PR_REMOVE_LINK(callback);
                recPtr = he->rec;
                callback->OnLookupComplete(this, recPtr, status);
                break;
            }
            node = node->next;
        }

        // If there are no more callbacks, remove the hash table entry
        if (recPtr && PR_CLIST_IS_EMPTY(&recPtr->callbacks)) {
            PL_DHashTableOperate(&mDB, (nsHostKey *)recPtr, PL_DHASH_REMOVE);
            // If record is on a Queue, remove it and then deref it
            if (recPtr->next != recPtr) {
                PR_REMOVE_LINK(recPtr);
                NS_RELEASE(recPtr);
            }
        }
    }
}

static size_t
SizeOfHostDBEntExcludingThis(PLDHashEntryHdr* hdr, MallocSizeOf mallocSizeOf,
                             void*)
{
    nsHostDBEnt* ent = static_cast<nsHostDBEnt*>(hdr);
    return ent->rec->SizeOfIncludingThis(mallocSizeOf);
}

size_t
nsHostResolver::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const
{
    MutexAutoLock lock(mLock);

    size_t n = mallocSizeOf(this);
    n += PL_DHashTableSizeOfExcludingThis(&mDB, SizeOfHostDBEntExcludingThis,
                                          mallocSizeOf);

    // The following fields aren't measured.
    // - mHighQ, mMediumQ, mLowQ, mEvictionQ, because they just point to
    //   nsHostRecords that also pointed to by entries |mDB|, and measured when
    //   |mDB| is measured.

    return n;
}

void
nsHostResolver::ThreadFunc(void *arg)
{
    LOG(("DNS lookup thread - starting execution.\n"));

    static nsThreadPoolNaming naming;
    naming.SetThreadPoolName(NS_LITERAL_CSTRING("DNS Resolver"));

#if defined(RES_RETRY_ON_FAILURE)
    nsResState rs;
#endif
    nsHostResolver *resolver = (nsHostResolver *)arg;
    nsHostRecord *rec;
    PRAddrInfo *prai = nullptr;
    while (resolver->GetHostToLookup(&rec)) {
        LOG(("DNS lookup thread - Calling getaddrinfo for host [%s].\n",
             rec->host));

        int flags = PR_AI_ADDRCONFIG;
        if (!(rec->flags & RES_CANON_NAME))
            flags |= PR_AI_NOCANONNAME;

        TimeStamp startTime = TimeStamp::Now();
        MOZ_EVENT_TRACER_EXEC(rec, "net::dns::resolve");

        // We need to remove IPv4 records manually
        // because PR_GetAddrInfoByName doesn't support PR_AF_INET6.
        bool disableIPv4 = rec->af == PR_AF_INET6;
        uint16_t af = disableIPv4 ? PR_AF_UNSPEC : rec->af;
        prai = PR_GetAddrInfoByName(rec->host, af, flags);
#if defined(RES_RETRY_ON_FAILURE)
        if (!prai && rs.Reset())
            prai = PR_GetAddrInfoByName(rec->host, af, flags);
#endif

        TimeDuration elapsed = TimeStamp::Now() - startTime;
        uint32_t millis = static_cast<uint32_t>(elapsed.ToMilliseconds());

        // convert error code to nsresult
        nsresult status;
        AddrInfo *ai = nullptr;
        if (prai) {
            const char *cname = nullptr;
            if (rec->flags & RES_CANON_NAME)
                cname = PR_GetCanonNameFromAddrInfo(prai);
            ai = new AddrInfo(rec->host, prai, disableIPv4, cname);
            PR_FreeAddrInfo(prai);
            if (ai->mAddresses.isEmpty()) {
                delete ai;
                ai = nullptr;
            }
        }
        if (ai) {
            status = NS_OK;

            Telemetry::Accumulate(!rec->addr_info_gencnt ?
                                    Telemetry::DNS_LOOKUP_TIME :
                                    Telemetry::DNS_RENEWAL_TIME,
                                  millis);
        }
        else {
            status = NS_ERROR_UNKNOWN_HOST;
            Telemetry::Accumulate(Telemetry::DNS_FAILED_LOOKUP_TIME, millis);
        }

        // OnLookupComplete may release "rec", log before we lose it.
        LOG(("DNS lookup thread - lookup completed for host [%s]: %s.\n",
             rec->host, ai ? "success" : "failure: unknown host"));
        resolver->OnLookupComplete(rec, status, ai);
    }
    NS_RELEASE(resolver);
    LOG(("DNS lookup thread - queue empty, thread finished.\n"));
}

nsresult
nsHostResolver::Create(uint32_t         maxCacheEntries,
                       uint32_t         maxCacheLifetime,
                       uint32_t         lifetimeGracePeriod,
                       nsHostResolver **result)
{
#if defined(PR_LOGGING)
    if (!gHostResolverLog)
        gHostResolverLog = PR_NewLogModule("nsHostResolver");
#endif

    nsHostResolver *res = new nsHostResolver(maxCacheEntries,
                                             maxCacheLifetime,
                                             lifetimeGracePeriod);
    if (!res)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(res);

    nsresult rv = res->Init();
    if (NS_FAILED(rv))
        NS_RELEASE(res);

    *result = res;
    return rv;
}

PLDHashOperator
CacheEntryEnumerator(PLDHashTable *table, PLDHashEntryHdr *entry,
                     uint32_t number, void *arg)
{
    // We don't pay attention to address literals, only resolved domains.
    // Also require a host.
    nsHostRecord *rec = static_cast<nsHostDBEnt*>(entry)->rec;
    MOZ_ASSERT(rec, "rec should never be null here!");
    if (!rec || !rec->addr_info || !rec->host) {
        return PL_DHASH_NEXT;
    }

    DNSCacheEntries info;
    info.hostname = rec->host;
    info.family = rec->af;
    info.expiration = (int64_t)(rec->expiration - TimeStamp::NowLoRes()).ToSeconds();
    if (info.expiration <= 0) {
        // We only need valid DNS cache entries
        return PL_DHASH_NEXT;
    }

    {
        MutexAutoLock lock(rec->addr_info_lock);

        NetAddr *addr = nullptr;
        NetAddrElement *addrElement = rec->addr_info->mAddresses.getFirst();
        if (addrElement) {
            addr = &addrElement->mAddress;
        }
        while (addr) {
            char buf[kIPv6CStrBufSize];
            if (NetAddrToString(addr, buf, sizeof(buf))) {
                info.hostaddr.AppendElement(buf);
            }
            addr = nullptr;
            addrElement = addrElement->getNext();
            if (addrElement) {
                addr = &addrElement->mAddress;
            }
        }
    }

    nsTArray<DNSCacheEntries> *args = static_cast<nsTArray<DNSCacheEntries> *>(arg);
    args->AppendElement(info);

    return PL_DHASH_NEXT;
}

void
nsHostResolver::GetDNSCacheEntries(nsTArray<DNSCacheEntries> *args)
{
    PL_DHashTableEnumerate(&mDB, CacheEntryEnumerator, args);
}
