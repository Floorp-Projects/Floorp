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
#include "nsIThreadManager.h"
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
#include "TRR.h"
#include "TRRService.h"

#include "mozilla/Atomics.h"
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

namespace mozilla {
namespace net {
LazyLogModule gHostResolverLog("nsHostResolver");
#define LOG(args) MOZ_LOG(mozilla::net::gHostResolverLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(mozilla::net::gHostResolverLog, mozilla::LogLevel::Debug)
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

bool
nsHostKey::operator==(const nsHostKey& other) const
{
    return host == other.host &&
        RES_KEY_FLAGS (flags) == RES_KEY_FLAGS(other.flags) &&
        af == other.af &&
        originSuffix == other.originSuffix;
}

PLDHashNumber
nsHostKey::Hash() const
{
    return AddToHash(HashString(host.get()), RES_KEY_FLAGS(flags), af,
                     HashString(originSuffix.get()));
}

size_t
nsHostKey::SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t n = 0;
    n += host.SizeOfExcludingThisIfUnshared(mallocSizeOf);
    n += originSuffix.SizeOfExcludingThisIfUnshared(mallocSizeOf);
    return n;
}

nsHostRecord::nsHostRecord(const nsHostKey& key)
    : nsHostKey(key)
    , addr_info_lock("nsHostRecord.addr_info_lock")
    , addr_info_gencnt(0)
    , addr_info(nullptr)
    , addr(nullptr)
    , negative(false)
    , mResolverMode(MODE_NATIVEONLY)
    , mFirstTRRresult(NS_OK)
    , mResolving(0)
    , mTRRSuccess(0)
    , mNativeSuccess(0)
    , mNative(false)
    , mTRRUsed(false)
    , mNativeUsed(false)
    , onQueue(false)
    , usingAnyThread(false)
    , mDoomed(false)
    , mDidCallbacks(false)
    , mGetTtl(false)
    , mResolveAgain(false)
    , mTrrAUsed(INIT)
    , mTrrAAAAUsed(INIT)
    , mTrrLock("nsHostRecord.mTrrLock")
    , mBlacklistedCount(0)
{
}

void
nsHostRecord::Cancel()
{
    MutexAutoLock trrlock(mTrrLock);
    if (mTrrA) {
        mTrrA->Cancel();
        mTrrA = nullptr;
    }
    if (mTrrAAAA) {
        mTrrAAAA->Cancel();
        mTrrAAAA = nullptr;
    }
}

void
nsHostRecord::Invalidate()
{
    mDoomed = true;
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

void
nsHostRecord::ResolveComplete()
{
    if (mNativeUsed) {
        if (mNativeSuccess) {
            uint32_t millis = static_cast<uint32_t>(mNativeDuration.ToMilliseconds());
            Telemetry::Accumulate(Telemetry::DNS_NATIVE_LOOKUP_TIME, millis);
        }
        AccumulateCategorical(mNativeSuccess ?
                              Telemetry::LABELS_DNS_LOOKUP_DISPOSITION::osOK :
                              Telemetry::LABELS_DNS_LOOKUP_DISPOSITION::osFail);
    }

    if (mTRRUsed) {
        if (mTRRSuccess) {
            uint32_t millis = static_cast<uint32_t>(mTrrDuration.ToMilliseconds());
            Telemetry::Accumulate(Telemetry::DNS_TRR_LOOKUP_TIME, millis);
        }
        AccumulateCategorical(mTRRSuccess ?
                              Telemetry::LABELS_DNS_LOOKUP_DISPOSITION::trrOK :
                              Telemetry::LABELS_DNS_LOOKUP_DISPOSITION::trrFail);

        if (mTrrAUsed == OK) {
            AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_DISPOSITION::trrAOK);
        } else if (mTrrAUsed == FAILED) {
            AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_DISPOSITION::trrAFail);
        }

        if (mTrrAAAAUsed == OK) {
            AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_DISPOSITION::trrAAAAOK);
        } else if (mTrrAAAAUsed == FAILED) {
            AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_DISPOSITION::trrAAAAFail);
        }
    }

    if (mTRRUsed && mNativeUsed && mNativeSuccess && mTRRSuccess) { // race or shadow!
        static const TimeDuration k50ms = TimeDuration::FromMilliseconds(50);
        if (mTrrDuration <= mNativeDuration) {
            AccumulateCategorical(((mNativeDuration - mTrrDuration) > k50ms) ?
                                  Telemetry::LABELS_DNS_TRR_RACE::TRRFasterBy50 :
                                  Telemetry::LABELS_DNS_TRR_RACE::TRRFaster);
            LOG(("nsHostRecord::Complete %s Dns Race: TRR\n", host.get()));
        } else {
            AccumulateCategorical(((mTrrDuration - mNativeDuration) > k50ms) ?
                                  Telemetry::LABELS_DNS_TRR_RACE::NativeFasterBy50 :
                                  Telemetry::LABELS_DNS_TRR_RACE::NativeFaster);
            LOG(("nsHostRecord::Complete %s Dns Race: NATIVE\n", host.get()));
        }
    }

    if (mTRRUsed && mNativeUsed) {
        // both were used, accumulate comparative success
        AccumulateCategorical(mNativeSuccess && mTRRSuccess?
                              Telemetry::LABELS_DNS_TRR_COMPARE::BothWorked :
                              ((mNativeSuccess ? Telemetry::LABELS_DNS_TRR_COMPARE::NativeWorked :
                                (mTRRSuccess ? Telemetry::LABELS_DNS_TRR_COMPARE::TRRWorked:
                                 Telemetry::LABELS_DNS_TRR_COMPARE::BothFailed))));
    }

    switch(mResolverMode) {
    case MODE_NATIVEONLY:
    case MODE_TRROFF:
        AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_ALGORITHM::nativeOnly);
        break;
    case MODE_PARALLEL:
        AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_ALGORITHM::trrRace);
        break;
    case MODE_TRRFIRST:
        AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_ALGORITHM::trrFirst);
        break;
    case MODE_TRRONLY:
        AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_ALGORITHM::trrOnly);
        break;
    case MODE_SHADOW:
        AccumulateCategorical(Telemetry::LABELS_DNS_LOOKUP_ALGORITHM::trrShadow);
        break;
    }

    if (mTRRUsed && !mTRRSuccess && mNativeSuccess && gTRRService) {
        gTRRService->TRRBlacklist(nsCString(host), pb, true);
    }
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
    LOG(("Checking blacklist for host [%s], host record [%p].\n",
         host.get(), this));

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
            LOG(("Address [%s] is blacklisted for host [%s].\n", buf, host.get()));
            return true;
        }
    }

    return false;
}

