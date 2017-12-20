/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(HAVE_RES_NINIT)
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#define RES_RETRY_ON_FAILURE
#endif

#include <stdlib.h>
#include <ctime>
#include "nsHostResolver.h"
#include "nsError.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"
#include "nsAutoPtr.h"
#include "nsPrintfCString.h"
#include "prthread.h"
#include "prerror.h"
#include "prtime.h"
#include "mozilla/Logging.h"
#include "PLDHashTable.h"
#include "plstr.h"
#include "nsURLHelper.h"
#include "nsThreadUtils.h"
#include "GetAddrInfo.h"
#include "GeckoProfiler.h"

#include "mozilla/HashFunctions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::net;

// None of our implementations expose a TTL for negative responses, so we use a
// constant always.
static const unsigned int NEGATIVE_RECORD_LIFETIME = 60;

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

static_assert(HighThreadThreshold <= MAX_RESOLVER_THREADS,
              "High Thread Threshold should be less equal Maximum allowed thread");

//----------------------------------------------------------------------------

static LazyLogModule gHostResolverLog("nsHostResolver");
#define LOG(args) MOZ_LOG(gHostResolverLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gHostResolverLog, mozilla::LogLevel::Debug)

#define LOG_HOST(host, interface) host,                                        \
                 (interface && interface[0] != '\0') ? " on interface " : "",  \
                 (interface && interface[0] != '\0') ? interface : ""

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
#if TTL_AVAILABLE
    , mGetTtl(false)
#endif
    , mBlacklistedCount(0)
    , mResolveAgain(false)
{
    host = ((char *) this) + sizeof(nsHostRecord);
    memcpy((char *) host, key->host, strlen(key->host) + 1);
    flags = key->flags;
    af = key->af;
    netInterface = host + strlen(key->host) + 1;
    memcpy((char *) netInterface, key->netInterface,
           strlen(key->netInterface) + 1);
    originSuffix = netInterface + strlen(key->netInterface) + 1;
    memcpy((char *) originSuffix, key->originSuffix,
           strlen(key->originSuffix) + 1);
    PR_INIT_CLIST(this);
}

nsresult
nsHostRecord::Create(const nsHostKey *key, nsHostRecord **result)
{
    size_t hostLen = strlen(key->host) + 1;
    size_t netInterfaceLen = strlen(key->netInterface) + 1;
    size_t originSuffixLen = strlen(key->originSuffix) + 1;
    size_t size = hostLen + netInterfaceLen + originSuffixLen + sizeof(nsHostRecord);

    // Use placement new to create the object with room for the hostname,
    // network interface name and originSuffix allocated after it.
    void *place = ::operator new(size);
    *result = new(place) nsHostRecord(key);
    NS_ADDREF(*result);

    return NS_OK;
}

void
nsHostRecord::SetExpiration(const mozilla::TimeStamp& now, unsigned int valid, unsigned int grace)
{
    mValidStart = now;
    mGraceStart = now + TimeDuration::FromSeconds(valid);
    mValidEnd = now + TimeDuration::FromSeconds(valid + grace);
}

void
nsHostRecord::CopyExpirationTimesAndFlagsFrom(const nsHostRecord *aFromHostRecord)
{
    // This is used to copy information from a cache entry to a record. All
    // information necessary for HasUsableRecord needs to be copied.
    mValidStart = aFromHostRecord->mValidStart;
    mValidEnd = aFromHostRecord->mValidEnd;
    mGraceStart = aFromHostRecord->mGraceStart;
    mDoomed = aFromHostRecord->mDoomed;
}

nsHostRecord::~nsHostRecord()
{
    mCallbacks.clear();

    Telemetry::Accumulate(Telemetry::DNS_BLACKLIST_COUNT, mBlacklistedCount);
    delete addr_info;
}

bool
nsHostRecord::Blacklisted(NetAddr *aQuery)
{
    // must call locked
    LOG(("Checking blacklist for host [%s%s%s], host record [%p].\n",
          LOG_HOST(host, netInterface), this));

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
            LOG(("Address [%s] is blacklisted for host [%s%s%s].\n", buf,
                 LOG_HOST(host, netInterface)));
            return true;
        }
    }

    return false;
}

void
nsHostRecord::ReportUnusable(NetAddr *aAddress)
{
    // must call locked
    LOG(("Adding address to blacklist for host [%s%s%s], host record [%p].\n",
         LOG_HOST(host, netInterface), this));

    ++mBlacklistedCount;

    if (negative)
        mDoomed = true;

    char buf[kIPv6CStrBufSize];
    if (NetAddrToString(aAddress, buf, sizeof(buf))) {
        LOG(("Successfully adding address [%s] to blacklist for host "
             "[%s%s%s].\n", buf, LOG_HOST(host, netInterface)));
        mBlacklistedItems.AppendElement(nsCString(buf));
    }
}

