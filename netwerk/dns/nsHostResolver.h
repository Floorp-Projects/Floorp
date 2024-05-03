/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHostResolver_h__
#define nsHostResolver_h__

#include "nscore.h"
#include "prnetdb.h"
#include "PLDHashTable.h"
#include "mozilla/CondVar.h"
#include "mozilla/DataMutex.h"
#include "nsISupportsImpl.h"
#include "nsIDNSListener.h"
#include "nsTArray.h"
#include "GetAddrInfo.h"
#include "HostRecordQueue.h"
#include "mozilla/net/DNS.h"
#include "mozilla/net/DashboardTypes.h"
#include "mozilla/Atomics.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsHostRecord.h"
#include "nsRefPtrHashtable.h"
#include "nsIThreadPool.h"
#include "mozilla/net/NetworkConnectivityService.h"
#include "mozilla/net/DNSByTypeRecord.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_network.h"

namespace mozilla {
namespace net {
class TRR;
class TRRQuery;

static inline uint32_t MaxResolverThreadsAnyPriority() {
  return StaticPrefs::network_dns_max_any_priority_threads();
}

static inline uint32_t MaxResolverThreadsHighPriority() {
  return StaticPrefs::network_dns_max_high_priority_threads();
}

static inline uint32_t MaxResolverThreads() {
  return MaxResolverThreadsAnyPriority() + MaxResolverThreadsHighPriority();
}

}  // namespace net
}  // namespace mozilla

#define TRR_DISABLED(x)                       \
  (((x) == nsIDNSService::MODE_NATIVEONLY) || \
   ((x) == nsIDNSService::MODE_TRROFF))

extern mozilla::Atomic<bool, mozilla::Relaxed> gNativeIsLocalhost;

#define MAX_NON_PRIORITY_REQUESTS 150

class AHostResolver {
 public:
  AHostResolver() = default;
  virtual ~AHostResolver() = default;
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  enum LookupStatus {
    LOOKUP_OK,
    LOOKUP_RESOLVEAGAIN,
  };

  virtual LookupStatus CompleteLookup(nsHostRecord*, nsresult,
                                      mozilla::net::AddrInfo*, bool pb,
                                      const nsACString& aOriginsuffix,
                                      mozilla::net::TRRSkippedReason aReason,
                                      mozilla::net::TRR*) = 0;
  virtual LookupStatus CompleteLookupByType(
      nsHostRecord*, nsresult, mozilla::net::TypeRecordResultType& aResult,
      mozilla::net::TRRSkippedReason aReason, uint32_t aTtl, bool pb) = 0;
  virtual nsresult GetHostRecord(const nsACString& host,
                                 const nsACString& aTrrServer, uint16_t type,
                                 nsIDNSService::DNSFlags flags, uint16_t af,
                                 bool pb, const nsCString& originSuffix,
                                 nsHostRecord** result) {
    return NS_ERROR_FAILURE;
  }
  virtual nsresult TrrLookup_unlocked(nsHostRecord*,
                                      mozilla::net::TRR* pushedTRR = nullptr) {
    return NS_ERROR_FAILURE;
  }
  virtual void MaybeRenewHostRecord(nsHostRecord* aRec) {}
};

/**
 * nsHostResolver - an asynchronous host name resolver.
 */
class nsHostResolver : public nsISupports, public AHostResolver {
  using CondVar = mozilla::CondVar;
  using Mutex = mozilla::Mutex;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  /**
   * creates an addref'd instance of a nsHostResolver object.
   */
  static nsresult Create(uint32_t maxCacheEntries,  // zero disables cache
                         uint32_t defaultCacheEntryLifetime,  // seconds
                         uint32_t defaultGracePeriod,         // seconds
                         nsHostResolver** result);

  /**
   * Set (new) cache limits.
   */
  void SetCacheLimits(uint32_t maxCacheEntries,  // zero disables cache
                      uint32_t defaultCacheEntryLifetime,  // seconds
                      uint32_t defaultGracePeriod);        // seconds