void
nsHostRecord::ReportUnusable(NetAddr *aAddress)
{
    // must call locked
    LOG(("Adding address to blacklist for host [%s], host record [%p]."
         "used trr=%d\n", host.get(), this, mTRRSuccess));

    ++mBlacklistedCount;

    if (negative)
        mDoomed = true;

    char buf[kIPv6CStrBufSize];
    if (NetAddrToString(aAddress, buf, sizeof(buf))) {
        LOG(("Successfully adding address [%s] to blacklist for host "
             "[%s].\n", buf, host.get()));
        mBlacklistedItems.AppendElement(nsCString(buf));
    }
}

void
nsHostRecord::ResetBlacklist()
{
    // must call locked
    LOG(("Resetting blacklist for host [%s], host record [%p].\n",
         host.get(), this));
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

    n += nsHostKey::SizeOfExcludingThis(mallocSizeOf);
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
    // no need to flush TRRed names, they're not resolved "locally"
    MutexAutoLock lock(addr_info_lock);
    if (addr_info && addr_info->IsTRR()) {
        return false;
    }
    if (mNative) {
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
    // Already resolved; not in a pending state; remove from cache
    return true;
}

//----------------------------------------------------------------------------

static const char kPrefGetTtl[] = "network.dns.get-ttl";
static const char kPrefNativeIsLocalhost[] = "network.dns.native-is-localhost";
static bool sGetTtlEnabled = false;
mozilla::Atomic<bool, mozilla::Relaxed> gNativeIsLocalhost;

static void DnsPrefChanged(const char* aPref, void* aClosure)
{
    MOZ_ASSERT(NS_IsMainThread(),
               "Should be getting pref changed notification on main thread!");

    DebugOnly<nsHostResolver*> self = static_cast<nsHostResolver*>(aClosure);
    MOZ_ASSERT(self);

    if (!strcmp(aPref, kPrefGetTtl)) {
#ifdef DNSQUERY_AVAILABLE
        sGetTtlEnabled = Preferences::GetBool(kPrefGetTtl);
#endif
    } else if (!strcmp(aPref, kPrefNativeIsLocalhost)) {
        gNativeIsLocalhost = Preferences::GetBool(kPrefNativeIsLocalhost);
    }
}

NS_IMPL_ISUPPORTS0(nsHostResolver)

nsHostResolver::nsHostResolver(uint32_t maxCacheEntries,
                               uint32_t defaultCacheEntryLifetime,
                               uint32_t defaultGracePeriod)
    : mMaxCacheEntries(maxCacheEntries)
    , mDefaultCacheLifetime(defaultCacheEntryLifetime)
    , mDefaultGracePeriod(defaultGracePeriod)
    , mLock("nsHostResolver.mLock")
    , mIdleThreadCV(mLock, "nsHostResolver.mIdleThreadCV")
    , mEvictionQSize(0)
    , mShutdown(true)
    , mNumIdleThreads(0)
    , mThreadCount(0)
    , mActiveAnyThreadCount(0)
    , mPendingCount(0)
{
    mCreationTime = PR_Now();

    mLongIdleTimeout  = TimeDuration::FromSeconds(LongIdleTimeoutSeconds);
    mShortIdleTimeout = TimeDuration::FromSeconds(ShortIdleTimeoutSeconds);
}

nsHostResolver::~nsHostResolver() = default;

nsresult
nsHostResolver::Init()
{
    MOZ_ASSERT(NS_IsMainThread());
    if (NS_FAILED(GetAddrInfoInit())) {
        return NS_ERROR_FAILURE;
    }

    LOG(("nsHostResolver::Init this=%p", this));

    mShutdown = false;

    // The preferences probably haven't been loaded from the disk yet, so we
    // need to register a callback that will set up the experiment once they
    // are. We also need to explicitly set a value for the props otherwise the
    // callback won't be called.
    {
        DebugOnly<nsresult> rv = Preferences::RegisterCallbackAndCall(
            &DnsPrefChanged, kPrefGetTtl, this);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "Could not register DNS TTL pref callback.");
        rv = Preferences::RegisterCallbackAndCall(
            &DnsPrefChanged, kPrefNativeIsLocalhost, this);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "Could not register DNS pref callback.");
    }

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
nsHostResolver::ClearPendingQueue(LinkedList<RefPtr<nsHostRecord>>& aPendingQ)
{
    // loop through pending queue, erroring out pending lookups.
    if (!aPendingQ.isEmpty()) {
        for (RefPtr<nsHostRecord> rec : aPendingQ) {
            rec->Cancel();
            CompleteLookup(rec, NS_ERROR_ABORT, nullptr, rec->pb);
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
    if (!mEvictionQ.isEmpty()) {
        for (RefPtr<nsHostRecord> rec : mEvictionQ) {
            rec->Cancel();
            mRecordDB.Remove(*static_cast<nsHostKey *>(rec));
        }
        mEvictionQ.clear();
    }

    // Refresh the cache entries that are resolving RIGHT now, remove the rest.
    for (auto iter = mRecordDB.Iter(); !iter.Done(); iter.Next()) {
        nsHostRecord* record = iter.UserData();
        // Try to remove the record, or mark it for refresh.
        if (record->RemoveOrRefresh()) {
            if (record->isInList()) {
                record->remove();
            }
            iter.Remove();
        }
    }
}

void
nsHostResolver::Shutdown()
{
    LOG(("Shutting down host resolver.\n"));

    {
        DebugOnly<nsresult> rv = Preferences::UnregisterCallback(
            &DnsPrefChanged, kPrefGetTtl, this);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "Could not unregister DNS TTL pref callback.");
    }

    LinkedList<RefPtr<nsHostRecord>> pendingQHigh, pendingQMed, pendingQLow, evictionQ;

    {
        MutexAutoLock lock(mLock);

        mShutdown = true;

        // Move queues to temporary lists.
        pendingQHigh = std::move(mHighQ);
        pendingQMed = std::move(mMediumQ);
        pendingQLow = std::move(mLowQ);
        evictionQ = std::move(mEvictionQ);

        mEvictionQSize = 0;
        mPendingCount = 0;

        if (mNumIdleThreads)
            mIdleThreadCV.NotifyAll();

        // empty host database
        mRecordDB.Clear();
    }

    ClearPendingQueue(pendingQHigh);
    ClearPendingQueue(pendingQMed);
    ClearPendingQueue(pendingQLow);

    if (!evictionQ.isEmpty()) {
        for (RefPtr<nsHostRecord> rec : evictionQ) {
            rec->Cancel();
        }
    }

    pendingQHigh.clear();
    pendingQMed.clear();
    pendingQLow.clear();
    evictionQ.clear();

    for (auto iter = mRecordDB.Iter(); !iter.Done(); iter.Next()) {
        iter.UserData()->Cancel();
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

nsresult
nsHostResolver::GetHostRecord(const nsACString &host,
                              uint16_t flags, uint16_t af, bool pb,
                              const nsCString &originSuffix,
                              nsHostRecord **result)
{
    MutexAutoLock lock(mLock);
    nsHostKey key(host, flags, af, pb, originSuffix);

    RefPtr<nsHostRecord>& entry = mRecordDB.GetOrInsert(key);
    if (!entry) {
        entry = new nsHostRecord(key);
    }

    RefPtr<nsHostRecord> rec = entry;
    if (rec->addr) {
        return NS_ERROR_FAILURE;
    }
    if (rec->mResolving) {
        return NS_ERROR_FAILURE;
    }
    *result = rec.forget().take();
    return NS_OK;
}

nsresult
nsHostResolver::ResolveHost(const nsACString &aHost,
                            const OriginAttributes &aOriginAttributes,
                            uint16_t                flags,
                            uint16_t                af,
                            nsResolveHostCallback  *aCallback)
{
    nsAutoCString host(aHost);
    NS_ENSURE_TRUE(!host.IsEmpty(), NS_ERROR_UNEXPECTED);

    LOG(("Resolving host [%s]%s%s.\n", host.get(),
         flags & RES_BYPASS_CACHE ? " - bypassing cache" : "",
         flags & RES_REFRESH_CACHE ? " - refresh cache" : ""));

    // ensure that we are working with a valid hostname before proceeding.  see
    // bug 304904 for details.
    if (!net_IsValidHostName(host))
        return NS_ERROR_UNKNOWN_HOST;

    RefPtr<nsResolveHostCallback> callback(aCallback);
    // if result is set inside the lock, then we need to issue the
    // callback before returning.
    RefPtr<nsHostRecord> result;
    nsresult status = NS_OK, rv = NS_OK;
    {
        MutexAutoLock lock(mLock);

        if (mShutdown) {
            rv = NS_ERROR_NOT_INITIALIZED;
        } else {
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

            nsHostKey key(host, flags, af,
                          (aOriginAttributes.mPrivateBrowsingId > 0),
                          originSuffix);
            RefPtr<nsHostRecord>& entry = mRecordDB.GetOrInsert(key);
            if (!entry) {
                entry = new nsHostRecord(key);
            }

            RefPtr<nsHostRecord> rec = entry;
            MOZ_ASSERT(rec, "Record should not be null");
            if (!(flags & RES_BYPASS_CACHE) &&
                     rec->HasUsableResult(TimeStamp::NowLoRes(), flags)) {
                LOG(("  Using cached record for host [%s].\n", host.get()));
                // put reference to host record on stack...
                result = rec;
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_HIT);

                // For entries that are in the grace period
                // or all cached negative entries, use the cache but start a new
                // lookup in the background
                ConditionallyRefreshRecord(rec, host);

                if (rec->negative) {
                    LOG(("  Negative cache entry for host [%s].\n", host.get()));
                    Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                          METHOD_NEGATIVE_HIT);
                    status = NS_ERROR_UNKNOWN_HOST;
                }
            } else if (rec->addr) {
                // if the host name is an IP address literal and has been parsed,
                // go ahead and use it.
                LOG(("  Using cached address for IP Literal [%s].\n", host.get()));
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                      METHOD_LITERAL);
                result = rec;
            } else if (PR_StringToNetAddr(host.get(), &tempAddr) == PR_SUCCESS) {
                // try parsing the host name as an IP address literal to short
                // circuit full host resolution.  (this is necessary on some
                // platforms like Win9x.  see bug 219376 for more details.)
                LOG(("  Host is IP Literal [%s].\n", host.get()));
                // ok, just copy the result into the host record, and be done
                // with it! ;-)
                rec->addr = MakeUnique<NetAddr>();
                PRNetAddrToNetAddr(&tempAddr, rec->addr.get());
                // put reference to host record on stack...
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                      METHOD_LITERAL);
                result = rec;
            } else if (mPendingCount >= MAX_NON_PRIORITY_REQUESTS &&
                       !IsHighPriority(flags) &&
                       !rec->mResolving) {
                LOG(("  Lookup queue full: dropping %s priority request for "
                     "host [%s].\n",
                     IsMediumPriority(flags) ? "medium" : "low", host.get()));
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                      METHOD_OVERFLOW);
                // This is a lower priority request and we are swamped, so refuse it.
                rv = NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
            } else if (flags & RES_OFFLINE) {
                LOG(("  Offline request for host [%s]; ignoring.\n", host.get()));
                rv = NS_ERROR_OFFLINE;
            } else if (!rec->mResolving) {
                // If this is an IPV4 or IPV6 specific request, check if there is
                // an AF_UNSPEC entry we can use. Otherwise, hit the resolver...

                if (!(flags & RES_BYPASS_CACHE) &&
                    ((af == PR_AF_INET) || (af == PR_AF_INET6))) {
                    // First, search for an entry with AF_UNSPEC
                    const nsHostKey unspecKey(host, flags, PR_AF_UNSPEC,
                                              (aOriginAttributes.mPrivateBrowsingId > 0),
                                              originSuffix);
                    RefPtr<nsHostRecord> unspecRec = mRecordDB.Get(unspecKey);

                    TimeStamp now = TimeStamp::NowLoRes();
                    if (unspecRec && unspecRec->HasUsableResult(now, flags)) {

                        MOZ_ASSERT(unspecRec->addr_info || unspecRec->negative,
                                   "Entry should be resolved or negative.");

                        LOG(("  Trying AF_UNSPEC entry for host [%s] af: %s.\n", host.get(),
                             (af == PR_AF_INET) ? "AF_INET" : "AF_INET6"));

                        // We need to lock in case any other thread is reading
                        // addr_info.
                        MutexAutoLock lock(rec->addr_info_lock);

                        // XXX: note that this actually leaks addr_info.
                        // For some reason, freeing the memory causes a crash in
                        // nsDNSRecord::GetNextAddr - see bug 1422173
                        rec->addr_info = nullptr;
                        if (unspecRec->negative) {
                            rec->negative = unspecRec->negative;
                            rec->CopyExpirationTimesAndFlagsFrom(unspecRec);
                        } else if (unspecRec->addr_info) {
                            // Search for any valid address in the AF_UNSPEC entry
                            // in the cache (not blacklisted and from the right
                            // family).
                            NetAddrElement *addrIter =
                                unspecRec->addr_info->mAddresses.getFirst();
                            while (addrIter) {
                                if ((af == addrIter->mAddress.inet.family) &&
                                     !unspecRec->Blacklisted(&addrIter->mAddress)) {
                                    if (!rec->addr_info) {
                                        rec->addr_info = new AddrInfo(
                                            unspecRec->addr_info->mHostName,
                                            unspecRec->addr_info->mCanonicalName,
                                            unspecRec->addr_info->IsTRR()
                                          );
                                        rec->CopyExpirationTimesAndFlagsFrom(unspecRec);
                                    }
                                    rec->addr_info->AddAddress(
                                        new NetAddrElement(*addrIter));
                                }
                                addrIter = addrIter->getNext();
                            }
                        }
                        // Now check if we have a new record.
                        if (rec->HasUsableResult(now, flags)) {
                            result = rec;
                            if (rec->negative) {
                                status = NS_ERROR_UNKNOWN_HOST;
                            }
                            Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                                  METHOD_HIT);
                            ConditionallyRefreshRecord(rec, host);
                        } else if (af == PR_AF_INET6) {
                            // For AF_INET6, a new lookup means another AF_UNSPEC
                            // lookup. We have already iterated through the
                            // AF_UNSPEC addresses, so we mark this record as
                            // negative.
                            LOG(("  No AF_INET6 in AF_UNSPEC entry: "
                                 "host [%s] unknown host.", host.get()));
                            result = rec;
                            rec->negative = true;
                            status = NS_ERROR_UNKNOWN_HOST;
                            Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                                  METHOD_NEGATIVE_HIT);
                        }
                    }
                }
                // If no valid address was found in the cache or this is an
                // AF_UNSPEC request, then start a new lookup.
                if (!result) {
                    LOG(("  No usable address in cache for host [%s].", host.get()));

                    if (flags & RES_REFRESH_CACHE) {
                        rec->Invalidate();
                    }

                    // Add callback to the list of pending callbacks.
                    rec->mCallbacks.insertBack(callback);
                    rec->flags = flags;
                    rv = NameLookup(rec);
                    Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                          METHOD_NETWORK_FIRST);
                    if (NS_FAILED(rv) && callback->isInList()) {
                        callback->remove();
                    } else {
                        LOG(("  DNS lookup for host [%s] blocking "
                             "pending 'getaddrinfo' query: callback [%p]",
                             host.get(), callback.get()));
                    }
                }
            } else if (rec->mDidCallbacks) {
                // record is still pending more (TRR) data; make the callback
                // at once
                result = rec;
                // make it count as a hit
                Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_HIT);
                LOG(("  Host [%s] re-using early TRR resolve data\n", host.get()));
            } else {
                LOG(("  Host [%s] is being resolved. Appending callback "
                     "[%p].", host.get(), callback.get()));

                rec->mCallbacks.insertBack(callback);
                if (rec->onQueue) {
                    Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                          METHOD_NETWORK_SHARED);

                    // Consider the case where we are on a pending queue of
                    // lower priority than the request is being made at.
                    // In that case we should upgrade to the higher queue.

                    if (IsHighPriority(flags) &&
                        !IsHighPriority(rec->flags)) {
                        // Move from (low|med) to high.
                        NS_ASSERTION(rec->onQueue, "Moving Host Record Not Currently Queued");
                        rec->remove();
                        mHighQ.insertBack(rec);
                        rec->flags = flags;
                        ConditionallyCreateThread(rec);
                    } else if (IsMediumPriority(flags) &&
                               IsLowPriority(rec->flags)) {
                        // Move from low to med.
                        NS_ASSERTION(rec->onQueue, "Moving Host Record Not Currently Queued");
                        rec->remove();
                        mMediumQ.insertBack(rec);
                        rec->flags = flags;
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
nsHostResolver::DetachCallback(const nsACString &host,
                               const OriginAttributes &aOriginAttributes,
                               uint16_t                flags,
                               uint16_t                af,
                               nsResolveHostCallback  *aCallback,
                               nsresult                status)
{
    RefPtr<nsHostRecord> rec;
    RefPtr<nsResolveHostCallback> callback(aCallback);

    {
        MutexAutoLock lock(mLock);

        nsAutoCString originSuffix;
        aOriginAttributes.CreateSuffix(originSuffix);

        nsHostKey key(host, flags, af,
                      (aOriginAttributes.mPrivateBrowsingId > 0),
                      originSuffix);
        RefPtr<nsHostRecord> entry = mRecordDB.Get(key);
        if (entry) {
            // walk list looking for |callback|... we cannot assume
            // that it will be there!

            for (nsResolveHostCallback* c: entry->mCallbacks) {
                if (c == callback) {
                    rec = entry;
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
        static nsThreadPoolNaming naming;
        nsCString name = naming.GetNextThreadName("DNS Resolver");

        // dispatch new worker thread
        nsCOMPtr<nsIThread> thread;
        nsresult rv = NS_NewNamedThread(name, getter_AddRefs(thread), nullptr,
                                        nsIThreadManager::kThreadPoolStackSize);
        if (NS_WARN_IF(NS_FAILED(rv)) || !thread) {
            return rv;
        }

        nsCOMPtr<nsIRunnable> event =
            mozilla::NewRunnableMethod("nsHostResolver::ThreadFunc",
                                       this,
                                       &nsHostResolver::ThreadFunc);
        mThreadCount++;
        rv = thread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
        if (NS_FAILED(rv)) {
            mThreadCount--;
        }
    }
    else {
        LOG(("  Unable to find a thread for looking up host [%s].\n", rec->host.get()));
    }
    return NS_OK;
}

// make sure the mTrrLock is held when this is used!
#define TRROutstanding() ((rec->mTrrA || rec->mTrrAAAA))

nsresult
nsHostResolver::TrrLookup_unlocked(nsHostRecord *rec, TRR *pushedTRR)
{
    MutexAutoLock lock(mLock);
    return TrrLookup(rec, pushedTRR);
}

// returns error if no TRR resolve is issued
// it is impt this is not called while a native lookup is going on
nsresult
nsHostResolver::TrrLookup(nsHostRecord *aRec, TRR *pushedTRR)
{
    RefPtr<nsHostRecord> rec(aRec);
    mLock.AssertCurrentThreadOwns();
#ifdef DEBUG
    {
        MutexAutoLock trrlock(rec->mTrrLock);
        MOZ_ASSERT(!TRROutstanding());
    }
#endif
    MOZ_ASSERT(!rec->mResolving);

    if (!gTRRService || !gTRRService->Enabled()) {
        LOG(("TrrLookup:: %s service not enabled\n", rec->host.get()));
        return NS_ERROR_UNKNOWN_HOST;
    }

    if (rec->isInList()) {
        // we're already on the eviction queue. This is a renewal
        MOZ_ASSERT(mEvictionQSize);
        AssertOnQ(rec, mEvictionQ);

        rec->remove();
        mEvictionQSize--;
    }

    rec->mTRRSuccess = 0; // bump for each successful TRR response
    rec->mTrrAUsed = nsHostRecord::INIT;
    rec->mTrrAAAAUsed = nsHostRecord::INIT;
    rec->mTrrStart = TimeStamp::Now();
    rec->mTRRUsed = true; // this record gets TRR treatment

    // If asking for AF_UNSPEC, issue both A and AAAA.
    // If asking for AF_INET6 or AF_INET, do only that single type
    enum TrrType rectype = (rec->af == AF_INET6)? TRRTYPE_AAAA : TRRTYPE_A;
    if (pushedTRR) {
        rectype = pushedTRR->Type();
    }
    bool sendAgain;

    bool madeQuery = false;
    do {
        sendAgain = false;
        if ((TRRTYPE_AAAA == rectype) && gTRRService && gTRRService->DisableIPv6()) {
            break;
        }
        LOG(("TRR Resolve %s type %d\n", rec->host.get(), (int)rectype));
        RefPtr<TRR> trr;
        MutexAutoLock trrlock(rec->mTrrLock);
        trr = pushedTRR ? pushedTRR : new TRR(this, rec, rectype);
        if (pushedTRR || NS_SUCCEEDED(NS_DispatchToMainThread(trr))) {
            rec->mResolving++;
            if (rectype == TRRTYPE_A) {
                MOZ_ASSERT(!rec->mTrrA);
                rec->mTrrA = trr;
                rec->mTrrAUsed = nsHostRecord::STARTED;
            } else if (rectype == TRRTYPE_AAAA) {
                MOZ_ASSERT(!rec->mTrrAAAA);
                rec->mTrrAAAA = trr;
                rec->mTrrAAAAUsed = nsHostRecord::STARTED;
            } else {
                LOG(("TrrLookup called with bad type set: %d\n", rectype));
                MOZ_ASSERT(0);
            }
            madeQuery = true;
            if (!pushedTRR && (rec->af == AF_UNSPEC) && (rectype == TRRTYPE_A)) {
                rectype = TRRTYPE_AAAA;
                sendAgain = true;
            }
        }
    } while (sendAgain);

    return madeQuery ? NS_OK : NS_ERROR_UNKNOWN_HOST;
}

void
nsHostResolver::AssertOnQ(nsHostRecord *rec, LinkedList<RefPtr<nsHostRecord>>& q)
{
#ifdef DEBUG
    MOZ_ASSERT(!q.isEmpty());
    MOZ_ASSERT(rec->isInList());
    for (RefPtr<nsHostRecord> r: q) {
        if (rec == r) {
            return;
        }
    }
    MOZ_ASSERT(false, "Did not find element");
#endif
}

nsresult
nsHostResolver::NativeLookup(nsHostRecord *aRec)
{
    mLock.AssertCurrentThreadOwns();
    RefPtr<nsHostRecord> rec(aRec);

    rec->mNativeStart = TimeStamp::Now();

    // Add rec to one of the pending queues, possibly removing it from mEvictionQ.
    if (rec->isInList()) {
        MOZ_ASSERT(mEvictionQSize);
        AssertOnQ(rec, mEvictionQ);
        rec->remove(); // was on the eviction queue
        mEvictionQSize--;
    }

    switch (nsHostRecord::GetPriority(rec->flags)) {
        case nsHostRecord::DNS_PRIORITY_HIGH:
            mHighQ.insertBack(rec);
            break;

        case nsHostRecord::DNS_PRIORITY_MEDIUM:
            mMediumQ.insertBack(rec);
            break;

        case nsHostRecord::DNS_PRIORITY_LOW:
            mLowQ.insertBack(rec);
            break;
    }
    mPendingCount++;

    rec->mNative = true;
    rec->mNativeUsed = true;
    rec->onQueue = true;
    rec->mResolving++;

    nsresult rv = ConditionallyCreateThread(rec);

    LOG (("  DNS thread counters: total=%d any-live=%d idle=%d pending=%d\n",
          static_cast<uint32_t>(mThreadCount),
          static_cast<uint32_t>(mActiveAnyThreadCount),
          static_cast<uint32_t>(mNumIdleThreads),
          static_cast<uint32_t>(mPendingCount)));

    return rv;
}

ResolverMode
nsHostResolver::Mode()
{
    if (gTRRService) {
        return static_cast<ResolverMode>(gTRRService->Mode());
    }

    return MODE_NATIVEONLY;
}

// Kick-off a name resolve operation, using native resolver and/or TRR
nsresult
nsHostResolver::NameLookup(nsHostRecord *rec)
{
    nsresult rv = NS_ERROR_UNKNOWN_HOST;
    if (rec->mResolving) {
        LOG(("NameLookup %s while already resolving\n", rec->host.get()));
        return NS_OK;
    }

    ResolverMode mode = rec->mResolverMode = Mode();

    rec->mNativeUsed = false;
    rec->mTRRUsed = false;
    rec->mNativeSuccess = false;
    rec->mTRRSuccess = 0;
    rec->mDidCallbacks = false;
    rec->mTrrAUsed = nsHostRecord::INIT;
    rec->mTrrAAAAUsed = nsHostRecord::INIT;

    if (rec->flags & RES_DISABLE_TRR) {
        if (mode == MODE_TRRONLY) {
            return rv;
        }
        mode = MODE_NATIVEONLY;
    }

    if (!TRR_DISABLED(mode)) {
        rv = TrrLookup(rec);
    }

    if ((mode == MODE_PARALLEL) ||
        TRR_DISABLED(mode) ||
        (mode == MODE_SHADOW) ||
        ((mode == MODE_TRRFIRST) && NS_FAILED(rv))) {
        rv = NativeLookup(rec);
    }

    return rv;
}

nsresult
nsHostResolver::ConditionallyRefreshRecord(nsHostRecord *rec, const nsACString &host)
{
    if ((rec->CheckExpiration(TimeStamp::NowLoRes()) != nsHostRecord::EXP_VALID
            || rec->negative) && !rec->mResolving) {
        LOG(("  Using %s cache entry for host [%s] but starting async renewal.",
            rec->negative ? "negative" :"positive", host.BeginReading()));
        NameLookup(rec);

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
nsHostResolver::DeQueue(LinkedList<RefPtr<nsHostRecord>>& aQ, nsHostRecord **aResult)
{
    RefPtr<nsHostRecord> rec = aQ.popFirst();
    mPendingCount--;
    rec.forget(aResult);
    (*aResult)->onQueue = false;
}

bool
nsHostResolver::GetHostToLookup(nsHostRecord **result)
{
    bool timedOut = false;
    TimeDuration timeout;
    TimeStamp epoch, now;

    MutexAutoLock lock(mLock);

    timeout = (mNumIdleThreads >= HighThreadThreshold) ? mShortIdleTimeout : mLongIdleTimeout;
    epoch = TimeStamp::Now();

    while (!mShutdown) {
        // remove next record from Q; hand over owning reference. Check high, then med, then low

#define SET_GET_TTL(var, val) (var)->mGetTtl = sGetTtlEnabled && (val)

        if (!mHighQ.isEmpty()) {
            DeQueue (mHighQ, result);
            SET_GET_TTL(*result, false);
            return true;
        }

        if (mActiveAnyThreadCount < HighThreadThreshold) {
            if (!mMediumQ.isEmpty()) {
                DeQueue (mMediumQ, result);
                mActiveAnyThreadCount++;
                (*result)->usingAnyThread = true;
                SET_GET_TTL(*result, true);
                return true;
            }

            if (!mLowQ.isEmpty()) {
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

        now = TimeStamp::Now();

        if (now - epoch >= timeout) {
            timedOut = true;
        } else {
            // It is possible that CondVar::Wait() was interrupted and returned
            // early, in which case we will loop back and re-enter it. In that
            // case we want to do so with the new timeout reduced to reflect
            // time already spent waiting.
            timeout -= now - epoch;
            epoch = now;
        }
    }

    // tell thread to exit...
    return false;
}

void
nsHostResolver::PrepareRecordExpiration(nsHostRecord* rec) const
{
    // NOTE: rec->addr_info_lock is already held by parent
    MOZ_ASSERT(((bool)rec->addr_info) != rec->negative);
    mLock.AssertCurrentThreadOwns();
    if (!rec->addr_info) {
        rec->SetExpiration(TimeStamp::NowLoRes(),
                           NEGATIVE_RECORD_LIFETIME, 0);
        LOG(("Caching host [%s] negative record for %u seconds.\n",
             rec->host.get(), NEGATIVE_RECORD_LIFETIME));
        return;
    }

    unsigned int lifetime = mDefaultCacheLifetime;
    unsigned int grace = mDefaultGracePeriod;

    unsigned int ttl = mDefaultCacheLifetime;
    if (sGetTtlEnabled || rec->addr_info->IsTRR()) {
        if (rec->addr_info && rec->addr_info->ttl != AddrInfo::NO_TTL_DATA) {
            ttl = rec->addr_info->ttl;
        }
        lifetime = ttl;
        grace = 0;
    }

    rec->SetExpiration(TimeStamp::NowLoRes(), lifetime, grace);
    LOG(("Caching host [%s] record for %u seconds (grace %d).",
         rec->host.get(), lifetime, grace));
}

static nsresult
merge_rrset(AddrInfo *rrto, AddrInfo *rrfrom)
{
    if (!rrto || !rrfrom) {
        return NS_ERROR_NULL_POINTER;
    }
    NetAddrElement *element;
    while ((element = rrfrom->mAddresses.getFirst())) {
        element->remove(); // unlist from old
        rrto->AddAddress(element); // enlist on new
    }
    return NS_OK;
}

static bool
different_rrset(AddrInfo *rrset1, AddrInfo *rrset2)
{
    if (!rrset1 || !rrset2) {
        return true;
    }

    LOG(("different_rrset %s\n", rrset1->mHostName.get()));
    nsTArray<NetAddr> orderedSet1;
    nsTArray<NetAddr> orderedSet2;

    if (rrset1->IsTRR() != rrset2->IsTRR()) {
        return true;
    }

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
nsHostResolver::CompleteLookup(nsHostRecord* rec, nsresult status, AddrInfo* aNewRRSet, bool pb)
{
    MutexAutoLock lock(mLock);
    MOZ_ASSERT(rec);
    MOZ_ASSERT(rec->pb == pb);

    // newRRSet needs to be taken into the hostrecord (which will then own it)
    // or deleted on early return.
    nsAutoPtr<AddrInfo> newRRSet(aNewRRSet);

    bool trrResult = newRRSet && newRRSet->IsTRR();

    if (rec->mResolveAgain && (status != NS_ERROR_ABORT) && !trrResult) {
        LOG(("nsHostResolver record %p resolve again due to flushcache\n", rec));
        rec->mResolveAgain = false;
        return LOOKUP_RESOLVEAGAIN;
    }

    MOZ_ASSERT(rec->mResolving);
    rec->mResolving--;
    LOG(("nsHostResolver::CompleteLookup %s %p %X trr=%d stillResolving=%d\n",
         rec->host.get(), aNewRRSet, (unsigned int)status,
         aNewRRSet ? aNewRRSet->IsTRR() : 0, rec->mResolving));

    if (trrResult) {
        MutexAutoLock trrlock(rec->mTrrLock);
        LOG(("TRR lookup Complete (%d) %s %s\n",
             newRRSet->IsTRR(), newRRSet->mHostName.get(),
             NS_SUCCEEDED(status) ? "OK" : "FAILED"));
        MOZ_ASSERT(TRROutstanding());
        if (newRRSet->IsTRR() == TRRTYPE_A) {
            MOZ_ASSERT(rec->mTrrA);
            rec->mTrrA = nullptr;
            rec->mTrrAUsed = NS_SUCCEEDED(status) ? nsHostRecord::OK : nsHostRecord::FAILED;
        } else if (newRRSet->IsTRR() == TRRTYPE_AAAA) {
            MOZ_ASSERT(rec->mTrrAAAA);
            rec->mTrrAAAA = nullptr;
            rec->mTrrAAAAUsed = NS_SUCCEEDED(status) ? nsHostRecord::OK : nsHostRecord::FAILED;
        } else {
            MOZ_ASSERT(0);
        }

        if (NS_SUCCEEDED(status)) {
            rec->mTRRSuccess++;
            if (rec->mTRRSuccess == 1) {
                // Store the duration on first succesful TRR response.  We
                // don't know that there will be a second response nor can we
                // tell which of two has useful data, especially in
                // MODE_SHADOW where the actual results are discarded.
                rec->mTrrDuration = TimeStamp::Now() - rec->mTrrStart;
            }
        }
        if (TRROutstanding()) {
            rec->mFirstTRRresult = status;
            if (NS_FAILED(status)) {
                return LOOKUP_OK; // wait for outstanding
            }

            // There's another TRR complete pending. Wait for it and keep
            // this RRset around until then.
            MOZ_ASSERT(!rec->mFirstTRR && newRRSet);
            rec->mFirstTRR = newRRSet; // autoPtr.swap()
            MOZ_ASSERT(rec->mFirstTRR && !newRRSet);

            if (rec->mDidCallbacks || rec->mResolverMode == MODE_SHADOW) {
                return LOOKUP_OK;
            }

            if (rec->mTrrA && (!gTRRService || !gTRRService->EarlyAAAA())) {
                // This is an early AAAA with a pending A response. Allowed
                // only by pref.
                LOG(("CompleteLookup: avoiding early use of TRR AAAA!\n"));
                return LOOKUP_OK;
            }

            // we can do some callbacks with this partial result which requires
            // a deep copy
            newRRSet = new AddrInfo(rec->mFirstTRR);
            MOZ_ASSERT(rec->mFirstTRR && newRRSet);

        } else {
            // no more outstanding TRRs
            // If mFirstTRR is set, merge those addresses into current set!
            if (rec->mFirstTRR) {
                if (NS_SUCCEEDED(status)) {
                    merge_rrset(newRRSet, rec->mFirstTRR);
                }
                else {
                    newRRSet = rec->mFirstTRR; // transfers
                }
                rec->mFirstTRR = nullptr;
            }

            if (NS_FAILED(rec->mFirstTRRresult) &&
                NS_FAILED(status) &&
                (rec->mFirstTRRresult != NS_ERROR_UNKNOWN_HOST) &&
                (status != NS_ERROR_UNKNOWN_HOST)) {
                // the errors are not failed resolves, that means
                // something else failed, consider this as *TRR not used*
                // for actually trying to resolve the host
                rec->mTRRUsed = false;
            }

            if (!rec->mTRRSuccess) {
                // no TRR success
                newRRSet = nullptr;
                status = NS_ERROR_UNKNOWN_HOST;
            }

            if (!rec->mTRRSuccess && rec->mResolverMode == MODE_TRRFIRST) {
                MOZ_ASSERT(!rec->mResolving);
                NativeLookup(rec);
                MOZ_ASSERT(rec->mResolving);
                return LOOKUP_OK;
            }

            // continue
        }

    } else { // native resolve completed
        if (rec->usingAnyThread) {
            mActiveAnyThreadCount--;
            rec->usingAnyThread = false;
        }

        rec->mNative = false;
        rec->mNativeSuccess = newRRSet ? true : false;
        if (rec->mNativeSuccess) {
            rec->mNativeDuration = TimeStamp::Now() - rec->mNativeStart;
        }
    }

    // update record fields.  We might have a rec->addr_info already if a
    // previous lookup result expired and we're reresolving it or we get
    // a late second TRR response.
    // note that we don't update the addr_info if this is trr shadow results
    if (!mShutdown &&
        !(trrResult && rec->mResolverMode == MODE_SHADOW)) {
        MutexAutoLock lock(rec->addr_info_lock);
        nsAutoPtr<AddrInfo> old_addr_info;
        if (different_rrset(rec->addr_info, newRRSet)) {
            LOG(("nsHostResolver record %p new gencnt\n", rec));
            old_addr_info = rec->addr_info;
            rec->addr_info = newRRSet.forget();
            rec->addr_info_gencnt++;
        } else {
            if (rec->addr_info && newRRSet) {
                rec->addr_info->ttl = newRRSet->ttl;
            }
            old_addr_info = newRRSet.forget();
        }
        rec->negative = !rec->addr_info;
        PrepareRecordExpiration(rec);
    }

    bool doCallbacks = true;

    if (trrResult && (rec->mResolverMode == MODE_SHADOW) && !rec->mDidCallbacks) {
        // don't report result based only on suppressed TRR info
        doCallbacks = false;
        LOG(("nsHostResolver Suppressing TRR %s because it is first shadow result\n",
             rec->host.get()));
    } else if(trrResult && rec->mDidCallbacks) {
        // already callback'ed on the first TRR response
        LOG(("nsHostResolver Suppressing callback for second TRR response for %s\n",
             rec->host.get()));
        doCallbacks = false;
    }


    if (LOG_ENABLED()) {
        MutexAutoLock lock(rec->addr_info_lock);
        NetAddrElement *element;
        if (rec->addr_info) {
            for (element = rec->addr_info->mAddresses.getFirst();
                 element; element = element->getNext()) {
                char buf[128];
                NetAddrToString(&element->mAddress, buf, sizeof(buf));
                LOG(("CompleteLookup: %s has %s\n", rec->host.get(), buf));
            }
        } else {
            LOG(("CompleteLookup: %s has NO address\n", rec->host.get()));
        }
    }

    if (doCallbacks) {
        // get the list of pending callbacks for this lookup, and notify
        // them that the lookup is complete.
        mozilla::LinkedList<RefPtr<nsResolveHostCallback>> cbs = std::move(rec->mCallbacks);

        LOG(("nsHostResolver record %p calling back dns users\n", rec));

        for (nsResolveHostCallback* c = cbs.getFirst(); c; c = c->removeAndGetNext()) {
            c->OnResolveHostComplete(this, rec, status);
        }
        rec->mDidCallbacks = true;
    }

    if (!rec->mResolving && !mShutdown) {
        rec->ResolveComplete();

        // add to mEvictionQ
        MOZ_ASSERT(!rec->isInList());
        mEvictionQ.insertBack(rec);
        if (mEvictionQSize < mMaxCacheEntries) {
            mEvictionQSize++;
        } else {
            // remove first element on mEvictionQ
            RefPtr<nsHostRecord> head = mEvictionQ.popFirst();
            mRecordDB.Remove(*static_cast<nsHostKey *>(head.get()));

            if (!head->negative) {
                // record the age of the entry upon eviction.
                TimeDuration age = TimeStamp::NowLoRes() - head->mValidStart;
                Telemetry::Accumulate(Telemetry::DNS_CLEANUP_AGE,
                                      static_cast<uint32_t>(age.ToSeconds() / 60));
                if (head->CheckExpiration(TimeStamp::Now()) !=
                    nsHostRecord::EXP_EXPIRED) {
                  Telemetry::Accumulate(Telemetry::DNS_PREMATURE_EVICTION,
                                        static_cast<uint32_t>(age.ToSeconds() / 60));
                }
            }
        }
    }

#ifdef DNSQUERY_AVAILABLE
    // Unless the result is from TRR, resolve again to get TTL
    bool fromTRR = false;
    {
        MutexAutoLock lock(rec->addr_info_lock);
        if(rec->addr_info && rec->addr_info->IsTRR()) {
            fromTRR = true;
        }
    }
    if (!fromTRR &&
        !mShutdown && !rec->mGetTtl && !rec->mResolving && sGetTtlEnabled) {
        LOG(("Issuing second async lookup for TTL for host [%s].", rec->host.get()));
        rec->flags =
            (rec->flags & ~RES_PRIORITY_MEDIUM) | RES_PRIORITY_LOW |
            RES_DISABLE_TRR;
        DebugOnly<nsresult> rv = NameLookup(rec);
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rv),
            "Could not issue second async lookup for TTL.");
    }
#endif
    return LOOKUP_OK;
}

void
nsHostResolver::CancelAsyncRequest(const nsACString &host,
                                   const OriginAttributes &aOriginAttributes,
                                   uint16_t                flags,
                                   uint16_t                af,
                                   nsIDNSListener         *aListener,
                                   nsresult                status)

{
    MutexAutoLock lock(mLock);

    nsAutoCString originSuffix;
    aOriginAttributes.CreateSuffix(originSuffix);

    // Lookup the host record associated with host, flags & address family

    nsHostKey key(host, flags, af,
                  (aOriginAttributes.mPrivateBrowsingId > 0),
                  originSuffix);
    RefPtr<nsHostRecord> rec = mRecordDB.Get(key);
    if (rec) {
        nsHostRecord* recPtr = nullptr;

        for (RefPtr<nsResolveHostCallback> c : rec->mCallbacks) {
            if (c->EqualsAsyncListener(aListener)) {
                c->remove();
                recPtr = rec;
                c->OnResolveHostComplete(this, recPtr, status);
                break;
            }
        }

        // If there are no more callbacks, remove the hash table entry
        if (recPtr && recPtr->mCallbacks.isEmpty()) {
            mRecordDB.Remove(*static_cast<nsHostKey *>(recPtr));
            // If record is on a Queue, remove it and then deref it
            if (recPtr->isInList()) {
                recPtr->remove();
            }
        }
    }
}

size_t
nsHostResolver::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const
{
    MutexAutoLock lock(mLock);

    size_t n = mallocSizeOf(this);

    n += mRecordDB.ShallowSizeOfExcludingThis(mallocSizeOf);
    for (auto iter = mRecordDB.ConstIter(); !iter.Done(); iter.Next()) {
        auto entry = iter.UserData();
        n += entry->SizeOfIncludingThis(mallocSizeOf);
    }

    // The following fields aren't measured.
    // - mHighQ, mMediumQ, mLowQ, mEvictionQ, because they just point to
    //   nsHostRecords that also pointed to by entries |mRecordDB|, and
    //   measured when |mRecordDB| is measured.

    return n;
}

void
nsHostResolver::ThreadFunc()
{
    LOG(("DNS lookup thread - starting execution.\n"));

#if defined(RES_RETRY_ON_FAILURE)
    nsResState rs;
#endif
    RefPtr<nsHostRecord> rec;
    AddrInfo *ai = nullptr;

    do {
        if (!rec) {
            RefPtr<nsHostRecord> tmpRec;
            if (!GetHostToLookup(getter_AddRefs(tmpRec))) {
                break; // thread shutdown signal
            }
            // GetHostToLookup() returns an owning reference
            MOZ_ASSERT(tmpRec);
            rec.swap(tmpRec);
        }

        LOG(("DNS lookup thread - Calling getaddrinfo for host [%s].\n",
             rec->host.get()));

        TimeStamp startTime = TimeStamp::Now();
        bool getTtl = rec->mGetTtl;
        nsresult status = GetAddrInfo(rec->host, rec->af,
                                      rec->flags, &ai,
                                      getTtl);
#if defined(RES_RETRY_ON_FAILURE)
        if (NS_FAILED(status) && rs.Reset()) {
            status = GetAddrInfo(rec->host, rec->af,
                                 rec->flags, &ai, getTtl);
        }
#endif

        {   // obtain lock to check shutdown and manage inter-module telemetry
            MutexAutoLock lock(mLock);

            if (!mShutdown) {
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

        LOG(("DNS lookup thread - lookup completed for host [%s]: %s.\n",
             rec->host.get(),
             ai ? "success" : "failure: unknown host"));

        if (LOOKUP_RESOLVEAGAIN == CompleteLookup(rec, status, ai, rec->pb)) {
            // leave 'rec' assigned and loop to make a renewed host resolve
            LOG(("DNS lookup thread - Re-resolving host [%s].\n", rec->host.get()));
        } else {
            rec = nullptr;
        }
    } while(true);

    nsCOMPtr<nsIThread> thread = NS_GetCurrentThread();
    NS_DispatchToMainThread(NS_NewRunnableFunction("nsHostResolver::ThreadFunc::AsyncShutdown", [thread]() {
        thread->AsyncShutdown();
    }));
    mThreadCount--;
    LOG(("DNS lookup thread - queue empty, thread finished.\n"));
}

void
nsHostResolver::SetCacheLimits(uint32_t aMaxCacheEntries,
                               uint32_t aDefaultCacheEntryLifetime,
                               uint32_t aDefaultGracePeriod)
{
    MutexAutoLock lock(mLock);
    mMaxCacheEntries = aMaxCacheEntries;
    mDefaultCacheLifetime = aDefaultCacheEntryLifetime;
    mDefaultGracePeriod = aDefaultGracePeriod;
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
    MutexAutoLock lock(mLock);
    for (auto iter = mRecordDB.Iter(); !iter.Done(); iter.Next()) {
        // We don't pay attention to address literals, only resolved domains.
        // Also require a host.
        nsHostRecord* rec = iter.UserData();
        MOZ_ASSERT(rec, "rec should never be null here!");
        if (!rec || !rec->addr_info) {
            continue;
        }

        DNSCacheEntries info;
        info.hostname = rec->host;
        info.family = rec->af;
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
            info.TRR = rec->addr_info->IsTRR();
        }

        args->AppendElement(info);
    }
}

#undef LOG
#undef LOG_ENABLED
