/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHostResolver_h__
#define nsHostResolver_h__

#include "nscore.h"
#include "prnetdb.h"
#include "PLDHashTable.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "nsISupportsImpl.h"
#include "nsIDNSListener.h"
#include "nsIDNSService.h"
#include "nsTArray.h"
#include "GetAddrInfo.h"
#include "mozilla/net/DNS.h"
#include "mozilla/net/DashboardTypes.h"
#include "mozilla/Atomics.h"
#include "mozilla/LinkedList.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsRefPtrHashtable.h"

class nsHostResolver;
class nsResolveHostCallback;
namespace mozilla { namespace net {
class TRR;
enum ResolverMode {
  MODE_NATIVEONLY, // 0 - TRR OFF (by default)
  MODE_PARALLEL,   // 1 - race and use the first response
  MODE_TRRFIRST,   // 2 - fallback to native on TRR failure
  MODE_TRRONLY,    // 3 - don't even fallback
  MODE_SHADOW,     // 4 - race for stats, but always use native result
  MODE_TRROFF      // 5 - identical to MODE_NATIVEONLY but explicitly selected
};
} }

#define TRR_DISABLED(x) (((x) == MODE_NATIVEONLY) || ((x) == MODE_TRROFF))

extern mozilla::Atomic<bool, mozilla::Relaxed> gNativeIsLocalhost;

#define MAX_RESOLVER_THREADS_FOR_ANY_PRIORITY  3
#define MAX_RESOLVER_THREADS_FOR_HIGH_PRIORITY 5
#define MAX_NON_PRIORITY_REQUESTS 150

#define MAX_RESOLVER_THREADS (MAX_RESOLVER_THREADS_FOR_ANY_PRIORITY + \
                              MAX_RESOLVER_THREADS_FOR_HIGH_PRIORITY)

struct nsHostKey
{
    const nsCString host;
    uint16_t flags;
    uint16_t af;
    bool     pb;
    const nsCString originSuffix;

    nsHostKey(const nsACString& host, uint16_t flags,
              uint16_t af, bool pb, const nsACString& originSuffix)
        : host(host)
        , flags(flags)
        , af(af)
        , pb(pb)
        , originSuffix(originSuffix) {
    }

    bool operator==(const nsHostKey& other) const;
    size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
    PLDHashNumber Hash() const;
};

/**
 * nsHostRecord - ref counted object type stored in host resolver cache.
 */
class nsHostRecord :
    public mozilla::LinkedListElement<RefPtr<nsHostRecord>>,
    public nsHostKey
{
    typedef mozilla::Mutex Mutex;

public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsHostRecord)

    /* a fully resolved host record has either a non-null |addr_info| or |addr|
     * field.  if |addr_info| is null, it implies that the |host| is an IP
     * address literal.  in which case, |addr| contains the parsed address.
     * otherwise, if |addr_info| is non-null, then it contains one or many
     * IP addresses corresponding to the given host name.  if both |addr_info|
     * and |addr| are null, then the given host has not yet been fully resolved.
     * |af| is the address family of the record we are querying for.
     */

    /* the lock protects |addr_info| and |addr_info_gencnt| because they
     * are mutable and accessed by the resolver worker thread and the
     * nsDNSService2 class.  |addr| doesn't change after it has been
     * assigned a value.  only the resolver worker thread modifies
     * nsHostRecord (and only in nsHostResolver::CompleteLookup);
     * the other threads just read it.  therefore the resolver worker
     * thread doesn't need to lock when reading |addr_info|.
     */
    Mutex        addr_info_lock;
    int          addr_info_gencnt; /* generation count of |addr_info| */
    mozilla::net::AddrInfo *addr_info;
    mozilla::UniquePtr<mozilla::net::NetAddr> addr;
    bool         negative;   /* True if this record is a cache of a failed lookup.
                                Negative cache entries are valid just like any other
                                (though never for more than 60 seconds), but a use
                                of that negative entry forces an asynchronous refresh. */

    enum ExpirationStatus {
        EXP_VALID,
        EXP_GRACE,
        EXP_EXPIRED,
    };

    ExpirationStatus CheckExpiration(const mozilla::TimeStamp& now) const;

    // When the record began being valid. Used mainly for bookkeeping.
    mozilla::TimeStamp mValidStart;

    // When the record is no longer valid (it's time of expiration)
    mozilla::TimeStamp mValidEnd;

    // When the record enters its grace period. This must be before mValidEnd.
    // If a record is in its grace period (and not expired), it will be used
    // but a request to refresh it will be made.
    mozilla::TimeStamp mGraceStart;

    // When the lookups of this record started and their durations
    mozilla::TimeStamp mTrrStart;
    mozilla::TimeStamp mNativeStart;
    mozilla::TimeDuration mTrrDuration;
    mozilla::TimeDuration mNativeDuration;

    // Convenience function for setting the timestamps above (mValidStart,
    // mValidEnd, and mGraceStart). valid and grace are durations in seconds.
    void SetExpiration(const mozilla::TimeStamp& now, unsigned int valid,
                       unsigned int grace);
    void CopyExpirationTimesAndFlagsFrom(const nsHostRecord *aFromHostRecord);

    // Checks if the record is usable (not expired and has a value)
    bool HasUsableResult(const mozilla::TimeStamp& now, uint16_t queryFlags = 0) const;

    // Mark hostrecord as not usable
    void Invalidate();

    // hold addr_info_lock when calling the blacklist functions
    bool   Blacklisted(mozilla::net::NetAddr *query);
    void   ResetBlacklist();
    void   ReportUnusable(mozilla::net::NetAddr *addr);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    enum DnsPriority {
        DNS_PRIORITY_LOW,
        DNS_PRIORITY_MEDIUM,
        DNS_PRIORITY_HIGH,
    };
    static DnsPriority GetPriority(uint16_t aFlags);

    bool RemoveOrRefresh(); // Mark records currently being resolved as needed
                            // to resolve again.
    bool IsTRR() { return mTRRUsed; }
    void ResolveComplete();
    void Cancel();

    mozilla::net::ResolverMode mResolverMode;