  /**
   * puts the resolver in the shutdown state, which will cause any pending
   * callbacks to be detached.  any future calls to ResolveHost will fail.
   */
  void Shutdown();

  /**
   * resolve the given hostname and originAttributes asynchronously.  the caller
   * can synthesize a synchronous host lookup using a lock and a cvar.  as noted
   * above the callback will occur re-entrantly from an unspecified thread.  the
   * host lookup cannot be canceled (cancelation can be layered above this by
   * having the callback implementation return without doing anything).
   */
  nsresult ResolveHost(const nsACString& aHost, const nsACString& trrServer,
                       int32_t aPort, uint16_t type,
                       const mozilla::OriginAttributes& aOriginAttributes,
                       nsIDNSService::DNSFlags flags, uint16_t af,
                       nsResolveHostCallback* callback);

  nsHostRecord* InitRecord(const nsHostKey& key);
  mozilla::net::NetworkConnectivityService* GetNCS() {
    return mNCS;
  }  // This is actually a singleton

  /**
   * return a resolved hard coded loopback dns record for the specified key
   */
  already_AddRefed<nsHostRecord> InitLoopbackRecord(const nsHostKey& key,
                                                    nsresult* aRv);

  /**
   * removes the specified callback from the nsHostRecord for the given
   * hostname, originAttributes, flags, and address family.  these parameters
   * should correspond to the parameters passed to ResolveHost.  this function
   * executes the callback if the callback is still pending with the given
   * status.
   */
  void DetachCallback(const nsACString& hostname, const nsACString& trrServer,
                      uint16_t type,
                      const mozilla::OriginAttributes& aOriginAttributes,
                      nsIDNSService::DNSFlags flags, uint16_t af,
                      nsResolveHostCallback* callback, nsresult status);

  /**
   * Cancels an async request associated with the hostname, originAttributes,
   * flags, address family and listener.  Cancels first callback found which
   * matches these criteria.  These parameters should correspond to the
   * parameters passed to ResolveHost.  If this is the last callback associated
   * with the host record, it is removed from any request queues it might be on.
   */
  void CancelAsyncRequest(const nsACString& host, const nsACString& trrServer,
                          uint16_t type,
                          const mozilla::OriginAttributes& aOriginAttributes,
                          nsIDNSService::DNSFlags flags, uint16_t af,
                          nsIDNSListener* aListener, nsresult status);
  /**
   * values for the flags parameter passed to ResolveHost and DetachCallback
   * that may be bitwise OR'd together.
   *
   * NOTE: in this implementation, these flags correspond exactly in value
   *       to the flags defined on nsIDNSService.
   */
  enum {
    RES_BYPASS_CACHE = nsIDNSService::RESOLVE_BYPASS_CACHE,
    RES_CANON_NAME = nsIDNSService::RESOLVE_CANONICAL_NAME,
    RES_PRIORITY_MEDIUM = nsHostRecord::DNS_PRIORITY_MEDIUM,
    RES_PRIORITY_LOW = nsHostRecord::DNS_PRIORITY_LOW,
    RES_SPECULATE = nsIDNSService::RESOLVE_SPECULATE,
    // RES_DISABLE_IPV6 = nsIDNSService::RESOLVE_DISABLE_IPV6, // Not used
    RES_OFFLINE = nsIDNSService::RESOLVE_OFFLINE,
    // RES_DISABLE_IPv4 = nsIDNSService::RESOLVE_DISABLE_IPV4, // Not Used
    RES_ALLOW_NAME_COLLISION = nsIDNSService::RESOLVE_ALLOW_NAME_COLLISION,
    RES_DISABLE_TRR = nsIDNSService::RESOLVE_DISABLE_TRR,
    RES_REFRESH_CACHE = nsIDNSService::RESOLVE_REFRESH_CACHE,
    RES_IP_HINT = nsIDNSService::RESOLVE_IP_HINT
  };

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  /**
   * Flush the DNS cache.
   */
  void FlushCache(bool aTrrToo);

