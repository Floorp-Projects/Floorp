/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(HAVE_RES_NINIT)
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <arpa/nameser.h>
#  include <resolv.h>
#  define RES_RETRY_ON_FAILURE
#endif

#include <stdlib.h>
#include <ctime>
#include "nsHostResolver.h"
#include "nsError.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"
#include "nsIThreadManager.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsXPCOMCIDInternal.h"
#include "prthread.h"
#include "prerror.h"
#include "prtime.h"
#include "mozilla/Logging.h"
#include "PLDHashTable.h"
#include "plstr.h"
#include "nsQueryObject.h"
#include "nsURLHelper.h"
#include "nsThreadUtils.h"
#include "nsThreadPool.h"
#include "GetAddrInfo.h"
#include "TRR.h"
#include "TRRQuery.h"
#include "TRRService.h"
#include "ODoHService.h"

#include "mozilla/Atomics.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_network.h"
// Put DNSLogging.h at the end to avoid LOG being overwritten by other headers.
#include "DNSLogging.h"

#define IS_ADDR_TYPE(_type) ((_type) == nsIDNSService::RESOLVE_TYPE_DEFAULT)
#define IS_OTHER_TYPE(_type) ((_type) != nsIDNSService::RESOLVE_TYPE_DEFAULT)

using namespace mozilla;
using namespace mozilla::net;

// None of our implementations expose a TTL for negative responses, so we use a
// constant always.
static const unsigned int NEGATIVE_RECORD_LIFETIME = 60;

//----------------------------------------------------------------------------

// Use a persistent thread pool in order to avoid spinning up new threads all
// the time. In particular, thread creation results in a res_init() call from
// libc which is quite expensive.
//
// The pool dynamically grows between 0 and MAX_RESOLVER_THREADS in size. New
// requests go first to an idle thread. If that cannot be found and there are
// fewer than MAX_RESOLVER_THREADS currently in the pool a new thread is created
// for high priority requests. If the new request is at a lower priority a new
// thread will only be created if there are fewer than HighThreadThreshold
// currently outstanding. If a thread cannot be created or an idle thread
// located for the request it is queued.
//
// When the pool is greater than HighThreadThreshold in size a thread will be
// destroyed after ShortIdleTimeoutSeconds of idle time. Smaller pools use
// LongIdleTimeoutSeconds for a timeout period.

#define HighThreadThreshold MAX_RESOLVER_THREADS_FOR_ANY_PRIORITY
#define LongIdleTimeoutSeconds 300  // for threads 1 -> HighThreadThreshold
#define ShortIdleTimeoutSeconds \
  60  // for threads HighThreadThreshold+1 -> MAX_RESOLVER_THREADS

static_assert(
    HighThreadThreshold <= MAX_RESOLVER_THREADS,
    "High Thread Threshold should be less equal Maximum allowed thread");

//----------------------------------------------------------------------------

namespace mozilla::net {
LazyLogModule gHostResolverLog("nsHostResolver");
}  // namespace mozilla::net

//----------------------------------------------------------------------------

#if defined(RES_RETRY_ON_FAILURE)

// this class represents the resolver state for a given thread.  if we
// encounter a lookup failure, then we can invoke the Reset method on an
// instance of this class to reset the resolver (in case /etc/resolv.conf
// for example changed).  this is mainly an issue on GNU systems since glibc
// only reads in /etc/resolv.conf once per thread.  it may be an issue on
// other systems as well.

class nsResState {
 public:
  nsResState()
      // initialize mLastReset to the time when this object
      // is created.  this means that a reset will not occur
      // if a thread is too young.  the alternative would be
      // to initialize this to the beginning of time, so that
      // the first failure would cause a reset, but since the
      // thread would have just started up, it likely would
      // already have current /etc/resolv.conf info.
      : mLastReset(PR_IntervalNow()) {}

  bool Reset() {
    // reset no more than once per second
    if (PR_IntervalToSeconds(PR_IntervalNow() - mLastReset) < 1) {
      return false;
    }

    mLastReset = PR_IntervalNow();
    auto result = res_ninit(&_res);

    LOG(("nsResState::Reset() > 'res_ninit' returned %d", result));
    return (result == 0);
  }

 private:
  PRIntervalTime mLastReset;
};

#endif  // RES_RETRY_ON_FAILURE

//----------------------------------------------------------------------------

static const char kPrefGetTtl[] = "network.dns.get-ttl";
static const char kPrefNativeIsLocalhost[] = "network.dns.native-is-localhost";
static const char kPrefThreadIdleTime[] =
    "network.dns.resolver-thread-extra-idle-time-seconds";
static bool sGetTtlEnabled = false;
mozilla::Atomic<bool, mozilla::Relaxed> gNativeIsLocalhost;

static void DnsPrefChanged(const char* aPref, void* aSelf) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Should be getting pref changed notification on main thread!");

  MOZ_ASSERT(aSelf);

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
    : mMaxCacheEntries(maxCacheEntries),
      mDefaultCacheLifetime(defaultCacheEntryLifetime),
      mDefaultGracePeriod(defaultGracePeriod),
      mIdleTaskCV(mLock, "nsHostResolver.mIdleTaskCV") {
  mCreationTime = PR_Now();

  mLongIdleTimeout = TimeDuration::FromSeconds(LongIdleTimeoutSeconds);
  mShortIdleTimeout = TimeDuration::FromSeconds(ShortIdleTimeoutSeconds);
}

nsHostResolver::~nsHostResolver() = default;

nsresult nsHostResolver::Init() {
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_FAILED(GetAddrInfoInit())) {
    return NS_ERROR_FAILURE;
  }

  LOG(("nsHostResolver::Init this=%p", this));

  mShutdown = false;
  mNCS = NetworkConnectivityService::GetSingleton();

  // The preferences probably haven't been loaded from the disk yet, so we
  // need to register a callback that will set up the experiment once they
  // are. We also need to explicitly set a value for the props otherwise the
  // callback won't be called.
  {
    DebugOnly<nsresult> rv = Preferences::RegisterCallbackAndCall(
        &DnsPrefChanged, kPrefGetTtl, this);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Could not register DNS TTL pref callback.");
    rv = Preferences::RegisterCallbackAndCall(&DnsPrefChanged,
                                              kPrefNativeIsLocalhost, this);
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
    auto result = res_ninit(&_res);
    LOG(("nsHostResolver::Init > 'res_ninit' returned %d", result));
  }
#endif

  // We can configure the threadpool to keep threads alive for a while after
  // the last ThreadFunc task has been executed.
  int32_t poolTimeoutSecs = Preferences::GetInt(kPrefThreadIdleTime, 60);
  uint32_t poolTimeoutMs;
  if (poolTimeoutSecs < 0) {
    // This means never shut down the idle threads
    poolTimeoutMs = UINT32_MAX;
  } else {
    // We clamp down the idle time between 0 and one hour.
    poolTimeoutMs =
        mozilla::clamped<uint32_t>(poolTimeoutSecs * 1000, 0, 3600 * 1000);
  }

  nsCOMPtr<nsIThreadPool> threadPool = new nsThreadPool();
  MOZ_ALWAYS_SUCCEEDS(threadPool->SetThreadLimit(MAX_RESOLVER_THREADS));
  MOZ_ALWAYS_SUCCEEDS(threadPool->SetIdleThreadLimit(MAX_RESOLVER_THREADS));
  MOZ_ALWAYS_SUCCEEDS(threadPool->SetIdleThreadTimeout(poolTimeoutMs));
  MOZ_ALWAYS_SUCCEEDS(
      threadPool->SetThreadStackSize(nsIThreadManager::kThreadPoolStackSize));
  MOZ_ALWAYS_SUCCEEDS(threadPool->SetName("DNS Resolver"_ns));
  mResolverThreads = ToRefPtr(std::move(threadPool));

  return NS_OK;
}