private:
    friend class nsHostResolver;

    explicit nsHostRecord(const nsHostKey& key);
    mozilla::LinkedList<RefPtr<nsResolveHostCallback>> mCallbacks;
    nsAutoPtr<mozilla::net::AddrInfo> mFirstTRR; // partial TRR storage
    nsresult mFirstTRRresult;

    uint16_t  mResolving;  // counter of outstanding resolving calls
    uint8_t   mTRRSuccess; // number of successful TRR responses
    uint8_t   mNativeSuccess; // number of native lookup responses

    uint16_t    mNative : 1;     // true if this record is being resolved "natively",
                                 // which means that it is either on the pending queue
                                 // or owned by one of the worker threads. */
    uint16_t    mTRRUsed : 1;    // TRR was used on this record
    uint16_t    mNativeUsed : 1;
    uint16_t    onQueue : 1;    // true if pending and on the queue (not yet given to getaddrinfo())
    uint16_t    usingAnyThread : 1; // true if off queue and contributing to mActiveAnyThreadCount
    uint16_t    mDoomed : 1;    // explicitly expired
    uint16_t    mDidCallbacks : 1;
    uint16_t    mGetTtl : 1;

    // when the results from this resolve is returned, it is not to be
    // trusted, but instead a new resolve must be made!
    uint16_t    mResolveAgain : 1;

    enum {
        INIT, STARTED, OK, FAILED
    } mTrrAUsed, mTrrAAAAUsed;

    Mutex mTrrLock; // lock when accessing the mTrrA[AAA] pointers
    RefPtr<mozilla::net::TRR> mTrrA;
    RefPtr<mozilla::net::TRR> mTrrAAAA;

    // The number of times ReportUnusable() has been called in the record's
    // lifetime.
    uint32_t mBlacklistedCount;

    // a list of addresses associated with this record that have been reported
    // as unusable. the list is kept as a set of strings to make it independent
    // of gencnt.
    nsTArray<nsCString> mBlacklistedItems;

   ~nsHostRecord();
};

/**
 * This class is used to notify listeners when a ResolveHost operation is
 * complete. Classes that derive it must implement threadsafe nsISupports
 * to be able to use RefPtr with this class.
 */