void
nsHostRecord::ResetBlacklist()
{
    // must call locked
    LOG(("Resetting blacklist for host [%s%s%s], host record [%p].\n",
         LOG_HOST(host, netInterface), this));
    mBlacklistedItems.Clear();
}

nsHostRecord::ExpirationStatus
nsHostRecord::CheckExpiration(const mozilla::TimeStamp& now) const {
    if (!mGraceStart.IsNull() && now >= mGraceStart
            && !mValidEnd.IsNull() && now < mValidEnd) {
        return nsHostRecord::EXP_GRACE;
    }
    if (!mValidEnd.IsNull() && now < mValidEnd) {
        return nsHostRecord::EXP_VALID;
    }

    return nsHostRecord::EXP_EXPIRED;
}


bool
nsHostRecord::HasUsableResult(const mozilla::TimeStamp& now, uint16_t queryFlags) const
{
    if (mDoomed) {
        return false;
    }

    // don't use cached negative results for high priority queries.
    if (negative && IsHighPriority(queryFlags)) {
        return false;
    }

    if (CheckExpiration(now) == EXP_EXPIRED) {
        return false;
    }

    return addr_info || addr || negative;
}

static size_t
SizeOfResolveHostCallbackListExcludingHead(const mozilla::LinkedList<RefPtr<nsResolveHostCallback>>& aCallbacks,
                                           MallocSizeOf mallocSizeOf)
{
    size_t n = aCallbacks.sizeOfExcludingThis(mallocSizeOf);

    for (const nsResolveHostCallback* t = aCallbacks.getFirst(); t; t = t->getNext()) {
      n += t->SizeOfIncludingThis(mallocSizeOf);
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

    n += SizeOfResolveHostCallbackListExcludingHead(mCallbacks, mallocSizeOf);
    n += addr_info ? addr_info->SizeOfIncludingThis(mallocSizeOf) : 0;
    n += mallocSizeOf(addr.get());

    n += mBlacklistedItems.ShallowSizeOfExcludingThis(mallocSizeOf);
    for (size_t i = 0; i < mBlacklistedItems.Length(); i++) {
        n += mBlacklistedItems[i].SizeOfExcludingThisIfUnshared(mallocSizeOf);
    }
    return n;
}

nsHostRecord::DnsPriority
nsHostRecord::GetPriority(uint16_t aFlags)
{
    if (IsHighPriority(aFlags)){
        return nsHostRecord::DNS_PRIORITY_HIGH;
    }
    if (IsMediumPriority(aFlags)) {
        return nsHostRecord::DNS_PRIORITY_MEDIUM;
    }

    return nsHostRecord::DNS_PRIORITY_LOW;
}

// Returns true if the entry can be removed, or false if it should be left.
// Sets mResolveAgain true for entries being resolved right now.
bool
nsHostRecord::RemoveOrRefresh()
{
    if (resolving) {
        if (!onQueue) {
            // The request has been passed to the OS resolver. The resultant DNS
            // record should be considered stale and not trusted; set a flag to
            // ensure it is called again.
            mResolveAgain = true;
        }
        // if Onqueue is true, the host entry is already added to the cache
        // but is still pending to get resolved: just leave it in hash.
        return false;
    }
    // Already resolved; not in a pending state; remove from cache.
    return true;
}

//----------------------------------------------------------------------------

struct nsHostDBEnt : PLDHashEntryHdr
{
    nsHostRecord *rec;
};

static PLDHashNumber
HostDB_HashKey(const void *key)
{
    const nsHostKey *hk = static_cast<const nsHostKey *>(key);
    return AddToHash(HashString(hk->host), RES_KEY_FLAGS(hk->flags), hk->af,
                     HashString(hk->netInterface), HashString(hk->originSuffix));
}

static bool
HostDB_MatchEntry(const PLDHashEntryHdr *entry,
                  const void *key)
{
    const nsHostDBEnt *he = static_cast<const nsHostDBEnt *>(entry);
    const nsHostKey *hk = static_cast<const nsHostKey *>(key);

    return !strcmp(he->rec->host ? he->rec->host : "",
                   hk->host ? hk->host : "") &&
            RES_KEY_FLAGS (he->rec->flags) == RES_KEY_FLAGS(hk->flags) &&
            he->rec->af == hk->af &&
            !strcmp(he->rec->netInterface, hk->netInterface) &&
            !strcmp(he->rec->originSuffix, hk->originSuffix);
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

    LOG(("Clearing cache db entry for host [%s%s%s].\n",
         LOG_HOST(hr->host, hr->netInterface)));
#if defined(DEBUG)
    {
        MutexAutoLock lock(hr->addr_info_lock);
        if (!hr->addr_info) {
            LOG(("No address info for host [%s%s%s].\n",
                 LOG_HOST(hr->host, hr->netInterface)));
        } else {
            if (!hr->mValidEnd.IsNull()) {
                TimeDuration diff = hr->mValidEnd - TimeStamp::NowLoRes();
                LOG(("Record for host [%s%s%s] expires in %f seconds.\n",
                     LOG_HOST(hr->host, hr->netInterface),
                     diff.ToSeconds()));
            } else {
                LOG(("Record for host [%s%s%s] not yet valid.\n",
                     LOG_HOST(hr->host, hr->netInterface)));
            }

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

static void
HostDB_InitEntry(PLDHashEntryHdr *entry,
                 const void *key)
{
    nsHostDBEnt *he = static_cast<nsHostDBEnt *>(entry);
    nsHostRecord::Create(static_cast<const nsHostKey *>(key), &he->rec);
}

static const PLDHashTableOps gHostDB_ops =
{
    HostDB_HashKey,
    HostDB_MatchEntry,
    HostDB_MoveEntry,
    HostDB_ClearEntry,
    HostDB_InitEntry,
};

//----------------------------------------------------------------------------

#if TTL_AVAILABLE
static const char kPrefGetTtl[] = "network.dns.get-ttl";
static bool sGetTtlEnabled = false;

static void DnsPrefChanged(const char* aPref, void* aClosure)
{
    MOZ_ASSERT(NS_IsMainThread(),
               "Should be getting pref changed notification on main thread!");

    if (strcmp(aPref, kPrefGetTtl) != 0) {
        LOG(("DnsPrefChanged ignoring pref \"%s\"", aPref));
        return;
    }

    DebugOnly<nsHostResolver*> self = static_cast<nsHostResolver*>(aClosure);
    MOZ_ASSERT(self);

    sGetTtlEnabled = Preferences::GetBool(kPrefGetTtl);
}
#endif

nsHostResolver::nsHostResolver(uint32_t maxCacheEntries,
                               uint32_t defaultCacheEntryLifetime,
                               uint32_t defaultGracePeriod)
    : mMaxCacheEntries(maxCacheEntries)
    , mDefaultCacheLifetime(defaultCacheEntryLifetime)
    , mDefaultGracePeriod(defaultGracePeriod)
    , mLock("nsHostResolver.mLock")
    , mIdleThreadCV(mLock, "nsHostResolver.mIdleThreadCV")
    , mDB(&gHostDB_ops, sizeof(nsHostDBEnt), 0)
    , mEvictionQSize(0)
    , mShutdown(true)
    , mNumIdleThreads(0)
    , mThreadCount(0)
    , mActiveAnyThreadCount(0)
    , mPendingCount(0)
{
    mCreationTime = PR_Now();
    PR_INIT_CLIST(&mHighQ);
    PR_INIT_CLIST(&mMediumQ);
    PR_INIT_CLIST(&mLowQ);
    PR_INIT_CLIST(&mEvictionQ);

    mLongIdleTimeout  = PR_SecondsToInterval(LongIdleTimeoutSeconds);
    mShortIdleTimeout = PR_SecondsToInterval(ShortIdleTimeoutSeconds);
}

nsHostResolver::~nsHostResolver() = default;

nsresult
nsHostResolver::Init()
{
    if (NS_FAILED(GetAddrInfoInit())) {
        return NS_ERROR_FAILURE;
    }

    LOG(("nsHostResolver::Init this=%p", this));

    mShutdown = false;

#if TTL_AVAILABLE
    // The preferences probably haven't been loaded from the disk yet, so we
    // need to register a callback that will set up the experiment once they
    // are. We also need to explicitly set a value for the props otherwise the
    // callback won't be called.
    {
        DebugOnly<nsresult> rv = Preferences::RegisterCallbackAndCall(
            &DnsPrefChanged, kPrefGetTtl, this);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "Could not register DNS TTL pref callback.");
    }
#endif

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
            CompleteLookup(rec, NS_ERROR_ABORT, nullptr);
        }
    }
}

//
// FlushCache() is what we call when the network has changed. We must not
// trust names that were resolved before this change. They may resolve
// differently now.
//
// This function removes all existing resolved host entries from the hash.
// Names that are in the pending queues can be left there. Entries in the
// cache that have 'Resolve' set true but not 'onQueue' are being resolved
// right now, so we need to mark them to get re-resolved on completion!

void
nsHostResolver::FlushCache()
{
    MutexAutoLock lock(mLock);
    mEvictionQSize = 0;

    // Clear the evictionQ and remove all its corresponding entries from
    // the cache first
    if (!PR_CLIST_IS_EMPTY(&mEvictionQ)) {
        PRCList *node = mEvictionQ.next;
        while (node != &mEvictionQ) {
            nsHostRecord *rec = static_cast<nsHostRecord *>(node);
            node = node->next;
            PR_REMOVE_AND_INIT_LINK(rec);
            mDB.Remove((nsHostKey *) rec);
            NS_RELEASE(rec);
        }
    }

    // Refresh the cache entries that are resolving RIGHT now, remove the rest.
    for (auto iter = mDB.Iter(); !iter.Done(); iter.Next()) {
        auto entry = static_cast<nsHostDBEnt *>(iter.Get());
        // Try to remove the record, or mark it for refresh.
        if (entry->rec->RemoveOrRefresh()) {
            PR_REMOVE_LINK(entry->rec);
            iter.Remove();
        }
    }
}

void
nsHostResolver::Shutdown()
{
    LOG(("Shutting down host resolver.\n"));

#if TTL_AVAILABLE
    {
        DebugOnly<nsresult> rv = Preferences::UnregisterCallback(
            &DnsPrefChanged, kPrefGetTtl, this);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "Could not unregister DNS TTL pref callback.");
    }
#endif

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
        mDB.Clear();
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

    {
        mozilla::DebugOnly<nsresult> rv = GetAddrInfoShutdown();
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "Failed to shutdown GetAddrInfo");
    }
}