void nsHostResolver::ClearPendingQueue(
    LinkedList<RefPtr<nsHostRecord>>& aPendingQ) {
  // loop through pending queue, erroring out pending lookups.
  if (!aPendingQ.isEmpty()) {
    for (const RefPtr<nsHostRecord>& rec : aPendingQ) {
      rec->Cancel();
      if (rec->IsAddrRecord()) {
        CompleteLookup(rec, NS_ERROR_ABORT, nullptr, rec->pb, rec->originSuffix,
                       rec->mTRRSkippedReason, nullptr);
      } else {
        mozilla::net::TypeRecordResultType empty(Nothing{});
        CompleteLookupByType(rec, NS_ERROR_ABORT, empty, 0, rec->pb);
      }
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
// cache that have 'Resolve' set true but not 'OnQueue' are being resolved
// right now, so we need to mark them to get re-resolved on completion!

void nsHostResolver::FlushCache(bool aTrrToo) {
  MutexAutoLock lock(mLock);

  mQueue.FlushEvictionQ(mRecordDB, lock);

  // Refresh the cache entries that are resolving RIGHT now, remove the rest.
  for (auto iter = mRecordDB.Iter(); !iter.Done(); iter.Next()) {
    nsHostRecord* record = iter.UserData();
    // Try to remove the record, or mark it for refresh.
    // By-type records are from TRR. We do not need to flush those entry
    // when the network has change, because they are not local.
    if (record->IsAddrRecord()) {
      RefPtr<AddrHostRecord> addrRec = do_QueryObject(record);
      MOZ_ASSERT(addrRec);
      if (addrRec->RemoveOrRefresh(aTrrToo)) {
        mQueue.MaybeRemoveFromQ(record, lock);
        LOG(("Removing (%s) Addr record from mRecordDB", record->host.get()));
        iter.Remove();
      }
    } else if (aTrrToo) {
      // remove by type records
      LOG(("Removing (%s) type record from mRecordDB", record->host.get()));
      iter.Remove();
    }
  }
}

void nsHostResolver::Shutdown() {
  LOG(("Shutting down host resolver.\n"));

  {
    DebugOnly<nsresult> rv =
        Preferences::UnregisterCallback(&DnsPrefChanged, kPrefGetTtl, this);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Could not unregister DNS TTL pref callback.");
  }

  LinkedList<RefPtr<nsHostRecord>> pendingQHigh, pendingQMed, pendingQLow,
      evictionQ;

  {
    MutexAutoLock lock(mLock);

    mShutdown = true;

    if (mNumIdleTasks) {
      mIdleTaskCV.NotifyAll();
    }

    mQueue.ClearAll(
        [&](nsHostRecord* aRec) {
          if (aRec->IsAddrRecord()) {
            CompleteLookupLocked(aRec, NS_ERROR_ABORT, nullptr, aRec->pb,
                                 aRec->originSuffix, aRec->mTRRSkippedReason,
                                 nullptr, lock);
          } else {
            mozilla::net::TypeRecordResultType empty(Nothing{});
            CompleteLookupByTypeLocked(aRec, NS_ERROR_ABORT, empty, 0, aRec->pb,
                                       lock);
          }
        },
        lock);

    for (const auto& data : mRecordDB.Values()) {
      data->Cancel();
    }
    // empty host database
    mRecordDB.Clear();

    mNCS = nullptr;
  }

  // Shutdown the resolver threads, but with a timeout of 2 seconds (prefable).
  // If the timeout is exceeded, any stuck threads will be leaked.
  mResolverThreads->ShutdownWithTimeout(
      StaticPrefs::network_dns_resolver_shutdown_timeout_ms());

  {
    mozilla::DebugOnly<nsresult> rv = GetAddrInfoShutdown();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to shutdown GetAddrInfo");
  }
}

nsresult nsHostResolver::GetHostRecord(const nsACString& host,
                                       const nsACString& aTrrServer,
                                       uint16_t type, uint16_t flags,
                                       uint16_t af, bool pb,
                                       const nsCString& originSuffix,
                                       nsHostRecord** result) {
  MutexAutoLock lock(mLock);
  nsHostKey key(host, aTrrServer, type, flags, af, pb, originSuffix);

  RefPtr<nsHostRecord> rec =
      mRecordDB.LookupOrInsertWith(key, [&] { return InitRecord(key); });
  if (rec->IsAddrRecord()) {
    RefPtr<AddrHostRecord> addrRec = do_QueryObject(rec);
    if (addrRec->addr) {
      return NS_ERROR_FAILURE;
    }
  }

  if (rec->mResolving) {
    return NS_ERROR_FAILURE;
  }

  *result = rec.forget().take();
  return NS_OK;
}

nsHostRecord* nsHostResolver::InitRecord(const nsHostKey& key) {
  if (IS_ADDR_TYPE(key.type)) {
    return new AddrHostRecord(key);
  }
  return new TypeHostRecord(key);
}

already_AddRefed<nsHostRecord> nsHostResolver::InitLoopbackRecord(
    const nsHostKey& key, nsresult* aRv) {
  MOZ_ASSERT(aRv);
  MOZ_ASSERT(IS_ADDR_TYPE(key.type));

  *aRv = NS_ERROR_FAILURE;
  RefPtr<nsHostRecord> rec = InitRecord(key);

  nsTArray<NetAddr> addresses;
  NetAddr addr;
  if (key.af == PR_AF_INET || key.af == PR_AF_UNSPEC) {
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(addr.InitFromString("127.0.0.1"_ns)));
    addresses.AppendElement(addr);
  }
  if (key.af == PR_AF_INET6 || key.af == PR_AF_UNSPEC) {
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(addr.InitFromString("::1"_ns)));
    addresses.AppendElement(addr);
  }

  RefPtr<AddrInfo> ai =
      new AddrInfo(rec->host, DNSResolverType::Native, 0, std::move(addresses));

  RefPtr<AddrHostRecord> addrRec = do_QueryObject(rec);
  MutexAutoLock lock(addrRec->addr_info_lock);
  addrRec->addr_info = ai;
  addrRec->SetExpiration(TimeStamp::NowLoRes(), mDefaultCacheLifetime,
                         mDefaultGracePeriod);
  addrRec->negative = false;

  *aRv = NS_OK;
  return rec.forget();
}

nsresult nsHostResolver::ResolveHost(const nsACString& aHost,
                                     const nsACString& aTrrServer,
                                     uint16_t type,
                                     const OriginAttributes& aOriginAttributes,
                                     uint16_t flags, uint16_t af,
                                     nsResolveHostCallback* aCallback) {
  nsAutoCString host(aHost);
  NS_ENSURE_TRUE(!host.IsEmpty(), NS_ERROR_UNEXPECTED);

  nsAutoCString originSuffix;
  aOriginAttributes.CreateSuffix(originSuffix);
  LOG(("Resolving host [%s]<%s>%s%s type %d. [this=%p]\n", host.get(),
       originSuffix.get(), flags & RES_BYPASS_CACHE ? " - bypassing cache" : "",
       flags & RES_REFRESH_CACHE ? " - refresh cache" : "", type, this));

  // ensure that we are working with a valid hostname before proceeding.  see
  // bug 304904 for details.
  if (!net_IsValidHostName(host)) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  // By-Type requests use only TRR. If TRR is disabled we can return
  // immediately.
  if (IS_OTHER_TYPE(type) && Mode() == nsIDNSService::MODE_TRROFF) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  // Used to try to parse to an IP address literal.
  NetAddr tempAddr;
  if (IS_OTHER_TYPE(type) && (NS_SUCCEEDED(tempAddr.InitFromString(host)))) {
    // For by-type queries the host cannot be IP literal.
    return NS_ERROR_UNKNOWN_HOST;
  }

  RefPtr<nsResolveHostCallback> callback(aCallback);
  // if result is set inside the lock, then we need to issue the
  // callback before returning.
  RefPtr<nsHostRecord> result;
  nsresult status = NS_OK, rv = NS_OK;
  {
    MutexAutoLock lock(mLock);

    if (mShutdown) {
      return NS_ERROR_NOT_INITIALIZED;
    }

    // check to see if there is already an entry for this |host|
    // in the hash table.  if so, then check to see if we can't
    // just reuse the lookup result.  otherwise, if there are
    // any pending callbacks, then add to pending callbacks queue,
    // and return.  otherwise, add ourselves as first pending
    // callback, and proceed to do the lookup.

    bool excludedFromTRR = false;
    if (TRRService::Get() && TRRService::Get()->IsExcludedFromTRR(host)) {
      flags |= RES_DISABLE_TRR;
      excludedFromTRR = true;

      if (!aTrrServer.IsEmpty()) {
        return NS_ERROR_UNKNOWN_HOST;
      }
    }

    nsHostKey key(host, aTrrServer, type, flags, af,
                  (aOriginAttributes.mPrivateBrowsingId > 0), originSuffix);

    // Check if we have a localhost domain, if so hardcode to loopback
    if (IS_ADDR_TYPE(type) && IsLoopbackHostname(host)) {
      nsresult rv;
      RefPtr<nsHostRecord> result = InitLoopbackRecord(key, &rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      MOZ_ASSERT(result);
      aCallback->OnResolveHostComplete(this, result, NS_OK);
      return NS_OK;
    }

    RefPtr<nsHostRecord> rec =
        mRecordDB.LookupOrInsertWith(key, [&] { return InitRecord(key); });

    RefPtr<AddrHostRecord> addrRec = do_QueryObject(rec);
    MOZ_ASSERT(rec, "Record should not be null");
    MOZ_ASSERT((IS_ADDR_TYPE(type) && rec->IsAddrRecord() && addrRec) ||
               (IS_OTHER_TYPE(type) && !rec->IsAddrRecord()));

    if (excludedFromTRR) {
      rec->RecordReason(TRRSkippedReason::TRR_EXCLUDED);
    }

    if (!(flags & RES_BYPASS_CACHE) &&
        rec->HasUsableResult(TimeStamp::NowLoRes(), flags)) {
      result = FromCache(rec, host, type, status, lock);
    } else if (addrRec && addrRec->addr) {
      // if the host name is an IP address literal and has been
      // parsed, go ahead and use it.
      LOG(("  Using cached address for IP Literal [%s].\n", host.get()));
      result = FromCachedIPLiteral(rec);
    } else if (addrRec && NS_SUCCEEDED(tempAddr.InitFromString(host))) {
      // try parsing the host name as an IP address literal to short
      // circuit full host resolution.  (this is necessary on some
      // platforms like Win9x.  see bug 219376 for more details.)
      LOG(("  Host is IP Literal [%s].\n", host.get()));
      result = FromIPLiteral(addrRec, tempAddr);
    } else if (mQueue.PendingCount() >= MAX_NON_PRIORITY_REQUESTS &&
               !IsHighPriority(flags) && !rec->mResolving) {
      LOG(
          ("  Lookup queue full: dropping %s priority request for "
           "host [%s].\n",
           IsMediumPriority(flags) ? "medium" : "low", host.get()));
      if (IS_ADDR_TYPE(type)) {
        Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_OVERFLOW);
      }
      // This is a lower priority request and we are swamped, so refuse it.
      rv = NS_ERROR_DNS_LOOKUP_QUEUE_FULL;

      // Check if the offline flag is set.
    } else if (flags & RES_OFFLINE) {
      LOG(("  Offline request for host [%s]; ignoring.\n", host.get()));
      rv = NS_ERROR_OFFLINE;

      // We do not have a valid result till here.
      // A/AAAA request can check for an alternative entry like AF_UNSPEC.
      // Otherwise we need to start a new query.
    } else if (!rec->mResolving) {
      result =
          FromUnspecEntry(rec, host, aTrrServer, originSuffix, type, flags, af,
                          aOriginAttributes.mPrivateBrowsingId > 0, status);
      // If this is a by-type request or if no valid record was found
      // in the cache or this is an AF_UNSPEC request, then start a
      // new lookup.
      if (!result) {
        LOG(("  No usable record in cache for host [%s] type %d.", host.get(),
             type));

        if (flags & RES_REFRESH_CACHE) {
          rec->Invalidate();
        }

        // Add callback to the list of pending callbacks.
        rec->mCallbacks.insertBack(callback);
        rec->flags = flags;
        rv = NameLookup(rec, lock);
        if (IS_ADDR_TYPE(type)) {
          Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                                METHOD_NETWORK_FIRST);
        }
        if (NS_FAILED(rv) && callback->isInList()) {
          callback->remove();
        } else {
          LOG(
              ("  DNS lookup for host [%s] blocking "
               "pending 'getaddrinfo' or trr query: "
               "callback [%p]",
               host.get(), callback.get()));
        }
      }
    } else {
      LOG(
          ("  Host [%s] is being resolved. Appending callback "
           "[%p].",
           host.get(), callback.get()));

      rec->mCallbacks.insertBack(callback);

      // Only A/AAAA records are place in a queue. The queues are for
      // the native resolver, therefore by-type request are never put
      // into a queue.
      if (addrRec && addrRec->onQueue()) {
        Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                              METHOD_NETWORK_SHARED);

        // Consider the case where we are on a pending queue of
        // lower priority than the request is being made at.
        // In that case we should upgrade to the higher queue.

        if (IsHighPriority(flags) && !IsHighPriority(rec->flags)) {
          // Move from (low|med) to high.
          mQueue.MoveToAnotherPendingQ(rec, flags, lock);
          rec->flags = flags;
          ConditionallyCreateThread(rec);
        } else if (IsMediumPriority(flags) && IsLowPriority(rec->flags)) {
          // Move from low to med.
          mQueue.MoveToAnotherPendingQ(rec, flags, lock);
          rec->flags = flags;
          mIdleTaskCV.Notify();
        }
      }
    }

    if (result && callback->isInList()) {
      callback->remove();
    }
  }  // lock

  if (result) {
    callback->OnResolveHostComplete(this, result, status);
  }

  return rv;
}