  LookupStatus CompleteLookup(nsHostRecord*, nsresult, mozilla::net::AddrInfo*,
                              bool pb, const nsACString& aOriginsuffix,
                              mozilla::net::TRRSkippedReason aReason,
                              mozilla::net::TRR* aTRRRequest) override;
  LookupStatus CompleteLookupByType(nsHostRecord*, nsresult,
                                    mozilla::net::TypeRecordResultType& aResult,
                                    mozilla::net::TRRSkippedReason aReason,
                                    uint32_t aTtl, bool pb) override;
  nsresult GetHostRecord(const nsACString& host, const nsACString& trrServer,
                         uint16_t type, nsIDNSService::DNSFlags flags,
                         uint16_t af, bool pb, const nsCString& originSuffix,
                         nsHostRecord** result) override;
  nsresult TrrLookup_unlocked(nsHostRecord*,
                              mozilla::net::TRR* pushedTRR = nullptr) override;
  static nsIDNSService::ResolverMode Mode();

  virtual void MaybeRenewHostRecord(nsHostRecord* aRec) override;

  // Records true if the TRR service is enabled for the record's effective
  // TRR mode. Also records the TRRSkipReason when the TRR service is not
  // available/enabled.
  bool TRRServiceEnabledForRecord(nsHostRecord* aRec) MOZ_REQUIRES(mLock);

 private:
  explicit nsHostResolver(uint32_t maxCacheEntries,
                          uint32_t defaultCacheEntryLifetime,
                          uint32_t defaultGracePeriod);
  virtual ~nsHostResolver();

  bool DoRetryTRR(AddrHostRecord* aAddrRec,
                  const mozilla::MutexAutoLock& aLock);
  bool MaybeRetryTRRLookup(
      AddrHostRecord* aAddrRec, nsresult aFirstAttemptStatus,
      mozilla::net::TRRSkippedReason aFirstAttemptSkipReason,
      nsresult aChannelStatus, const mozilla::MutexAutoLock& aLock);

  LookupStatus CompleteLookupLocked(nsHostRecord*, nsresult,
                                    mozilla::net::AddrInfo*, bool pb,
                                    const nsACString& aOriginsuffix,
                                    mozilla::net::TRRSkippedReason aReason,
                                    mozilla::net::TRR* aTRRRequest,
                                    const mozilla::MutexAutoLock& aLock)
      MOZ_REQUIRES(mLock);
  LookupStatus CompleteLookupByTypeLocked(
      nsHostRecord*, nsresult, mozilla::net::TypeRecordResultType& aResult,
      mozilla::net::TRRSkippedReason aReason, uint32_t aTtl, bool pb,
      const mozilla::MutexAutoLock& aLock) MOZ_REQUIRES(mLock);
  nsresult Init();
  static void ComputeEffectiveTRRMode(nsHostRecord* aRec);
  nsresult NativeLookup(nsHostRecord* aRec,
                        const mozilla::MutexAutoLock& aLock);
  nsresult TrrLookup(nsHostRecord*, const mozilla::MutexAutoLock& aLock,
                     mozilla::net::TRR* pushedTRR = nullptr);

  // Kick-off a name resolve operation, using native resolver and/or TRR
  nsresult NameLookup(nsHostRecord* aRec, const mozilla::MutexAutoLock& aLock);
  bool GetHostToLookup(nsHostRecord** result);
  void MaybeRenewHostRecordLocked(nsHostRecord* aRec,
                                  const mozilla::MutexAutoLock& aLock)
      MOZ_REQUIRES(mLock);

  // Cancels host records in the pending queue and also
  // calls CompleteLookup with the NS_ERROR_ABORT result code.
  void ClearPendingQueue(mozilla::LinkedList<RefPtr<nsHostRecord>>& aPendingQ);
  nsresult ConditionallyCreateThread(nsHostRecord* rec) MOZ_REQUIRES(mLock);