void
nsHostResolver::MoveQueue(nsHostRecord *aRec, PRCList &aDestQ)
{
    NS_ASSERTION(aRec->onQueue, "Moving Host Record Not Currently Queued");

    PR_REMOVE_LINK(aRec);
    PR_APPEND_LINK(aRec, &aDestQ);
}

nsresult
nsHostResolver::ResolveHost(const char             *host,
                            const OriginAttributes &aOriginAttributes,
                            uint16_t                flags,
                            uint16_t                af,
                            const char             *netInterface,
                            nsResolveHostCallback  *aCallback)
{
    NS_ENSURE_TRUE(host && *host, NS_ERROR_UNEXPECTED);
    NS_ENSURE_TRUE(netInterface, NS_ERROR_UNEXPECTED);

    LOG(("Resolving host [%s%s%s]%s.\n", LOG_HOST(host, netInterface),
         flags & RES_BYPASS_CACHE ? " - bypassing cache" : ""));

    // ensure that we are working with a valid hostname before proceeding.  see
    // bug 304904 for details.
    if (!net_IsValidHostName(nsDependentCString(host)))
        return NS_ERROR_UNKNOWN_HOST;

    RefPtr<nsResolveHostCallback> callback(aCallback);
    // if result is set inside the lock, then we need to issue the
    // callback before returning.
    RefPtr<nsHostRecord> result;
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
            nsAutoCString originSuffix;
            aOriginAttributes.CreateSuffix(originSuffix);

            nsHostKey key = { host, flags, af, netInterface, originSuffix.get() };
            auto he = static_cast<nsHostDBEnt*>(mDB.Add(&key, fallible));

            // if the record is null, the hash table OOM'd.
            if (!he) {
                LOG(("  Out of memory: no cache entry for host [%s%s%s].\n",
                     LOG_HOST(host, netInterface)));
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
            // do we have a cached result that we can reuse?
            else if (!(flags & RES_BYPASS_CACHE) &&
                     he->rec->HasUsableResult(TimeStamp::NowLoRes(), flags)) {
                LOG(("  Using cached record for host [%s%s%s].\n",
                     LOG_HOST(host, netInterface)));
                // put reference to host record on stack...
                result = he->rec;
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_HIT);

                // For entries that are in the grace period
                // or all cached negative entries, use the cache but start a new
                // lookup in the background
                ConditionallyRefreshRecord(he->rec, host);

                if (he->rec->negative) {
                    LOG(("  Negative cache entry for host [%s%s%s].\n",
                         LOG_HOST(host, netInterface)));
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
                he->rec->addr = MakeUnique<NetAddr>();
                PRNetAddrToNetAddr(&tempAddr, he->rec->addr.get());
                // put reference to host record on stack...
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                      METHOD_LITERAL);
                result = he->rec;
            }
            else if (mPendingCount >= MAX_NON_PRIORITY_REQUESTS &&
                     !IsHighPriority(flags) &&
                     !he->rec->resolving) {
                LOG(("  Lookup queue full: dropping %s priority request for "
                     "host [%s%s%s].\n",
                     IsMediumPriority(flags) ? "medium" : "low",
                     LOG_HOST(host, netInterface)));
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                      METHOD_OVERFLOW);
                // This is a lower priority request and we are swamped, so refuse it.
                rv = NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
            }
            else if (flags & RES_OFFLINE) {
                LOG(("  Offline request for host [%s%s%s]; ignoring.\n",
                     LOG_HOST(host, netInterface)));
                rv = NS_ERROR_OFFLINE;
            }

            // If this is an IPV4 or IPV6 specific request, check if there is
            // an AF_UNSPEC entry we can use. Otherwise, hit the resolver...
            else if (!he->rec->resolving) {
                if (!(flags & RES_BYPASS_CACHE) &&
                    ((af == PR_AF_INET) || (af == PR_AF_INET6))) {
                    // First, search for an entry with AF_UNSPEC
                    const nsHostKey unspecKey = { host, flags, PR_AF_UNSPEC,
                                                  netInterface, originSuffix.get() };
                    auto unspecHe =
                        static_cast<nsHostDBEnt*>(mDB.Search(&unspecKey));
                    NS_ASSERTION(!unspecHe ||
                                 (unspecHe && unspecHe->rec),
                                "Valid host entries should contain a record");
                    TimeStamp now = TimeStamp::NowLoRes();
                    if (unspecHe &&
                        unspecHe->rec->HasUsableResult(now, flags)) {

                        MOZ_ASSERT(unspecHe->rec->addr_info || unspecHe->rec->negative,
                                   "Entry should be resolved or negative.");

                        LOG(("  Trying AF_UNSPEC entry for host [%s%s%s] af: %s.\n",
                             LOG_HOST(host, netInterface),
                             (af == PR_AF_INET) ? "AF_INET" : "AF_INET6"));

                        // We need to lock in case any other thread is reading
                        // addr_info.
                        MutexAutoLock lock(he->rec->addr_info_lock);

                        // XXX: note that this actually leaks addr_info.
                        // For some reason, freeing the memory causes a crash in
                        // nsDNSRecord::GetNextAddr - see bug 1422173
                        he->rec->addr_info = nullptr;
                        if (unspecHe->rec->negative) {
                            he->rec->negative = unspecHe->rec->negative;
                            he->rec->CopyExpirationTimesAndFlagsFrom(unspecHe->rec);
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
                                        he->rec->CopyExpirationTimesAndFlagsFrom(unspecHe->rec);
                                    }
                                    he->rec->addr_info->AddAddress(
                                        new NetAddrElement(*addrIter));
                                }
                                addrIter = addrIter->getNext();
                            }
                        }
                        // Now check if we have a new record.
                        if (he->rec->HasUsableResult(now, flags)) {
                            result = he->rec;
                            if (he->rec->negative) {
                                status = NS_ERROR_UNKNOWN_HOST;
                            }
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
                                 "host [%s%s%s] unknown host.",
                                 LOG_HOST(host, netInterface)));
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
                    LOG(("  No usable address in cache for host [%s%s%s].",
                         LOG_HOST(host, netInterface)));

                    // Add callback to the list of pending callbacks.
                    he->rec->mCallbacks.insertBack(callback);
                    he->rec->flags = flags;
                    rv = IssueLookup(he->rec);
                    Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                          METHOD_NETWORK_FIRST);
                    if (NS_FAILED(rv) && callback->isInList()) {
                        callback->remove();
                    }
                    else {
                        LOG(("  DNS lookup for host [%s%s%s] blocking "
                             "pending 'getaddrinfo' query: callback [%p]",
                             LOG_HOST(host, netInterface), callback.get()));
                    }
                }
            }
            else {
                LOG(("  Host [%s%s%s] is being resolved. Appending callback "
                     "[%p].", LOG_HOST(host, netInterface), callback.get()));

                he->rec->mCallbacks.insertBack(callback);
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

    if (result) {
        if (callback->isInList()) {
            callback->remove();
        }
        callback->OnResolveHostComplete(this, result, status);
    }

    return rv;
}