already_AddRefed<nsHostRecord> nsHostResolver::FromCache(
    nsHostRecord* aRec, const nsACString& aHost, uint16_t aType,
    nsresult& aStatus, const MutexAutoLock& aLock) {
  LOG(("  Using cached record for host [%s].\n",
       nsPromiseFlatCString(aHost).get()));

  // put reference to host record on stack...
  RefPtr<nsHostRecord> result = aRec;
  if (IS_ADDR_TYPE(aType)) {
    Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_HIT);
  }

  // For entries that are in the grace period
  // or all cached negative entries, use the cache but start a new
  // lookup in the background
  ConditionallyRefreshRecord(aRec, aHost, aLock);

  if (aRec->negative) {
    LOG(("  Negative cache entry for host [%s].\n",
         nsPromiseFlatCString(aHost).get()));
    if (IS_ADDR_TYPE(aType)) {
      Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_NEGATIVE_HIT);
    }
    aStatus = NS_ERROR_UNKNOWN_HOST;
  }

  return result.forget();
}

already_AddRefed<nsHostRecord> nsHostResolver::FromCachedIPLiteral(
    nsHostRecord* aRec) {
  Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_LITERAL);
  RefPtr<nsHostRecord> result = aRec;
  return result.forget();
}

already_AddRefed<nsHostRecord> nsHostResolver::FromIPLiteral(
    AddrHostRecord* aAddrRec, const NetAddr& aAddr) {
  // ok, just copy the result into the host record, and be
  // done with it! ;-)
  aAddrRec->addr = MakeUnique<NetAddr>(aAddr);
  Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_LITERAL);
  // put reference to host record on stack...
  RefPtr<nsHostRecord> result = aAddrRec;
  return result.forget();
}