  /**
   * Starts a new lookup in the background for entries that are in the grace
   * period with a failed connect or all cached entries are negative.
   */
  nsresult ConditionallyRefreshRecord(nsHostRecord* rec, const nsACString& host,
                                      const mozilla::MutexAutoLock& aLock)
      MOZ_REQUIRES(mLock);

  void OnResolveComplete(nsHostRecord* aRec,
                         const mozilla::MutexAutoLock& aLock)
      MOZ_REQUIRES(mLock);

  void AddToEvictionQ(nsHostRecord* rec, const mozilla::MutexAutoLock& aLock)
      MOZ_REQUIRES(mLock);

  void ThreadFunc();

  // Resolve the host from the DNS cache.
  already_AddRefed<nsHostRecord> FromCache(nsHostRecord* aRec,
                                           const nsACString& aHost,
                                           uint16_t aType, nsresult& aStatus,
                                           const mozilla::MutexAutoLock& aLock)
      MOZ_REQUIRES(mLock);
  // Called when the host name is an IP address and has been passed.
  already_AddRefed<nsHostRecord> FromCachedIPLiteral(nsHostRecord* aRec);
  // Like the above function, but the host name is not parsed to NetAddr yet.
  already_AddRefed<nsHostRecord> FromIPLiteral(
      AddrHostRecord* aAddrRec, const mozilla::net::NetAddr& aAddr);
  // Called to check if we have an AF_UNSPEC entry in the cache.
  already_AddRefed<nsHostRecord> FromUnspecEntry(
      nsHostRecord* aRec, const nsACString& aHost, const nsACString& aTrrServer,
      const nsACString& aOriginSuffix, uint16_t aType,
      nsIDNSService::DNSFlags aFlags, uint16_t af, bool aPb, nsresult& aStatus)
      MOZ_REQUIRES(mLock);

  enum {
    METHOD_HIT = 1,
    METHOD_RENEWAL = 2,
    METHOD_NEGATIVE_HIT = 3,
    METHOD_LITERAL = 4,
    METHOD_OVERFLOW = 5,
    METHOD_NETWORK_FIRST = 6,
    METHOD_NETWORK_SHARED = 7
  };

  uint32_t mMaxCacheEntries = 0;
  uint32_t mDefaultCacheLifetime = 0;  // granularity seconds
  uint32_t mDefaultGracePeriod = 0;    // granularity seconds
  // mutable so SizeOfIncludingThis can be const
  mutable Mutex mLock{"nsHostResolver.mLock"};
  CondVar mIdleTaskCV;
  nsRefPtrHashtable<nsGenericHashKey<nsHostKey>, nsHostRecord> mRecordDB
      MOZ_GUARDED_BY(mLock);
  PRTime mCreationTime;
  mozilla::TimeDuration mLongIdleTimeout;
  mozilla::TimeDuration mShortIdleTimeout;

  RefPtr<nsIThreadPool> mResolverThreads;
  RefPtr<mozilla::net::NetworkConnectivityService>
      mNCS;  // reference to a singleton
  mozilla::net::HostRecordQueue mQueue MOZ_GUARDED_BY(mLock);
  mozilla::Atomic<bool> mShutdown MOZ_GUARDED_BY(mLock){true};
  mozilla::Atomic<uint32_t> mNumIdleTasks MOZ_GUARDED_BY(mLock){0};
  mozilla::Atomic<uint32_t> mActiveTaskCount MOZ_GUARDED_BY(mLock){0};
  mozilla::Atomic<uint32_t> mActiveAnyThreadCount MOZ_GUARDED_BY(mLock){0};

  // Set the expiration time stamps appropriately.
  void PrepareRecordExpirationAddrRecord(AddrHostRecord* rec) const;

 public:
  /*
   * Called by the networking dashboard via the DnsService2
   */
  void GetDNSCacheEntries(nsTArray<mozilla::net::DNSCacheEntries>*);

  static bool IsNativeHTTPSEnabled();
};

#endif  // nsHostResolver_h__