class nsResolveHostCallback
    : public mozilla::LinkedListElement<RefPtr<nsResolveHostCallback>>
    , public nsISupports
{
public:
    /**
     * OnResolveHostComplete
     *
     * this function is called to complete a host lookup initiated by
     * nsHostResolver::ResolveHost.  it may be invoked recursively from
     * ResolveHost or on an unspecified background thread.
     *
     * NOTE: it is the responsibility of the implementor of this method
     * to handle the callback in a thread safe manner.
     *
     * @param resolver
     *        nsHostResolver object associated with this result
     * @param record
     *        the host record containing the results of the lookup
     * @param status
     *        if successful, |record| contains non-null results
     */
    virtual void OnResolveHostComplete(nsHostResolver *resolver,
                                       nsHostRecord   *record,
                                       nsresult        status) = 0;
    /**
     * EqualsAsyncListener
     *
     * Determines if the listener argument matches the listener member var.
     * For subclasses not implementing a member listener, should return false.
     * For subclasses having a member listener, the function should check if
     * they are the same.  Used for cases where a pointer to an object
     * implementing nsResolveHostCallback is unknown, but a pointer to
     * the original listener is known.
     *
     * @param aListener
     *        nsIDNSListener object associated with the original request
     */
    virtual bool EqualsAsyncListener(nsIDNSListener *aListener) = 0;

    virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf) const = 0;
protected:
    virtual ~nsResolveHostCallback() = default;
};

class AHostResolver
{
public:
    AHostResolver() = default;
    virtual ~AHostResolver() = default;
    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

     enum LookupStatus {
        LOOKUP_OK,
        LOOKUP_RESOLVEAGAIN,
    };

    virtual LookupStatus CompleteLookup(nsHostRecord *, nsresult, mozilla::net::AddrInfo *, bool pb) = 0;
    virtual nsresult GetHostRecord(const nsACString &host,
                                   uint16_t flags, uint16_t af, bool pb,
                                   const nsCString &originSuffix,
                                   nsHostRecord **result)
    {
        return NS_ERROR_FAILURE;
    }
    virtual nsresult TrrLookup_unlocked(nsHostRecord *, mozilla::net::TRR *pushedTRR = nullptr)
    {
        return NS_ERROR_FAILURE;
    }
};

/**
 * nsHostResolver - an asynchronous host name resolver.
 */
class nsHostResolver : public nsISupports, public AHostResolver
{
    typedef mozilla::CondVar CondVar;
    typedef mozilla::Mutex Mutex;

public:
    NS_DECL_THREADSAFE_ISUPPORTS

    /**
     * creates an addref'd instance of a nsHostResolver object.
     */
    static nsresult Create(uint32_t maxCacheEntries, // zero disables cache
                           uint32_t defaultCacheEntryLifetime, // seconds
                           uint32_t defaultGracePeriod, // seconds
                           nsHostResolver **resolver);

    /**
     * Set (new) cache limits.
     */
    void SetCacheLimits(uint32_t maxCacheEntries, // zero disables cache
                        uint32_t defaultCacheEntryLifetime, // seconds
                        uint32_t defaultGracePeriod); // seconds

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
    nsresult ResolveHost(const nsACString &hostname,
                         const mozilla::OriginAttributes &aOriginAttributes,
                         uint16_t                         flags,
                         uint16_t                         af,
                         nsResolveHostCallback           *callback);

    /**
     * removes the specified callback from the nsHostRecord for the given
     * hostname, originAttributes, flags, and address family.  these parameters
     * should correspond to the parameters passed to ResolveHost.  this function
     * executes the callback if the callback is still pending with the given status.
     */
    void DetachCallback(const nsACString &hostname,
                        const mozilla::OriginAttributes &aOriginAttributes,
                        uint16_t                         flags,
                        uint16_t                         af,
                        nsResolveHostCallback           *callback,
                        nsresult                         status);

    /**
     * Cancels an async request associated with the hostname, originAttributes, flags,
     * address family and listener.  Cancels first callback found which matches
     * these criteria.  These parameters should correspond to the parameters
     * passed to ResolveHost.  If this is the last callback associated with the
     * host record, it is removed from any request queues it might be on.
     */
    void CancelAsyncRequest(const nsACString &host,
                            const mozilla::OriginAttributes &aOriginAttributes,
                            uint16_t                         flags,
                            uint16_t                         af,
                            nsIDNSListener                  *aListener,
                            nsresult                         status);
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
        RES_PRIORITY_MEDIUM = nsIDNSService::RESOLVE_PRIORITY_MEDIUM,
        RES_PRIORITY_LOW = nsIDNSService::RESOLVE_PRIORITY_LOW,
        RES_SPECULATE = nsIDNSService::RESOLVE_SPECULATE,
        //RES_DISABLE_IPV6 = nsIDNSService::RESOLVE_DISABLE_IPV6, // Not used
        RES_OFFLINE = nsIDNSService::RESOLVE_OFFLINE,
        //RES_DISABLE_IPv4 = nsIDNSService::RESOLVE_DISABLE_IPV4, // Not Used
        RES_ALLOW_NAME_COLLISION = nsIDNSService::RESOLVE_ALLOW_NAME_COLLISION,
        RES_DISABLE_TRR = nsIDNSService::RESOLVE_DISABLE_TRR,
        RES_REFRESH_CACHE = nsIDNSService::RESOLVE_REFRESH_CACHE
    };

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    /**
     * Flush the DNS cache.
     */
    void FlushCache();

    LookupStatus CompleteLookup(nsHostRecord *, nsresult, mozilla::net::AddrInfo *, bool pb) override;
    nsresult GetHostRecord(const nsACString &host,
                           uint16_t flags, uint16_t af, bool pb,
                           const nsCString &originSuffix,
                           nsHostRecord **result) override;
    nsresult TrrLookup_unlocked(nsHostRecord *, mozilla::net::TRR *pushedTRR = nullptr) override;