already_AddRefed<nsHostRecord> nsHostResolver::FromUnspecEntry(
    nsHostRecord* aRec, const nsACString& aHost, const nsACString& aTrrServer,
    const nsACString& aOriginSuffix, uint16_t aType, uint16_t aFlags,
    uint16_t af, bool aPb, nsresult& aStatus) {
  RefPtr<nsHostRecord> result = nullptr;
  // If this is an IPV4 or IPV6 specific request, check if there is
  // an AF_UNSPEC entry we can use. Otherwise, hit the resolver...
  RefPtr<AddrHostRecord> addrRec = do_QueryObject(aRec);
  if (addrRec && !(aFlags & RES_BYPASS_CACHE) &&
      ((af == PR_AF_INET) || (af == PR_AF_INET6))) {
    // Check for an AF_UNSPEC entry.

    const nsHostKey unspecKey(aHost, aTrrServer,
                              nsIDNSService::RESOLVE_TYPE_DEFAULT, aFlags,
                              PR_AF_UNSPEC, aPb, aOriginSuffix);
    RefPtr<nsHostRecord> unspecRec = mRecordDB.Get(unspecKey);

    TimeStamp now = TimeStamp::NowLoRes();
    if (unspecRec && unspecRec->HasUsableResult(now, aFlags)) {
      MOZ_ASSERT(unspecRec->IsAddrRecord());

      RefPtr<AddrHostRecord> addrUnspecRec = do_QueryObject(unspecRec);
      MOZ_ASSERT(addrUnspecRec);
      MOZ_ASSERT(addrUnspecRec->addr_info || addrUnspecRec->negative,
                 "Entry should be resolved or negative.");

      LOG(("  Trying AF_UNSPEC entry for host [%s] af: %s.\n",
           PromiseFlatCString(aHost).get(),
           (af == PR_AF_INET) ? "AF_INET" : "AF_INET6"));

      // We need to lock in case any other thread is reading
      // addr_info.
      MutexAutoLock lock(addrRec->addr_info_lock);

      addrRec->addr_info = nullptr;
      addrRec->addr_info_gencnt++;
      if (unspecRec->negative) {
        aRec->negative = unspecRec->negative;
        aRec->CopyExpirationTimesAndFlagsFrom(unspecRec);
      } else if (addrUnspecRec->addr_info) {
        MutexAutoLock lock(addrUnspecRec->addr_info_lock);
        if (addrUnspecRec->addr_info) {
          // Search for any valid address in the AF_UNSPEC entry
          // in the cache (not blocklisted and from the right
          // family).
          nsTArray<NetAddr> addresses;
          for (const auto& addr : addrUnspecRec->addr_info->Addresses()) {
            if ((af == addr.inet.family) &&
                !addrUnspecRec->Blocklisted(&addr)) {
              addresses.AppendElement(addr);
            }
          }
          if (!addresses.IsEmpty()) {
            addrRec->addr_info = new AddrInfo(
                addrUnspecRec->addr_info->Hostname(),
                addrUnspecRec->addr_info->CanonicalHostname(),
                addrUnspecRec->addr_info->ResolverType(),
                addrUnspecRec->addr_info->TRRType(), std::move(addresses));
            addrRec->addr_info_gencnt++;
            aRec->CopyExpirationTimesAndFlagsFrom(unspecRec);
          }
        }
      }
      // Now check if we have a new record.
      if (aRec->HasUsableResult(now, aFlags)) {
        result = aRec;
        if (aRec->negative) {
          aStatus = NS_ERROR_UNKNOWN_HOST;
        }
        Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_HIT);
        ConditionallyRefreshRecord(aRec, aHost, lock);
      } else if (af == PR_AF_INET6) {
        // For AF_INET6, a new lookup means another AF_UNSPEC
        // lookup. We have already iterated through the
        // AF_UNSPEC addresses, so we mark this record as
        // negative.
        LOG(
            ("  No AF_INET6 in AF_UNSPEC entry: "
             "host [%s] unknown host.",
             nsPromiseFlatCString(aHost).get()));
        result = aRec;
        aRec->negative = true;
        aStatus = NS_ERROR_UNKNOWN_HOST;
        Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2,
                              METHOD_NEGATIVE_HIT);
      }
    }
  }

  return result.forget();
}

