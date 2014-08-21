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
#include "nsAutoPtr.h"
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

#if XP_WIN
// If this is enabled, we will make two queries to our resolver for unspec
// queries. One for any AAAA records, and the other for A records. We will then
// merge the results ourselves. This is done for us by getaddrinfo, but because
// getaddrinfo doesn't give us the TTL we use other APIs when we can that may
// not do it for us (Window's DnsQuery function for example).
#define DO_MERGE_FOR_AF_UNSPEC 1
#else
#define DO_MERGE_FOR_AF_UNSPEC 0
#endif

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
    /* generation count of |addr_info|. Must be incremented whenever addr_info
     * is changed so any iterators going through the old linked list can be
     * invalidated. */
    int          addr_info_gencnt;
    mozilla::net::AddrInfo *addr_info;
    mozilla::net::NetAddr  *addr;
    bool         negative;   /* True if this record is a cache of a failed lookup.
                                Negative cache entries are valid just like any other
                                (though never for more than 60 seconds), but a use
                                of that negative entry forces an asynchronous refresh. */

    mozilla::TimeStamp expiration;

    bool HasUsableResult(uint16_t queryFlags) const;

    // hold addr_info_lock when calling the blacklist functions
    bool   Blacklisted(mozilla::net::NetAddr *query);
    void   ResetBlacklist();
    void   ReportUnusable(mozilla::net::NetAddr *addr);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
    friend class nsHostResolver;

    PRCList callbacks; /* list of callbacks */

    bool    resolving; /* true if this record is being resolved, which means
                        * that it is either on the pending queue or owned by
                        * one of the worker threads. */

    bool    onQueue;  /* true if pending and on the queue (not yet given to getaddrinfo())*/
    bool    usingAnyThread; /* true if off queue and contributing to mActiveAnyThreadCount */
    bool    mDoomed; /* explicitly expired */

#if DO_MERGE_FOR_AF_UNSPEC
    // If this->af is PR_AF_UNSPEC, this will contain the address family that
    // should actually be passed to GetAddrInfo.
    uint16_t mInnerAF;
    static const uint16_t UNSPECAF_NULL = -1;

    // mCloneOf will point at the original host record if this record is a
    // clone. Null otherwise.
    nsHostRecord* mCloneOf;

    // This will be set to the number of unresolved host records out for the
    // given host record key. 0 for non AF_UNSPEC records.
    int mNumPending;

    nsresult CloneForAFUnspec(nsHostRecord** aNewRecord, uint16_t aUnspecAF);
#endif

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
    static nsresult Create(uint32_t         maxCacheEntries,  // zero disables cache
                           uint32_t         maxCacheLifetime, // seconds
                           uint32_t         lifetimeGracePeriod, // seconds
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

private:
   explicit nsHostResolver(uint32_t maxCacheEntries = 50, uint32_t maxCacheLifetime = 60,
                            uint32_t lifetimeGracePeriod = 0);
   ~nsHostResolver();

    nsresult Init();
    bool     GetHostToLookup(nsHostRecord **m);
    void     OnLookupComplete(nsHostRecord *, nsresult, mozilla::net::AddrInfo *);
    void     DeQueue(PRCList &aQ, nsHostRecord **aResult);
    void     ClearPendingQueue(PRCList *aPendingQueue);
    nsresult ConditionallyCreateThread(nsHostRecord *rec);

    // This will issue two lookups (using the internal version) if
    // DO_MERGE_FOR_AF_UNSPEC is enabled and the passed in record is an
    // AF_UNSPEC record.
    nsresult IssueLookup(nsHostRecord *);

    // This actually issues a single lookup
    nsresult IssueLookupInternal(nsHostRecord *);

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
    mozilla::TimeDuration mMaxCacheLifetime; // granularity seconds
    mozilla::TimeDuration mGracePeriod; // granularity seconds
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

public:
    /*
     * Called by the networking dashboard via the DnsService2
     */
    void GetDNSCacheEntries(nsTArray<mozilla::net::DNSCacheEntries> *);
};

#endif // nsHostResolver_h__