void
nsHostResolver::DetachCallback(const char             *host,
                               const OriginAttributes &aOriginAttributes,
                               uint16_t                flags,
                               uint16_t                af,
                               const char             *netInterface,
                               nsResolveHostCallback  *aCallback,
                               nsresult                status)
{
    RefPtr<nsHostRecord> rec;
    RefPtr<nsResolveHostCallback> callback(aCallback);

    {
        MutexAutoLock lock(mLock);

        nsAutoCString originSuffix;
        aOriginAttributes.CreateSuffix(originSuffix);

        nsHostKey key = { host, flags, af, netInterface, originSuffix.get() };
        auto he = static_cast<nsHostDBEnt*>(mDB.Search(&key));
        if (he) {
            // walk list looking for |callback|... we cannot assume
            // that it will be there!

            for (nsResolveHostCallback* c: he->rec->mCallbacks) {
                if (c == callback) {
                    rec = he->rec;
                    c->remove();
                    break;
                }
            }
        }
    }

    // complete callback with the given status code; this would only be done if
    // the record was in the process of being resolved.
    if (rec) {
        callback->OnResolveHostComplete(this, rec, status);
    }
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
    else {
        LOG(("  Unable to find a thread for looking up host [%s%s%s].\n",
             LOG_HOST(rec->host, rec->netInterface)));
    }
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

    switch (nsHostRecord::GetPriority(rec->flags)) {
        case nsHostRecord::DNS_PRIORITY_HIGH:
            PR_APPEND_LINK(rec, &mHighQ);
            break;

        case nsHostRecord::DNS_PRIORITY_MEDIUM:
            PR_APPEND_LINK(rec, &mMediumQ);
            break;

        case nsHostRecord::DNS_PRIORITY_LOW:
            PR_APPEND_LINK(rec, &mLowQ);
            break;
    }
    mPendingCount++;

    rec->resolving = true;
    rec->onQueue = true;

    rv = ConditionallyCreateThread(rec);

    LOG (("  DNS thread counters: total=%d any-live=%d idle=%d pending=%d\n",
          static_cast<uint32_t>(mThreadCount),
          static_cast<uint32_t>(mActiveAnyThreadCount),
          static_cast<uint32_t>(mNumIdleThreads),
          static_cast<uint32_t>(mPendingCount)));

    return rv;
}