void nsHostResolver::DetachCallback(
    const nsACString& host, const nsACString& aTrrServer, uint16_t aType,
    const OriginAttributes& aOriginAttributes, uint16_t flags, uint16_t af,
    nsResolveHostCallback* aCallback, nsresult status) {
  RefPtr<nsHostRecord> rec;
  RefPtr<nsResolveHostCallback> callback(aCallback);

  {
    MutexAutoLock lock(mLock);

    nsAutoCString originSuffix;
    aOriginAttributes.CreateSuffix(originSuffix);

    nsHostKey key(host, aTrrServer, aType, flags, af,
                  (aOriginAttributes.mPrivateBrowsingId > 0), originSuffix);
    RefPtr<nsHostRecord> entry = mRecordDB.Get(key);
    if (entry) {
      // walk list looking for |callback|... we cannot assume
      // that it will be there!

      for (nsResolveHostCallback* c : entry->mCallbacks) {
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

nsresult nsHostResolver::ConditionallyCreateThread(nsHostRecord* rec) {
  if (mNumIdleTasks) {
    // wake up idle tasks to process this lookup
    mIdleTaskCV.Notify();
  } else if ((mActiveTaskCount < HighThreadThreshold) ||
             (IsHighPriority(rec->flags) &&
              mActiveTaskCount < MAX_RESOLVER_THREADS)) {
    nsCOMPtr<nsIRunnable> event = mozilla::NewRunnableMethod(
        "nsHostResolver::ThreadFunc", this, &nsHostResolver::ThreadFunc);
    mActiveTaskCount++;
    nsresult rv =
        mResolverThreads->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      mActiveTaskCount--;
    }
  } else {
    LOG(("  Unable to find a thread for looking up host [%s].\n",
         rec->host.get()));
  }
  return NS_OK;
}

nsresult nsHostResolver::TrrLookup_unlocked(nsHostRecord* rec, TRR* pushedTRR) {
  MutexAutoLock lock(mLock);
  return TrrLookup(rec, lock, pushedTRR);
}

void nsHostResolver::MaybeRenewHostRecord(nsHostRecord* aRec) {
  MutexAutoLock lock(mLock);
  MaybeRenewHostRecordLocked(aRec, lock);
}

void nsHostResolver::MaybeRenewHostRecordLocked(nsHostRecord* aRec,
                                                const MutexAutoLock& aLock) {
  mQueue.MaybeRenewHostRecord(aRec, aLock);
}

bool nsHostResolver::TRRServiceEnabledForRecord(nsHostRecord* aRec) {
  MOZ_ASSERT(aRec, "Record must not be empty");
  MOZ_ASSERT(aRec->mEffectiveTRRMode != nsIRequest::TRR_DEFAULT_MODE,
             "effective TRR mode must be computed before this call");
  if (!TRRService::Get()) {
    aRec->RecordReason(TRRSkippedReason::TRR_NO_GSERVICE);
    return false;
  }

  // We always try custom resolvers.
  if (!aRec->mTrrServer.IsEmpty()) {
    return true;
  }

  nsIRequest::TRRMode reqMode = aRec->mEffectiveTRRMode;
  if (TRRService::Get()->Enabled(reqMode)) {
    return true;
  }

  if (NS_IsOffline()) {
    // If we are in the NOT_CONFIRMED state _because_ we lack connectivity,
    // then we should report that the browser is offline instead.
    aRec->RecordReason(TRRSkippedReason::TRR_IS_OFFLINE);
    return false;
  }

  auto hasConnectivity = [this]() -> bool {
    if (!mNCS) {
      return true;
    }
    nsINetworkConnectivityService::ConnectivityState ipv4 = mNCS->GetIPv4();
    nsINetworkConnectivityService::ConnectivityState ipv6 = mNCS->GetIPv6();

    if (ipv4 == nsINetworkConnectivityService::OK ||
        ipv6 == nsINetworkConnectivityService::OK) {
      return true;
    }

    if (ipv4 == nsINetworkConnectivityService::UNKNOWN ||
        ipv6 == nsINetworkConnectivityService::UNKNOWN) {
      // One of the checks hasn't completed yet. Optimistically assume we'll
      // have network connectivity.
      return true;
    }

    return false;
  };

  if (!hasConnectivity()) {
    aRec->RecordReason(TRRSkippedReason::TRR_NO_CONNECTIVITY);
    return false;
  }

  if (!TRRService::Get()->IsConfirmed()) {
    aRec->RecordReason(TRRSkippedReason::TRR_NOT_CONFIRMED);
    return false;
  }

  return false;
}

// returns error if no TRR resolve is issued
// it is impt this is not called while a native lookup is going on
nsresult nsHostResolver::TrrLookup(nsHostRecord* aRec,
                                   const MutexAutoLock& aLock, TRR* pushedTRR) {
  if (Mode() == nsIDNSService::MODE_TRROFF ||
      StaticPrefs::network_dns_disabled()) {
    return NS_ERROR_UNKNOWN_HOST;
  }
  LOG(("TrrLookup host:%s af:%" PRId16, aRec->host.get(), aRec->af));

  RefPtr<nsHostRecord> rec(aRec);
  mLock.AssertCurrentThreadOwns();

  RefPtr<AddrHostRecord> addrRec;
  RefPtr<TypeHostRecord> typeRec;

  if (rec->IsAddrRecord()) {
    addrRec = do_QueryObject(rec);
    MOZ_ASSERT(addrRec);
  } else {
    typeRec = do_QueryObject(rec);
    MOZ_ASSERT(typeRec);
  }

  MOZ_ASSERT(!rec->mResolving);

  if (!TRRServiceEnabledForRecord(aRec)) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  MaybeRenewHostRecordLocked(rec, aLock);

  RefPtr<TRRQuery> query = new TRRQuery(this, rec);
  bool useODoH = gODoHService->Enabled() &&
                 !((rec->flags & nsIDNSService::RESOLVE_DISABLE_ODOH));
  nsresult rv = query->DispatchLookup(pushedTRR, useODoH);
  if (NS_FAILED(rv)) {
    rec->RecordReason(TRRSkippedReason::TRR_DID_NOT_MAKE_QUERY);
    return rv;
  }

  {
    auto lock = rec->mTRRQuery.Lock();
    MOZ_ASSERT(!lock.ref(), "TRR already in progress");
    lock.ref() = query;
  }

  rec->mResolving++;
  rec->mTrrAttempts++;
  return NS_OK;
}

nsresult nsHostResolver::NativeLookup(nsHostRecord* aRec,
                                      const MutexAutoLock& aLock) {
  if (StaticPrefs::network_dns_disabled()) {
    return NS_ERROR_UNKNOWN_HOST;
  }
  LOG(("NativeLookup host:%s af:%" PRId16, aRec->host.get(), aRec->af));

  // Only A/AAAA request are resolve natively.
  MOZ_ASSERT(aRec->IsAddrRecord());
  mLock.AssertCurrentThreadOwns();

  RefPtr<nsHostRecord> rec(aRec);
  RefPtr<AddrHostRecord> addrRec;
  addrRec = do_QueryObject(rec);
  MOZ_ASSERT(addrRec);

  addrRec->mNativeStart = TimeStamp::Now();

  // Add rec to one of the pending queues, possibly removing it from mEvictionQ.
  MaybeRenewHostRecordLocked(aRec, aLock);

  mQueue.InsertRecord(rec, rec->flags, aLock);

  addrRec->StoreNative(true);
  addrRec->StoreNativeUsed(true);
  addrRec->mResolving++;

  nsresult rv = ConditionallyCreateThread(rec);

  LOG(("  DNS thread counters: total=%d any-live=%d idle=%d pending=%d\n",
       static_cast<uint32_t>(mActiveTaskCount),
       static_cast<uint32_t>(mActiveAnyThreadCount),
       static_cast<uint32_t>(mNumIdleTasks), mQueue.PendingCount()));

  return rv;
}

// static
nsIDNSService::ResolverMode nsHostResolver::Mode() {
  if (TRRService::Get()) {
    return TRRService::Get()->Mode();
  }

  // If we don't have a TRR service just return MODE_TRROFF so we don't make
  // any TRR requests by mistake.
  return nsIDNSService::MODE_TRROFF;
}

nsIRequest::TRRMode nsHostRecord::TRRMode() {
  return nsIDNSService::GetTRRModeFromFlags(flags);
}

// static
void nsHostResolver::ComputeEffectiveTRRMode(nsHostRecord* aRec) {
  nsIDNSService::ResolverMode resolverMode = nsHostResolver::Mode();
  nsIRequest::TRRMode requestMode = aRec->TRRMode();

  // For domains that are excluded from TRR or when parental control is enabled,
  // we fallback to NativeLookup. This happens even in MODE_TRRONLY. By default
  // localhost and local are excluded (so we cover *.local hosts) See the
  // network.trr.excluded-domains pref.

  if (!TRRService::Get()) {
    aRec->RecordReason(TRRSkippedReason::TRR_NO_GSERVICE);
    aRec->mEffectiveTRRMode = requestMode;
    return;
  }

  if (!aRec->mTrrServer.IsEmpty()) {
    aRec->mEffectiveTRRMode = nsIRequest::TRR_ONLY_MODE;
    return;
  }

  if (TRRService::Get()->IsExcludedFromTRR(aRec->host)) {
    aRec->RecordReason(TRRSkippedReason::TRR_EXCLUDED);
    aRec->mEffectiveTRRMode = nsIRequest::TRR_DISABLED_MODE;
    return;
  }

  if (StaticPrefs::network_dns_skipTRR_when_parental_control_enabled() &&
      TRRService::Get()->ParentalControlEnabled()) {
    aRec->RecordReason(TRRSkippedReason::TRR_PARENTAL_CONTROL);
    aRec->mEffectiveTRRMode = nsIRequest::TRR_DISABLED_MODE;
    return;
  }

  if (resolverMode == nsIDNSService::MODE_TRROFF) {
    aRec->RecordReason(TRRSkippedReason::TRR_OFF_EXPLICIT);
    aRec->mEffectiveTRRMode = nsIRequest::TRR_DISABLED_MODE;
    return;
  }

  if (requestMode == nsIRequest::TRR_DISABLED_MODE) {
    aRec->RecordReason(TRRSkippedReason::TRR_REQ_MODE_DISABLED);
    aRec->mEffectiveTRRMode = nsIRequest::TRR_DISABLED_MODE;
    return;
  }

  if ((requestMode == nsIRequest::TRR_DEFAULT_MODE &&
       resolverMode == nsIDNSService::MODE_NATIVEONLY)) {
    aRec->RecordReason(TRRSkippedReason::TRR_MODE_NOT_ENABLED);
    aRec->mEffectiveTRRMode = nsIRequest::TRR_DISABLED_MODE;
    return;
  }

  if (requestMode == nsIRequest::TRR_DEFAULT_MODE &&
      resolverMode == nsIDNSService::MODE_TRRFIRST) {
    aRec->mEffectiveTRRMode = nsIRequest::TRR_FIRST_MODE;
    return;
  }

  if (requestMode == nsIRequest::TRR_DEFAULT_MODE &&
      resolverMode == nsIDNSService::MODE_TRRONLY) {
    aRec->mEffectiveTRRMode = nsIRequest::TRR_ONLY_MODE;
    return;
  }

  aRec->mEffectiveTRRMode = requestMode;
}

// Kick-off a name resolve operation, using native resolver and/or TRR
nsresult nsHostResolver::NameLookup(nsHostRecord* rec,
                                    const mozilla::MutexAutoLock& aLock) {
  LOG(("NameLookup host:%s af:%" PRId16, rec->host.get(), rec->af));

  if (rec->flags & RES_IP_HINT) {
    LOG(("Skip lookup if RES_IP_HINT is set\n"));
    return NS_ERROR_UNKNOWN_HOST;
  }

  nsresult rv = NS_ERROR_UNKNOWN_HOST;
  if (rec->mResolving) {
    LOG(("NameLookup %s while already resolving\n", rec->host.get()));
    return NS_OK;
  }

  // Make sure we reset the reason each time we attempt to do a new lookup
  // so we don't wrongly report the reason for the previous one.
  rec->mTRRSkippedReason = TRRSkippedReason::TRR_UNSET;
  rec->mFirstTRRSkippedReason = TRRSkippedReason::TRR_UNSET;
  rec->mTrrAttempts = 0;

  ComputeEffectiveTRRMode(rec);

  if (rec->IsAddrRecord()) {
    RefPtr<AddrHostRecord> addrRec = do_QueryObject(rec);
    MOZ_ASSERT(addrRec);

    addrRec->StoreNativeUsed(false);
    addrRec->mResolverType = DNSResolverType::Native;
    addrRec->mNativeSuccess = false;
  }

  if (!rec->mTrrServer.IsEmpty()) {
    LOG(("NameLookup: %s use trr:%s", rec->host.get(), rec->mTrrServer.get()));
    if (rec->mEffectiveTRRMode != nsIRequest::TRR_ONLY_MODE) {
      return NS_ERROR_UNKNOWN_HOST;
    }

    if (rec->flags & RES_DISABLE_TRR) {
      LOG(("TRR with server and DISABLE_TRR flag. Returning error."));
      return NS_ERROR_UNKNOWN_HOST;
    }
    return TrrLookup(rec, aLock);
  }

  LOG(("NameLookup: %s effectiveTRRmode: %d flags: %X", rec->host.get(),
       rec->mEffectiveTRRMode, rec->flags));

  if (rec->flags & RES_DISABLE_TRR) {
    rec->RecordReason(TRRSkippedReason::TRR_DISABLED_FLAG);
  }

  bool serviceNotReady = !TRRServiceEnabledForRecord(rec);

  if (rec->mEffectiveTRRMode != nsIRequest::TRR_DISABLED_MODE &&
      !((rec->flags & RES_DISABLE_TRR)) && !serviceNotReady) {
    rv = TrrLookup(rec, aLock);
  }

  if (rec->mEffectiveTRRMode == nsIRequest::TRR_DISABLED_MODE ||
      (rec->mEffectiveTRRMode == nsIRequest::TRR_FIRST_MODE &&
       (rec->flags & RES_DISABLE_TRR || serviceNotReady || NS_FAILED(rv)))) {
    if (!rec->IsAddrRecord()) {
      return rv;
    }

#ifdef DEBUG
    // If we use this branch then the mTRRUsed flag should not be set
    // Even if we did call TrrLookup above, the fact that it failed sync-ly
    // means that we didn't actually succeed in opening the channel.
    RefPtr<AddrHostRecord> addrRec = do_QueryObject(rec);
    MOZ_ASSERT(addrRec && addrRec->mResolverType == DNSResolverType::Native);
#endif

    rv = NativeLookup(rec, aLock);
  }

  return rv;
}

nsresult nsHostResolver::ConditionallyRefreshRecord(
    nsHostRecord* rec, const nsACString& host, const MutexAutoLock& aLock) {
  if ((rec->CheckExpiration(TimeStamp::NowLoRes()) != nsHostRecord::EXP_VALID ||
       rec->negative) &&
      !rec->mResolving && rec->RefreshForNegativeResponse()) {
    LOG(("  Using %s cache entry for host [%s] but starting async renewal.",
         rec->negative ? "negative" : "positive", host.BeginReading()));
    NameLookup(rec, aLock);

    if (rec->IsAddrRecord() && !rec->negative) {
      // negative entries are constantly being refreshed, only
      // track positive grace period induced renewals
      Telemetry::Accumulate(Telemetry::DNS_LOOKUP_METHOD2, METHOD_RENEWAL);
    }
  }
  return NS_OK;
}

bool nsHostResolver::GetHostToLookup(AddrHostRecord** result) {
  bool timedOut = false;
  TimeDuration timeout;
  TimeStamp epoch, now;

  MutexAutoLock lock(mLock);

  timeout = (mNumIdleTasks >= HighThreadThreshold) ? mShortIdleTimeout
                                                   : mLongIdleTimeout;
  epoch = TimeStamp::Now();

  while (!mShutdown) {
    // remove next record from Q; hand over owning reference. Check high, then
    // med, then low

#define SET_GET_TTL(var, val) (var)->StoreGetTtl(sGetTtlEnabled && (val))

    RefPtr<AddrHostRecord> addrRec = mQueue.Dequeue(true, lock);
    if (addrRec) {
      SET_GET_TTL(addrRec, false);
      addrRec.forget(result);
      return true;
    }

    if (mActiveAnyThreadCount < HighThreadThreshold) {
      addrRec = mQueue.Dequeue(false, lock);
      if (addrRec) {
        MOZ_ASSERT(IsMediumPriority(addrRec->flags) ||
                   IsLowPriority(addrRec->flags));
        mActiveAnyThreadCount++;
        addrRec->StoreUsingAnyThread(true);
        SET_GET_TTL(addrRec, true);
        addrRec.forget(result);
        return true;
      }
    }

    // Determining timeout is racy, so allow one cycle through checking the
    // queues before exiting.
    if (timedOut) {
      break;
    }

    // wait for one or more of the following to occur:
    //  (1) the pending queue has a host record to process
    //  (2) the shutdown flag has been set
    //  (3) the thread has been idle for too long

    mNumIdleTasks++;
    mIdleTaskCV.Wait(timeout);
    mNumIdleTasks--;

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

void nsHostResolver::PrepareRecordExpirationAddrRecord(
    AddrHostRecord* rec) const {
  // NOTE: rec->addr_info_lock is already held by parent
  MOZ_ASSERT(((bool)rec->addr_info) != rec->negative);
  mLock.AssertCurrentThreadOwns();
  if (!rec->addr_info) {
    rec->SetExpiration(TimeStamp::NowLoRes(), NEGATIVE_RECORD_LIFETIME, 0);
    LOG(("Caching host [%s] negative record for %u seconds.\n", rec->host.get(),
         NEGATIVE_RECORD_LIFETIME));
    return;
  }

  unsigned int lifetime = mDefaultCacheLifetime;
  unsigned int grace = mDefaultGracePeriod;

  unsigned int ttl = mDefaultCacheLifetime;
  if (sGetTtlEnabled || rec->addr_info->IsTRROrODoH()) {
    if (rec->addr_info && rec->addr_info->TTL() != AddrInfo::NO_TTL_DATA) {
      ttl = rec->addr_info->TTL();
    }
    lifetime = ttl;
    grace = 0;
  }

  rec->SetExpiration(TimeStamp::NowLoRes(), lifetime, grace);
  LOG(("Caching host [%s] record for %u seconds (grace %d).", rec->host.get(),
       lifetime, grace));
}

static bool different_rrset(AddrInfo* rrset1, AddrInfo* rrset2) {
  if (!rrset1 || !rrset2) {
    return true;
  }

  LOG(("different_rrset %s\n", rrset1->Hostname().get()));

  if (rrset1->ResolverType() != rrset2->ResolverType()) {
    return true;
  }

  if (rrset1->TRRType() != rrset2->TRRType()) {
    return true;
  }

  if (rrset1->Addresses().Length() != rrset2->Addresses().Length()) {
    LOG(("different_rrset true due to length change\n"));
    return true;
  }

  nsTArray<NetAddr> orderedSet1 = rrset1->Addresses().Clone();
  nsTArray<NetAddr> orderedSet2 = rrset2->Addresses().Clone();
  orderedSet1.Sort();
  orderedSet2.Sort();

  bool eq = orderedSet1 == orderedSet2;
  if (!eq) {
    LOG(("different_rrset true due to content change\n"));
  } else {
    LOG(("different_rrset false\n"));
  }
  return !eq;
}

void nsHostResolver::AddToEvictionQ(nsHostRecord* rec,
                                    const MutexAutoLock& aLock) {
  mQueue.AddToEvictionQ(rec, mMaxCacheEntries, mRecordDB, aLock);
}

// After a first lookup attempt with TRR in mode 2, we may:
// - If we are not in strict mode, retry with native.
// - If we are in strict mode:
//   - Retry with native if the first attempt failed because we got NXDOMAIN, an
//     unreachable address (TRR_DISABLED_FLAG), or we skipped TRR because
//     Confirmation failed.
//   - Trigger a "strict mode" Confirmation which will start a fresh
//     connection for TRR, and then retry the lookup with TRR.
// Returns true if we retried with either TRR or Native.
bool nsHostResolver::MaybeRetryTRRLookup(
    AddrHostRecord* aAddrRec, nsresult aFirstAttemptStatus,
    TRRSkippedReason aFirstAttemptSkipReason, const MutexAutoLock& aLock) {
  if (NS_SUCCEEDED(aFirstAttemptStatus) ||
      aAddrRec->mEffectiveTRRMode != nsIRequest::TRR_FIRST_MODE ||
      aFirstAttemptStatus == NS_ERROR_DEFINITIVE_UNKNOWN_HOST) {
    return false;
  }

  MOZ_ASSERT(!aAddrRec->mResolving);
  if (!StaticPrefs::network_trr_strict_native_fallback()) {
    LOG(("nsHostResolver::MaybeRetryTRRLookup retrying with native"));
    NativeLookup(aAddrRec, aLock);
    return true;
  }

  if (aFirstAttemptSkipReason == TRRSkippedReason::TRR_NXDOMAIN ||
      aFirstAttemptSkipReason == TRRSkippedReason::TRR_DISABLED_FLAG ||
      aFirstAttemptSkipReason == TRRSkippedReason::TRR_NOT_CONFIRMED) {
    LOG(
        ("nsHostResolver::MaybeRetryTRRLookup retrying with native in strict "
         "mode, skip reason was %d",
         static_cast<uint32_t>(aFirstAttemptSkipReason)));
    NativeLookup(aAddrRec, aLock);
    return true;
  }

  if (aAddrRec->mTrrAttempts > 1) {
    LOG(("nsHostResolver::MaybeRetryTRRLookup mTrrAttempts>1, not retrying."));
    return false;
  }

  LOG(
      ("nsHostResolver::MaybeRetryTRRLookup triggering Confirmation and "
       "retrying with TRR, skip reason was %d",
       static_cast<uint32_t>(aFirstAttemptSkipReason)));
  TRRService::Get()->StrictModeConfirm();

  {
    // Clear out the old query
    auto trrQuery = aAddrRec->mTRRQuery.Lock();
    trrQuery.ref() = nullptr;
  }
  aAddrRec->NotifyRetryingTrr();
  TrrLookup(aAddrRec, aLock, nullptr /* pushedTRR */);
  return true;
}

//
// CompleteLookup() checks if the resolving should be redone and if so it
// returns LOOKUP_RESOLVEAGAIN, but only if 'status' is not NS_ERROR_ABORT.
nsHostResolver::LookupStatus nsHostResolver::CompleteLookup(
    nsHostRecord* rec, nsresult status, AddrInfo* aNewRRSet, bool pb,
    const nsACString& aOriginsuffix, TRRSkippedReason aReason,
    mozilla::net::TRR* aTRRRequest) {
  MutexAutoLock lock(mLock);
  return CompleteLookupLocked(rec, status, aNewRRSet, pb, aOriginsuffix,
                              aReason, aTRRRequest, lock);
}

nsHostResolver::LookupStatus nsHostResolver::CompleteLookupLocked(
    nsHostRecord* rec, nsresult status, AddrInfo* aNewRRSet, bool pb,
    const nsACString& aOriginsuffix, TRRSkippedReason aReason,
    mozilla::net::TRR* aTRRRequest, const mozilla::MutexAutoLock& aLock) {
  MOZ_ASSERT(rec);
  MOZ_ASSERT(rec->pb == pb);
  MOZ_ASSERT(rec->IsAddrRecord());

  RefPtr<AddrHostRecord> addrRec = do_QueryObject(rec);
  MOZ_ASSERT(addrRec);

  RefPtr<AddrInfo> newRRSet(aNewRRSet);
  MOZ_ASSERT(NS_FAILED(status) || newRRSet->Addresses().Length() > 0);

  DNSResolverType type =
      newRRSet ? newRRSet->ResolverType() : DNSResolverType::Native;

  if (NS_FAILED(status)) {
    newRRSet = nullptr;
  }

  if (addrRec->LoadResolveAgain() && (status != NS_ERROR_ABORT) &&
      type == DNSResolverType::Native) {
    LOG(("nsHostResolver record %p resolve again due to flushcache\n",
         addrRec.get()));
    addrRec->StoreResolveAgain(false);
    return LOOKUP_RESOLVEAGAIN;
  }

  MOZ_ASSERT(addrRec->mResolving);
  addrRec->mResolving--;
  LOG((
      "nsHostResolver::CompleteLookup %s %p %X resolver=%d stillResolving=%d\n",
      addrRec->host.get(), aNewRRSet, (unsigned int)status, (int)type,
      int(addrRec->mResolving)));

  if (type != DNSResolverType::Native) {
    if (NS_FAILED(status) && status != NS_ERROR_UNKNOWN_HOST &&
        status != NS_ERROR_DEFINITIVE_UNKNOWN_HOST) {
      // the errors are not failed resolves, that means
      // something else failed, consider this as *TRR not used*
      // for actually trying to resolve the host
      addrRec->mResolverType = DNSResolverType::Native;
    }

    if (NS_FAILED(status)) {
      if (aReason != TRRSkippedReason::TRR_UNSET) {
        addrRec->RecordReason(aReason);
      } else {
        // Unknown failed reason.
        addrRec->RecordReason(TRRSkippedReason::TRR_FAILED);
      }
    } else {
      addrRec->mTRRSuccess++;
      addrRec->RecordReason(TRRSkippedReason::TRR_OK);
    }

    if (MaybeRetryTRRLookup(addrRec, status, aReason, aLock)) {
      MOZ_ASSERT(addrRec->mResolving);
      return LOOKUP_OK;
    }

    if (!addrRec->mTRRSuccess) {
      // no TRR success
      newRRSet = nullptr;
    }

    if (NS_FAILED(status)) {
      // This is the error that consumers expect.
      status = NS_ERROR_UNKNOWN_HOST;
    }
  } else {  // native resolve completed
    if (addrRec->LoadUsingAnyThread()) {
      mActiveAnyThreadCount--;
      addrRec->StoreUsingAnyThread(false);
    }

    addrRec->mNativeSuccess = static_cast<bool>(newRRSet);
    if (addrRec->mNativeSuccess) {
      addrRec->mNativeDuration = TimeStamp::Now() - addrRec->mNativeStart;
    }
  }

  // This should always be cleared when a request is completed.
  addrRec->StoreNative(false);

  // update record fields.  We might have a addrRec->addr_info already if a
  // previous lookup result expired and we're reresolving it or we get
  // a late second TRR response.
  if (!mShutdown) {
    MutexAutoLock lock(addrRec->addr_info_lock);
    RefPtr<AddrInfo> old_addr_info;
    if (different_rrset(addrRec->addr_info, newRRSet)) {
      LOG(("nsHostResolver record %p new gencnt\n", addrRec.get()));
      old_addr_info = addrRec->addr_info;
      addrRec->addr_info = std::move(newRRSet);
      addrRec->addr_info_gencnt++;
    } else {
      if (addrRec->addr_info && newRRSet) {
        auto builder = addrRec->addr_info->Build();
        builder.SetTTL(newRRSet->TTL());
        // Update trr timings
        builder.SetTrrFetchDuration(newRRSet->GetTrrFetchDuration());
        builder.SetTrrFetchDurationNetworkOnly(
            newRRSet->GetTrrFetchDurationNetworkOnly());

        addrRec->addr_info = builder.Finish();
      }
      old_addr_info = std::move(newRRSet);
    }
    addrRec->negative = !addrRec->addr_info;
    PrepareRecordExpirationAddrRecord(addrRec);
  }

  if (LOG_ENABLED()) {
    MutexAutoLock lock(addrRec->addr_info_lock);
    if (addrRec->addr_info) {
      for (const auto& elem : addrRec->addr_info->Addresses()) {
        char buf[128];
        elem.ToStringBuffer(buf, sizeof(buf));
        LOG(("CompleteLookup: %s has %s\n", addrRec->host.get(), buf));
      }
    } else {
      LOG(("CompleteLookup: %s has NO address\n", addrRec->host.get()));
    }
  }

  // get the list of pending callbacks for this lookup, and notify
  // them that the lookup is complete.
  mozilla::LinkedList<RefPtr<nsResolveHostCallback>> cbs =
      std::move(rec->mCallbacks);

  LOG(("nsHostResolver record %p calling back dns users status:%X\n",
       addrRec.get(), int(status)));

  for (nsResolveHostCallback* c = cbs.getFirst(); c;
       c = c->removeAndGetNext()) {
    c->OnResolveHostComplete(this, rec, status);
  }

  if (!addrRec->mResolving && !mShutdown) {
    {
      auto trrQuery = addrRec->mTRRQuery.Lock();
      if (trrQuery.ref()) {
        addrRec->mTrrDuration = trrQuery.ref()->Duration();
      }
      trrQuery.ref() = nullptr;
    }
    addrRec->ResolveComplete();

    AddToEvictionQ(rec, aLock);
  }

#ifdef DNSQUERY_AVAILABLE
  // Unless the result is from TRR, resolve again to get TTL
  bool hasNativeResult = false;
  {
    MutexAutoLock lock(addrRec->addr_info_lock);
    if (addrRec->addr_info && !addrRec->addr_info->IsTRROrODoH()) {
      hasNativeResult = true;
    }
  }
  if (hasNativeResult && !mShutdown && !addrRec->LoadGetTtl() &&
      !rec->mResolving && sGetTtlEnabled) {
    LOG(("Issuing second async lookup for TTL for host [%s].",
         addrRec->host.get()));
    addrRec->flags = (addrRec->flags & ~RES_PRIORITY_MEDIUM) | RES_PRIORITY_LOW;
    DebugOnly<nsresult> rv = NativeLookup(rec, aLock);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Could not issue second async lookup for TTL.");
  }
#endif
  return LOOKUP_OK;
}

nsHostResolver::LookupStatus nsHostResolver::CompleteLookupByType(
    nsHostRecord* rec, nsresult status,
    mozilla::net::TypeRecordResultType& aResult, uint32_t aTtl, bool pb) {
  MutexAutoLock lock(mLock);
  return CompleteLookupByTypeLocked(rec, status, aResult, aTtl, pb, lock);
}

nsHostResolver::LookupStatus nsHostResolver::CompleteLookupByTypeLocked(
    nsHostRecord* rec, nsresult status,
    mozilla::net::TypeRecordResultType& aResult, uint32_t aTtl, bool pb,
    const mozilla::MutexAutoLock& aLock) {
  MOZ_ASSERT(rec);
  MOZ_ASSERT(rec->pb == pb);
  MOZ_ASSERT(!rec->IsAddrRecord());

  RefPtr<TypeHostRecord> typeRec = do_QueryObject(rec);
  MOZ_ASSERT(typeRec);

  MOZ_ASSERT(typeRec->mResolving);
  typeRec->mResolving--;

  {
    auto lock = rec->mTRRQuery.Lock();
    lock.ref() = nullptr;
  }

  uint32_t duration = static_cast<uint32_t>(
      (TimeStamp::Now() - typeRec->mStart).ToMilliseconds());

  if (NS_FAILED(status)) {
    LOG(("nsHostResolver::CompleteLookupByType record %p [%s] status %x\n",
         typeRec.get(), typeRec->host.get(), (unsigned int)status));
    typeRec->SetExpiration(
        TimeStamp::NowLoRes(),
        StaticPrefs::network_dns_negative_ttl_for_type_record(), 0);
    MOZ_ASSERT(aResult.is<TypeRecordEmpty>());
    status = NS_ERROR_UNKNOWN_HOST;
    typeRec->negative = true;
    Telemetry::Accumulate(Telemetry::DNS_BY_TYPE_FAILED_LOOKUP_TIME, duration);
  } else {
    size_t recordCount = 0;
    if (aResult.is<TypeRecordTxt>()) {
      recordCount = aResult.as<TypeRecordTxt>().Length();
    } else if (aResult.is<TypeRecordHTTPSSVC>()) {
      recordCount = aResult.as<TypeRecordHTTPSSVC>().Length();
    }
    LOG(
        ("nsHostResolver::CompleteLookupByType record %p [%s], number of "
         "records %zu\n",
         typeRec.get(), typeRec->host.get(), recordCount));
    MutexAutoLock typeLock(typeRec->mResultsLock);
    typeRec->mResults = aResult;
    typeRec->SetExpiration(TimeStamp::NowLoRes(), aTtl, mDefaultGracePeriod);
    typeRec->negative = false;
    Telemetry::Accumulate(Telemetry::DNS_BY_TYPE_SUCCEEDED_LOOKUP_TIME,
                          duration);
  }

  mozilla::LinkedList<RefPtr<nsResolveHostCallback>> cbs =
      std::move(typeRec->mCallbacks);

  LOG(
      ("nsHostResolver::CompleteLookupByType record %p calling back dns "
       "users\n",
       typeRec.get()));

  for (nsResolveHostCallback* c = cbs.getFirst(); c;
       c = c->removeAndGetNext()) {
    c->OnResolveHostComplete(this, rec, status);
  }

  AddToEvictionQ(rec, aLock);
  return LOOKUP_OK;
}

void nsHostResolver::CancelAsyncRequest(
    const nsACString& host, const nsACString& aTrrServer, uint16_t aType,
    const OriginAttributes& aOriginAttributes, uint16_t flags, uint16_t af,
    nsIDNSListener* aListener, nsresult status)

{
  MutexAutoLock lock(mLock);

  nsAutoCString originSuffix;
  aOriginAttributes.CreateSuffix(originSuffix);

  // Lookup the host record associated with host, flags & address family

  nsHostKey key(host, aTrrServer, aType, flags, af,
                (aOriginAttributes.mPrivateBrowsingId > 0), originSuffix);
  RefPtr<nsHostRecord> rec = mRecordDB.Get(key);
  if (!rec) {
    return;
  }

  for (RefPtr<nsResolveHostCallback> c : rec->mCallbacks) {
    if (c->EqualsAsyncListener(aListener)) {
      c->remove();
      c->OnResolveHostComplete(this, rec.get(), status);
      break;
    }
  }

  // If there are no more callbacks, remove the hash table entry
  if (rec->mCallbacks.isEmpty()) {
    mRecordDB.Remove(*static_cast<nsHostKey*>(rec.get()));
    // If record is on a Queue, remove it
    mQueue.MaybeRemoveFromQ(rec, lock);
  }
}

size_t nsHostResolver::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const {
  MutexAutoLock lock(mLock);

  size_t n = mallocSizeOf(this);

  n += mRecordDB.ShallowSizeOfExcludingThis(mallocSizeOf);
  for (const auto& entry : mRecordDB.Values()) {
    n += entry->SizeOfIncludingThis(mallocSizeOf);
  }

  // The following fields aren't measured.
  // - mHighQ, mMediumQ, mLowQ, mEvictionQ, because they just point to
  //   nsHostRecords that also pointed to by entries |mRecordDB|, and
  //   measured when |mRecordDB| is measured.

  return n;
}

void nsHostResolver::ThreadFunc() {
  LOG(("DNS lookup thread - starting execution.\n"));

#if defined(RES_RETRY_ON_FAILURE)
  nsResState rs;
#endif
  RefPtr<AddrHostRecord> rec;
  RefPtr<AddrInfo> ai;

  do {
    if (!rec) {
      RefPtr<AddrHostRecord> tmpRec;
      if (!GetHostToLookup(getter_AddRefs(tmpRec))) {
        break;  // thread shutdown signal
      }
      // GetHostToLookup() returns an owning reference
      MOZ_ASSERT(tmpRec);
      rec.swap(tmpRec);
    }

    LOG1(("DNS lookup thread - Calling getaddrinfo for host [%s].\n",
          rec->host.get()));

    TimeStamp startTime = TimeStamp::Now();
    bool getTtl = rec->LoadGetTtl();
    TimeDuration inQueue = startTime - rec->mNativeStart;
    uint32_t ms = static_cast<uint32_t>(inQueue.ToMilliseconds());
    Telemetry::Accumulate(Telemetry::DNS_NATIVE_QUEUING, ms);
    nsresult status =
        GetAddrInfo(rec->host, rec->af, rec->flags, getter_AddRefs(ai), getTtl);
#if defined(RES_RETRY_ON_FAILURE)
    if (NS_FAILED(status) && rs.Reset()) {
      status = GetAddrInfo(rec->host, rec->af, rec->flags, getter_AddRefs(ai),
                           getTtl);
    }
#endif

    {  // obtain lock to check shutdown and manage inter-module telemetry
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

    LOG1(("DNS lookup thread - lookup completed for host [%s]: %s.\n",
          rec->host.get(), ai ? "success" : "failure: unknown host"));

    if (LOOKUP_RESOLVEAGAIN ==
        CompleteLookup(rec, status, ai, rec->pb, rec->originSuffix,
                       rec->mTRRSkippedReason, nullptr)) {
      // leave 'rec' assigned and loop to make a renewed host resolve
      LOG(("DNS lookup thread - Re-resolving host [%s].\n", rec->host.get()));
    } else {
      rec = nullptr;
    }
  } while (true);

  mActiveTaskCount--;
  LOG(("DNS lookup thread - queue empty, task finished.\n"));
}

void nsHostResolver::SetCacheLimits(uint32_t aMaxCacheEntries,
                                    uint32_t aDefaultCacheEntryLifetime,
                                    uint32_t aDefaultGracePeriod) {
  MutexAutoLock lock(mLock);
  mMaxCacheEntries = aMaxCacheEntries;
  mDefaultCacheLifetime = aDefaultCacheEntryLifetime;
  mDefaultGracePeriod = aDefaultGracePeriod;
}

nsresult nsHostResolver::Create(uint32_t maxCacheEntries,
                                uint32_t defaultCacheEntryLifetime,
                                uint32_t defaultGracePeriod,
                                nsHostResolver** result) {
  RefPtr<nsHostResolver> res = new nsHostResolver(
      maxCacheEntries, defaultCacheEntryLifetime, defaultGracePeriod);

  nsresult rv = res->Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  res.forget(result);
  return NS_OK;
}

void nsHostResolver::GetDNSCacheEntries(nsTArray<DNSCacheEntries>* args) {
  MutexAutoLock lock(mLock);
  for (const auto& recordEntry : mRecordDB) {
    // We don't pay attention to address literals, only resolved domains.
    // Also require a host.
    nsHostRecord* rec = recordEntry.GetWeak();
    MOZ_ASSERT(rec, "rec should never be null here!");

    if (!rec) {
      continue;
    }

    // For now we only show A/AAAA records.
    if (!rec->IsAddrRecord()) {
      continue;
    }

    RefPtr<AddrHostRecord> addrRec = do_QueryObject(rec);
    MOZ_ASSERT(addrRec);
    if (!addrRec || !addrRec->addr_info) {
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
      MutexAutoLock lock(addrRec->addr_info_lock);
      for (const auto& addr : addrRec->addr_info->Addresses()) {
        char buf[kIPv6CStrBufSize];
        if (addr.ToStringBuffer(buf, sizeof(buf))) {
          info.hostaddr.AppendElement(buf);
        }
      }
      info.TRR = addrRec->addr_info->IsTRROrODoH();
    }

    info.originAttributesSuffix = recordEntry.GetKey().originSuffix;
    info.flags = nsPrintfCString("%u|0x%x|%u|%d|%s", rec->type, rec->flags,
                                 rec->af, rec->pb, rec->mTrrServer.get());

    args->AppendElement(std::move(info));
  }
}

#undef LOG
#undef LOG_ENABLED
