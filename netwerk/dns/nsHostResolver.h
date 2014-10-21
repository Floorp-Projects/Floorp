/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHostResolver_h__
#define nsHostResolver_h__

#include "nscore.h"
#include "prclist.h"
#include "prnetdb.h"
#include "pldhash.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "nsISupportsImpl.h"
#include "nsIDNSListener.h"
#include "nsString.h"
#include "nsTArray.h"
#include "GetAddrInfo.h"
#include "mozilla/net/DNS.h"
#include "mozilla/net/DashboardTypes.h"
#include "mozilla/TimeStamp.h"

class nsHostResolver;
class nsHostRecord;
class nsResolveHostCallback;

#define MAX_RESOLVER_THREADS_FOR_ANY_PRIORITY  3
#define MAX_RESOLVER_THREADS_FOR_HIGH_PRIORITY 5
#define MAX_NON_PRIORITY_REQUESTS 150

#define MAX_RESOLVER_THREADS (MAX_RESOLVER_THREADS_FOR_ANY_PRIORITY + \
                              MAX_RESOLVER_THREADS_FOR_HIGH_PRIORITY)

struct nsHostKey
{
    const char *host;
    uint16_t    flags;
    uint16_t    af;
};

/**
 * nsHostRecord - ref counted object type stored in host resolver cache.
 */
class nsHostRecord : public PRCList, public nsHostKey
{
    typedef mozilla::Mutex Mutex;

public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsHostRecord)

    /* instantiates a new host record */
    static nsresult Create(const nsHostKey *key, nsHostRecord **record);

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
     * nsHostRecord (and only in nsHostResolver::OnLookupComplete);
     * the other threads just read it.  therefore the resolver worker
     * thread doesn't need to lock when reading |addr_info|.
     */
    Mutex        addr_info_lock;
    int          addr_info_gencnt; /* generation count of |addr_info| */
    mozilla::net::AddrInfo *addr_info;
    mozilla::net::NetAddr  *addr;
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

    // Convenience function for setting the timestamps above (mValidStart,
    // mValidEnd, and mGraceStart). valid and grace are durations in seconds.
    void SetExpiration(const mozilla::TimeStamp& now, unsigned int valid,
                       unsigned int grace);

    // Checks if the record is usable (not expired and has a value)
    bool HasUsableResult(const mozilla::TimeStamp& now, uint16_t queryFlags = 0) const;

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

private:
    friend class nsHostResolver;


    PRCList callbacks; /* list of callbacks */

    bool    resolving; /* true if this record is being resolved, which means
                        * that it is either on the pending queue or owned by
                        * one of the worker threads. */

    bool    onQueue;  /* true if pending and on the queue (not yet given to getaddrinfo())*/
    bool    usingAnyThread; /* true if off queue and contributing to mActiveAnyThreadCount */
    bool    mDoomed; /* explicitly expired */

#if TTL_AVAILABLE
    bool    mGetTtl;
#endif

    // The number of times ReportUnusable() has been called in the record's
    // lifetime.
    uint32_t mBlacklistedCount;

    // when the results from this resolve is returned, it is not to be
    // trusted, but instead a new resolve must be made!
    bool    mResolveAgain;

    // a list of addresses associated with this record that have been reported
    // as unusable. the list is kept as a set of strings to make it independent
    // of gencnt.
    nsTArray<nsCString> mBlacklistedItems;

    explicit nsHostRecord(const nsHostKey *key);           /* use Create() instead */
   ~nsHostRecord();
};

/**
 * ResolveHost callback object.  It's PRCList members are used by
 * the nsHostResolver and should not be used by anything else.
 */
class NS_NO_VTABLE nsResolveHostCallback : public PRCList
{
public:
    /**
     * OnLookupComplete
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
    virtual void OnLookupComplete(nsHostResolver *resolver,
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
};

/**
 * nsHostResolver - an asynchronous host name resolver.
 */
class nsHostResolver
{
    typedef mozilla::CondVar CondVar;
    typedef mozilla::Mutex Mutex;

public:
    /**
     * host resolver instances are reference counted.
     */
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsHostResolver)

    /**
     * creates an addref'd instance of a nsHostResolver object.
     */
    static nsresult Create(uint32_t maxCacheEntries, // zero disables cache
                           uint32_t defaultCacheEntryLifetime, // seconds
                           uint32_t defaultGracePeriod, // seconds
                           nsHostResolver **resolver);
    