nsresult
nsHostResolver::ConditionallyRefreshRecord(nsHostRecord *rec, const char *host)
{
    if ((rec->CheckExpiration(TimeStamp::NowLoRes()) != nsHostRecord::EXP_VALID
            || rec->negative) && !rec->resolving) {
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

#if TTL_AVAILABLE
        #define SET_GET_TTL(var, val) \
            (var)->mGetTtl = sGetTtlEnabled && (val)
#else
        #define SET_GET_TTL(var, val)
#endif

        if (!PR_CLIST_IS_EMPTY(&mHighQ)) {
            DeQueue (mHighQ, result);
            SET_GET_TTL(*result, false);
            return true;
        }

        if (mActiveAnyThreadCount < HighThreadThreshold) {
            if (!PR_CLIST_IS_EMPTY(&mMediumQ)) {
                DeQueue (mMediumQ, result);
                mActiveAnyThreadCount++;
                (*result)->usingAnyThread = true;
                SET_GET_TTL(*result, true);
                return true;
            }

            if (!PR_CLIST_IS_EMPTY(&mLowQ)) {
                DeQueue (mLowQ, result);
                mActiveAnyThreadCount++;
                (*result)->usingAnyThread = true;
                SET_GET_TTL(*result, true);
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
    return false;
}

void
nsHostResolver::PrepareRecordExpiration(nsHostRecord* rec) const
{
    MOZ_ASSERT(((bool)rec->addr_info) != rec->negative);
    if (!rec->addr_info) {
        rec->SetExpiration(TimeStamp::NowLoRes(),
                           NEGATIVE_RECORD_LIFETIME, 0);
        LOG(("Caching host [%s%s%s] negative record for %u seconds.\n",
             LOG_HOST(rec->host, rec->netInterface),
             NEGATIVE_RECORD_LIFETIME));
        return;
    }

    unsigned int lifetime = mDefaultCacheLifetime;
    unsigned int grace = mDefaultGracePeriod;
#if TTL_AVAILABLE
    unsigned int ttl = mDefaultCacheLifetime;
    if (sGetTtlEnabled) {
        MutexAutoLock lock(rec->addr_info_lock);
        if (rec->addr_info && rec->addr_info->ttl != AddrInfo::NO_TTL_DATA) {
            ttl = rec->addr_info->ttl;
        }
        lifetime = ttl;
        grace = 0;
    }
#endif

    rec->SetExpiration(TimeStamp::NowLoRes(), lifetime, grace);
    LOG(("Caching host [%s%s%s] record for %u seconds (grace %d).",
         LOG_HOST(rec->host, rec->netInterface), lifetime, grace));
}

static bool
different_rrset(AddrInfo *rrset1, AddrInfo *rrset2)
{
    if (!rrset1 || !rrset2) {
        return true;
    }

    LOG(("different_rrset %s\n", rrset1->mHostName));
    nsTArray<NetAddr> orderedSet1;
    nsTArray<NetAddr> orderedSet2;

    for (NetAddrElement *element = rrset1->mAddresses.getFirst();
         element; element = element->getNext()) {
        if (LOG_ENABLED()) {
            char buf[128];
            NetAddrToString(&element->mAddress, buf, 128);
            LOG(("different_rrset add to set 1 %s\n", buf));
        }
        orderedSet1.InsertElementAt(orderedSet1.Length(), element->mAddress);
    }

    for (NetAddrElement *element = rrset2->mAddresses.getFirst();
         element; element = element->getNext()) {
        if (LOG_ENABLED()) {
            char buf[128];
            NetAddrToString(&element->mAddress, buf, 128);
            LOG(("different_rrset add to set 2 %s\n", buf));
        }
        orderedSet2.InsertElementAt(orderedSet2.Length(), element->mAddress);
    }

    if (orderedSet1.Length() != orderedSet2.Length()) {
        LOG(("different_rrset true due to length change\n"));
        return true;
    }
    orderedSet1.Sort();
    orderedSet2.Sort();

    for (uint32_t i = 0; i < orderedSet1.Length(); ++i) {
        if (!(orderedSet1[i] == orderedSet2[i])) {
            LOG(("different_rrset true due to content change\n"));
            return true;
        }
    }
    LOG(("different_rrset false\n"));
    return false;
}

//
// CompleteLookup() checks if the resolving should be redone and if so it
// returns LOOKUP_RESOLVEAGAIN, but only if 'status' is not NS_ERROR_ABORT.
// takes ownership of AddrInfo parameter
nsHostResolver::LookupStatus
nsHostResolver::CompleteLookup(nsHostRecord* rec, nsresult status, AddrInfo* newRRSet)
{
    // get the list of pending callbacks for this lookup, and notify
    // them that the lookup is complete.
    mozilla::LinkedList<RefPtr<nsResolveHostCallback>> cbs;

    {
        MutexAutoLock lock(mLock);

        if (rec->mResolveAgain && (status != NS_ERROR_ABORT)) {
            LOG(("nsHostResolver record %p resolve again due to flushcache\n", rec));
            rec->mResolveAgain = false;
            delete newRRSet;
            return LOOKUP_RESOLVEAGAIN;
        }

        // grab list of callbacks to notify
        cbs = mozilla::Move(rec->mCallbacks);

        // update record fields.  We might have a rec->addr_info already if a
        // previous lookup result expired and we're reresolving it..
        AddrInfo  *old_addr_info;
        {
            MutexAutoLock lock(rec->addr_info_lock);
            if (different_rrset(rec->addr_info, newRRSet)) {
                LOG(("nsHostResolver record %p new gencnt\n", rec));
                old_addr_info = rec->addr_info;
                rec->addr_info = newRRSet;
                rec->addr_info_gencnt++;
            } else {
                if (rec->addr_info && newRRSet) {
                    rec->addr_info->ttl = newRRSet->ttl;
                }
                old_addr_info = newRRSet;
            }
        }
        delete old_addr_info;

        rec->negative = !rec->addr_info;
        PrepareRecordExpiration(rec);
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
                mDB.Remove((nsHostKey *) head);

                if (!head->negative) {
                    // record the age of the entry upon eviction.
                    TimeDuration age = TimeStamp::NowLoRes() - head->mValidStart;
                    Telemetry::Accumulate(Telemetry::DNS_CLEANUP_AGE,
                                          static_cast<uint32_t>(age.ToSeconds() / 60));
                }

                // release reference to rec owned by mEvictionQ
                NS_RELEASE(head);
            }
#if TTL_AVAILABLE
            if (!rec->mGetTtl && !rec->resolving && sGetTtlEnabled) {
                LOG(("Issuing second async lookup for TTL for host [%s%s%s].",
                     LOG_HOST(rec->host, rec->netInterface)));
                rec->flags =
                  (rec->flags & ~RES_PRIORITY_MEDIUM) | RES_PRIORITY_LOW;
                DebugOnly<nsresult> rv = IssueLookup(rec);
                NS_WARNING_ASSERTION(
                    NS_SUCCEEDED(rv),
                    "Could not issue second async lookup for TTL.");
            }
#endif
        }
    }

    for (nsResolveHostCallback* c = cbs.getFirst(); c; c = c->removeAndGetNext()) {
        c->OnResolveHostComplete(this, rec, status);
    }

    NS_RELEASE(rec);

    return LOOKUP_OK;
}

void
nsHostResolver::CancelAsyncRequest(const char             *host,
                                   const OriginAttributes &aOriginAttributes,
                                   uint16_t                flags,
                                   uint16_t                af,
                                   const char             *netInterface,
                                   nsIDNSListener         *aListener,
                                   nsresult                status)

{
    MutexAutoLock lock(mLock);

    nsAutoCString originSuffix;
    aOriginAttributes.CreateSuffix(originSuffix);

    // Lookup the host record associated with host, flags & address family
    nsHostKey key = { host, flags, af, netInterface, originSuffix.get() };
    auto he = static_cast<nsHostDBEnt*>(mDB.Search(&key));
    if (he) {
        nsHostRecord* recPtr = nullptr;

        for (RefPtr<nsResolveHostCallback> c : he->rec->mCallbacks) {
            if (c->EqualsAsyncListener(aListener)) {
                c->remove();
                recPtr = he->rec;
                c->OnResolveHostComplete(this, recPtr, status);
                break;
            }
        }

        // If there are no more callbacks, remove the hash table entry
        if (recPtr && recPtr->mCallbacks.isEmpty()) {
            mDB.Remove((nsHostKey *)recPtr);
            // If record is on a Queue, remove it and then deref it
            if (recPtr->next != recPtr) {
                PR_REMOVE_LINK(recPtr);
                NS_RELEASE(recPtr);
            }
        }
    }
}

size_t
nsHostResolver::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const
{
    MutexAutoLock lock(mLock);

    size_t n = mallocSizeOf(this);

    n += mDB.ShallowSizeOfExcludingThis(mallocSizeOf);
    for (auto iter = mDB.ConstIter(); !iter.Done(); iter.Next()) {
        auto entry = static_cast<nsHostDBEnt*>(iter.Get());
        n += entry->rec->SizeOfIncludingThis(mallocSizeOf);
    }

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
    nsCString name = naming.GetNextThreadName("DNS Resolver");

    AUTO_PROFILER_REGISTER_THREAD(name.BeginReading());
    NS_SetCurrentThreadName(name.BeginReading());

#if defined(RES_RETRY_ON_FAILURE)
    nsResState rs;
#endif
    RefPtr<nsHostResolver> resolver = dont_AddRef((nsHostResolver *)arg);
    nsHostRecord *rec  = nullptr;
    AddrInfo *ai = nullptr;

    while (rec || resolver->GetHostToLookup(&rec)) {
        LOG(("DNS lookup thread - Calling getaddrinfo for host [%s%s%s].\n",
             LOG_HOST(rec->host, rec->netInterface)));

        TimeStamp startTime = TimeStamp::Now();
#if TTL_AVAILABLE
        bool getTtl = rec->mGetTtl;
#else
        bool getTtl = false;
#endif

        nsresult status = GetAddrInfo(rec->host, rec->af, rec->flags, rec->netInterface,
                                      &ai, getTtl);
#if defined(RES_RETRY_ON_FAILURE)
        if (NS_FAILED(status) && rs.Reset()) {
            status = GetAddrInfo(rec->host, rec->af, rec->flags, rec->netInterface, &ai,
                                 getTtl);
        }
#endif

        {   // obtain lock to check shutdown and manage inter-module telemetry
            MutexAutoLock lock(resolver->mLock);

            if (!resolver->mShutdown) {
                TimeDuration elapsed = TimeStamp::Now() - startTime;
                uint32_t millis = static_cast<uint32_t>(elapsed.ToMilliseconds());

                if (NS_SUCCEEDED(status)) {
                    Telemetry::HistogramID histogramID;
                    if (!rec->addr_info_gencnt) {
                        // Time for initial lookup.
                        histogramID = Telemetry::DNS_LOOKUP_TIME;
                    } else if (!getTtl) {
                        // Time for renewal; categorized by expiration strategy.
                        histogramID = Telemetry::DNS_RENEWAL_TIME;
                    } else {
                        // Time to get TTL; categorized by expiration strategy.
                        histogramID = Telemetry::DNS_RENEWAL_TIME_FOR_TTL;
                    }
                    Telemetry::Accumulate(histogramID, millis);
                } else {
                    Telemetry::Accumulate(Telemetry::DNS_FAILED_LOOKUP_TIME, millis);
                }
            }
        }

        // CompleteLookup may release "rec", long before we lose it.
        LOG(("DNS lookup thread - lookup completed for host [%s%s%s]: %s.\n",
             LOG_HOST(rec->host, rec->netInterface),
             ai ? "success" : "failure: unknown host"));

        if (LOOKUP_RESOLVEAGAIN == resolver->CompleteLookup(rec, status, ai)) {
            // leave 'rec' assigned and loop to make a renewed host resolve
            LOG(("DNS lookup thread - Re-resolving host [%s%s%s].\n",
                 LOG_HOST(rec->host, rec->netInterface)));
        } else {
            rec = nullptr;
        }
    }
    resolver->mThreadCount--;
    resolver = nullptr;
    LOG(("DNS lookup thread - queue empty, thread finished.\n"));
}

nsresult
nsHostResolver::Create(uint32_t maxCacheEntries,
                       uint32_t defaultCacheEntryLifetime,
                       uint32_t defaultGracePeriod,
                       nsHostResolver **result)
{
    auto *res = new nsHostResolver(maxCacheEntries, defaultCacheEntryLifetime,
                                             defaultGracePeriod);
    NS_ADDREF(res);

    nsresult rv = res->Init();
    if (NS_FAILED(rv))
        NS_RELEASE(res);

    *result = res;
    return rv;
}

void
nsHostResolver::GetDNSCacheEntries(nsTArray<DNSCacheEntries> *args)
{
    for (auto iter = mDB.Iter(); !iter.Done(); iter.Next()) {
        // We don't pay attention to address literals, only resolved domains.
        // Also require a host.
        auto entry = static_cast<nsHostDBEnt*>(iter.Get());
        nsHostRecord* rec = entry->rec;
        MOZ_ASSERT(rec, "rec should never be null here!");
        if (!rec || !rec->addr_info || !rec->host) {
            continue;
        }

        DNSCacheEntries info;
        info.hostname = rec->host;
        info.family = rec->af;
        info.netInterface = rec->netInterface;
        info.expiration =
            (int64_t)(rec->mValidEnd - TimeStamp::NowLoRes()).ToSeconds();
        if (info.expiration <= 0) {
            // We only need valid DNS cache entries
            continue;
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

        args->AppendElement(info);
    }
}
