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

#ifndef nsHostResolver_h__
#define nsHostResolver_h__

#include "nscore.h"
#include "nsAtomicRefcnt.h"
#include "prclist.h"
#include "prnetdb.h"
#include "pldhash.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"

class nsHostResolver;
class nsHostRecord;
class nsResolveHostCallback;

/* XXX move this someplace more generic */
#define NS_DECL_REFCOUNTED_THREADSAFE(classname)                             \
  private:                                                                   \
    nsAutoRefCnt _refc;                                                      \
  public:                                                                    \
    PRInt32 AddRef() {                                                       \
        PRInt32 n = NS_AtomicIncrementRefcnt(_refc);                         \
        NS_LOG_ADDREF(this, n, #classname, sizeof(classname));               \
        return n;                                                            \
    }                                                                        \
    PRInt32 Release() {                                                      \
        PRInt32 n = NS_AtomicDecrementRefcnt(_refc);                         \
        NS_LOG_RELEASE(this, n, #classname);                                 \
        if (n == 0)                                                          \
            delete this;                                                     \
        return n;                                                            \
    }

#ifdef ANDROID
// See bug 687367 - pre gingerbread android has race conditions involving stdio.
// stdio is used as part of the getaddrinfo() implementation. In order to reduce
// that race window limit ourselves to 1 lookup at a time on android.

#define MAX_RESOLVER_THREADS_FOR_ANY_PRIORITY  0
#define MAX_RESOLVER_THREADS_FOR_HIGH_PRIORITY 1
#define MAX_NON_PRIORITY_REQUESTS 0
#else
#define MAX_RESOLVER_THREADS_FOR_ANY_PRIORITY  3
#define MAX_RESOLVER_THREADS_FOR_HIGH_PRIORITY 5
#define MAX_NON_PRIORITY_REQUESTS 150
#endif

#define MAX_RESOLVER_THREADS (MAX_RESOLVER_THREADS_FOR_ANY_PRIORITY + \
                              MAX_RESOLVER_THREADS_FOR_HIGH_PRIORITY)

struct nsHostKey
{
    const char *host;
    PRUint16    flags;
    PRUint16    af;
};

/**
 * nsHostRecord - ref counted object type stored in host resolver cache.
 */
class nsHostRecord : public PRCList, public nsHostKey
{
    typedef mozilla::Mutex Mutex;

public:
    NS_DECL_REFCOUNTED_THREADSAFE(nsHostRecord)

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
    PRAddrInfo  *addr_info;
    PRNetAddr   *addr;
    bool         negative;   /* True if this record is a cache of a failed lookup.
                                Negative cache entries are valid just like any other
                                (though never for more than 60 seconds), but a use
                                of that negative entry forces an asynchronous refresh. */

    PRUint32     expiration; /* measured in minutes since epoch */

    bool HasResult() const { return addr_info || addr || negative; }

    // hold addr_info_lock when calling the blacklist functions
    bool Blacklisted(PRNetAddr *query);
    void   ResetBlacklist();
    void   ReportUnusable(PRNetAddr *addr);

private:
    friend class nsHostResolver;

    PRCList callbacks; /* list of callbacks */

    bool    resolving; /* true if this record is being resolved, which means
                        * that it is either on the pending queue or owned by
                        * one of the worker threads. */ 
    
    bool    onQueue;  /* true if pending and on the queue (not yet given to getaddrinfo())*/
    bool    usingAnyThread; /* true if off queue and contributing to mActiveAnyThreadCount */

    // a list of addresses associated with this record that have been reported
    // as unusable. the list is kept as a set of strings to make it independent
    // of gencnt.
    nsTArray<nsCString> mBlacklistedItems;

    nsHostRecord(const nsHostKey *key);           /* use Create() instead */
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
    NS_DECL_REFCOUNTED_THREADSAFE(nsHostResolver)

    /**
     * creates an addref'd instance of a nsHostResolver object.
     */
    static nsresult Create(PRUint32         maxCacheEntries,  // zero disables cache
                           PRUint32         maxCacheLifetime, // minutes
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
                         PRUint16               flags,
                         PRUint16               af,
                         nsResolveHostCallback *callback);

    /**
     * removes the specified callback from the nsHostRecord for the given
     * hostname, flags, and address family.  these parameters should correspond
     * to the parameters passed to ResolveHost.  this function executes the
     * callback if the callback is still pending with the given status.
     */
    void DetachCallback(const char            *hostname,
                        PRUint16               flags,
                        PRUint16               af,
                        nsResolveHostCallback *callback,
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
        RES_SPECULATE     = 1 << 4   
    };

private:
    nsHostResolver(PRUint32 maxCacheEntries=50, PRUint32 maxCacheLifetime=1);
   ~nsHostResolver();

    nsresult Init();
    nsresult IssueLookup(nsHostRecord *);
    bool     GetHostToLookup(nsHostRecord **m);
    void     OnLookupComplete(nsHostRecord *, nsresult, PRAddrInfo *);
    void     DeQueue(PRCList &aQ, nsHostRecord **aResult);
    void     ClearPendingQueue(PRCList *aPendingQueue);
    nsresult ConditionallyCreateThread(nsHostRecord *rec);
    
    static void  MoveQueue(nsHostRecord *aRec, PRCList &aDestQ);
    
    static void ThreadFunc(void *);

    PRUint32      mMaxCacheEntries;
    PRUint32      mMaxCacheLifetime;
    Mutex         mLock;
    CondVar       mIdleThreadCV;
    PRUint32      mNumIdleThreads;
    PRUint32      mThreadCount;
    PRUint32      mActiveAnyThreadCount;
    PLDHashTable  mDB;
    PRCList       mHighQ;
    PRCList       mMediumQ;
    PRCList       mLowQ;
    PRCList       mEvictionQ;
    PRUint32      mEvictionQSize;
    PRUint32      mPendingCount;
    PRTime        mCreationTime;
    bool          mShutdown;
    PRIntervalTime mLongIdleTimeout;
    PRIntervalTime mShortIdleTimeout;
};

#endif // nsHostResolver_h__