private:
   explicit nsHostResolver(uint32_t maxCacheEntries,
                           uint32_t defaultCacheEntryLifetime,
                           uint32_t defaultGracePeriod);
   virtual ~nsHostResolver();

    nsresult Init();
    // In debug builds it asserts that the element is in the list.
    void AssertOnQ(nsHostRecord *, mozilla::LinkedList<RefPtr<nsHostRecord>>&);
    mozilla::net::ResolverMode Mode();
    nsresult NativeLookup(nsHostRecord *);
    nsresult TrrLookup(nsHostRecord *, mozilla::net::TRR *pushedTRR = nullptr);

    // Kick-off a name resolve operation, using native resolver and/or TRR
    nsresult NameLookup(nsHostRecord *);
    bool     GetHostToLookup(nsHostRecord **m);

    // Removes the first element from the list and returns it AddRef-ed in aResult
    // Should not be called for an empty linked list.
    void     DeQueue(mozilla::LinkedList<RefPtr<nsHostRecord>>& aQ, nsHostRecord **aResult);
    // Cancels host records in the pending queue and also
    // calls CompleteLookup with the NS_ERROR_ABORT result code.
    void     ClearPendingQueue(mozilla::LinkedList<RefPtr<nsHostRecord>>& aPendingQ);
    nsresult ConditionallyCreateThread(nsHostRecord *rec);

    /**
     * Starts a new lookup in the background for entries that are in the grace
     * period with a failed connect or all cached entries are negative.
     */
    nsresult ConditionallyRefreshRecord(nsHostRecord *rec, const nsACString &host);

    void ThreadFunc();

    enum {
        METHOD_HIT = 1,
        METHOD_RENEWAL = 2,
        METHOD_NEGATIVE_HIT = 3,
        METHOD_LITERAL = 4,
        METHOD_OVERFLOW = 5,
        METHOD_NETWORK_FIRST = 6,
        METHOD_NETWORK_SHARED = 7
    };

    uint32_t      mMaxCacheEntries;
    uint32_t      mDefaultCacheLifetime; // granularity seconds
    uint32_t      mDefaultGracePeriod; // granularity seconds
    mutable Mutex mLock;    // mutable so SizeOfIncludingThis can be const
    CondVar       mIdleThreadCV;
    nsRefPtrHashtable<nsGenericHashKey<nsHostKey>, nsHostRecord> mRecordDB;
    mozilla::LinkedList<RefPtr<nsHostRecord>> mHighQ;
    mozilla::LinkedList<RefPtr<nsHostRecord>> mMediumQ;
    mozilla::LinkedList<RefPtr<nsHostRecord>> mLowQ;
    mozilla::LinkedList<RefPtr<nsHostRecord>> mEvictionQ;
    uint32_t      mEvictionQSize;
    PRTime        mCreationTime;
    mozilla::TimeDuration mLongIdleTimeout;
    mozilla::TimeDuration mShortIdleTimeout;

    mozilla::Atomic<bool>     mShutdown;
    mozilla::Atomic<uint32_t> mNumIdleThreads;
    mozilla::Atomic<uint32_t> mThreadCount;
    mozilla::Atomic<uint32_t> mActiveAnyThreadCount;
    mozilla::Atomic<uint32_t> mPendingCount;

    // Set the expiration time stamps appropriately.
    void PrepareRecordExpiration(nsHostRecord* rec) const;

public:
    /*
     * Called by the networking dashboard via the DnsService2
     */
    void GetDNSCacheEntries(nsTArray<mozilla::net::DNSCacheEntries> *);
};

#endif // nsHostResolver_h__