    /**
     * puts the resolver in the shutdown state, which will cause any pending
     * callbacks to be detached.  any future calls to ResolveHost will fail.
     */
    void Shutdown();

    /**
     * resolve the given hostname asynchronously.  the caller can synthesize
     * a synchronous host lookup using a lock and a cvar.  as noted above
     * the callback will occur re-entrantly from an unspecified thread.  the
     * host lookup cannot be canceled (cancelation can be layered above this
     * by having the callback implementation return without doing anything).
     */
    nsresult ResolveHost(const char            *hostname,
                         uint16_t               flags,
                         uint16_t               af,
                         nsResolveHostCallback *callback);

    /**
     * removes the specified callback from the nsHostRecord for the given
     * hostname, flags, and address family.  these parameters should correspond
     * to the parameters passed to ResolveHost.  this function executes the
     * callback if the callback is still pending with the given status.
     */
    void DetachCallback(const char            *hostname,
                        uint16_t               flags,
                        uint16_t               af,
                        nsResolveHostCallback *callback,
                        nsresult               status);

    /**
     * Cancels an async request associated with the hostname, flags,
     * address family and listener.  Cancels first callback found which matches
     * these criteria.  These parameters should correspond to the parameters
     * passed to ResolveHost.  If this is the last callback associated with the
     * host record, it is removed from any request queues it might be on. 
     */
    void CancelAsyncRequest(const char            *host,
                            uint16_t               flags,
                            uint16_t               af,
                            nsIDNSListener        *aListener,
                            nsresult               status);
    /**
     * values for the flags parameter passed to ResolveHost and DetachCallback
     * that may be bitwise OR'd together.
     *
     * NOTE: in this implementation, these flags correspond exactly in value
     *       to the flags defined on nsIDNSService.
     */
    enum {
        RES_BYPASS_CACHE = 1 << 0,
        RES_CANON_NAME   = 1 << 1,
        RES_PRIORITY_MEDIUM   = 1 << 2,
        RES_PRIORITY_LOW  = 1 << 3,
        RES_SPECULATE     = 1 << 4,
        //RES_DISABLE_IPV6 = 1 << 5, // Not used
        RES_OFFLINE       = 1 << 6
    };

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    /**
     * Flush the DNS cache.
     */
    void FlushCache();

private:
   explicit nsHostResolver(uint32_t maxCacheEntries,
                           uint32_t defaultCacheEntryLifetime,
                           uint32_t defaultGracePeriod);
   ~nsHostResolver();

    nsresult Init();
    nsresult IssueLookup(nsHostRecord *);
    bool     GetHostToLookup(nsHostRecord **m);

    enum LookupStatus {
      LOOKUP_OK,
      LOOKUP_RESOLVEAGAIN,
    };

    LookupStatus OnLookupComplete(nsHostRecord *, nsresult, mozilla::net::AddrInfo *);
    void     DeQueue(PRCList &aQ, nsHostRecord **aResult);
    void     ClearPendingQueue(PRCList *aPendingQueue);
    nsresult ConditionallyCreateThread(nsHostRecord *rec);

    /**
     * Starts a new lookup in the background for entries that are in the grace
     * period with a failed connect or all cached entries are negative.
     */
    nsresult ConditionallyRefreshRecord(nsHostRecord *rec, const char *host);

    static void  MoveQueue(nsHostRecord *aRec, PRCList &aDestQ);
    
    static void ThreadFunc(void *);

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
    uint32_t      mNumIdleThreads;
    uint32_t      mThreadCount;
    uint32_t      mActiveAnyThreadCount;
    PLDHashTable  mDB;
    PRCList       mHighQ;
    PRCList       mMediumQ;
    PRCList       mLowQ;
    PRCList       mEvictionQ;
    uint32_t      mEvictionQSize;
    uint32_t      mPendingCount;
    PRTime        mCreationTime;
    bool          mShutdown;
    PRIntervalTime mLongIdleTimeout;
    PRIntervalTime mShortIdleTimeout;

    // Set the expiration time stamps appropriately.
    void PrepareRecordExpiration(nsHostRecord* rec) const;

public:
    /*
     * Called by the networking dashboard via the DnsService2
     */
    void GetDNSCacheEntries(nsTArray<mozilla::net::DNSCacheEntries> *);
};

#endif // nsHostResolver_h__
