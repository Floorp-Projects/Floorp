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
#include "pratom.h"
#include "prcvar.h"
#include "prclist.h"
#include "prnetdb.h"
#include "pldhash.h"

class nsHostResolver;
class nsHostRecord;
class nsResolveHostCB;
class nsAddrInfo;

/* XXX move this someplace more generic */
#define NET_DECL_REFCOUNTED_THREADSAFE                 \
    PRInt32 AddRef() {                                 \
        return PR_AtomicIncrement(&mRefCount);         \
    }                                                  \
    PRInt32 Release() {                                \
        PRInt32 n = PR_AtomicDecrement(&mRefCount);    \
        if (n == 0)                                    \
            delete this;                               \
        return n;                                      \
    }

/**
 * reference counted wrapper around PRAddrInfo.  these are stored in
 * the DNS cache.
 */
class nsAddrInfo
{
public:
    NET_DECL_REFCOUNTED_THREADSAFE

    nsAddrInfo(PRAddrInfo *data)
        : mRefCount(0)
        , mData(data) {}

    const PRAddrInfo *get() const { return mData; }

private:
    nsAddrInfo(); // never called
   ~nsAddrInfo() { if (mData) PR_FreeAddrInfo(mData); }

    PRInt32     mRefCount;
    PRAddrInfo *mData;    
};

/**
 * ResolveHost callback object.  It's PRCList members are used by
 * the nsHostResolver and should not be used by anything else.
 */
class nsResolveHostCB : public PRCList
{
public:
    /**
     * LookupComplete
     * 
     * Runs on an unspecified background thread.
     *
     * @param resolver
     *        nsHostResolver object associated with this result
     * @param host
     *        hostname that was resolved
     * @param status
     *        if successful, |result| will be non-null
     * @param result
     *        resulting nsAddrInfo object
     */
    virtual void OnLookupComplete(nsHostResolver *resolver,
                                  const char     *host,
                                  nsresult        status,
                                  nsAddrInfo     *result) = 0;

protected:
    virtual ~nsResolveHostCB() {}
};

/**
 * nsHostResolver: an asynchronous hostname resolver.
 */
class nsHostResolver
{
public:
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
     * host resolver instances are reference counted.
     */
    NET_DECL_REFCOUNTED_THREADSAFE

    /**
     * resolve the given hostname asynchronously.  the caller can synthesize
     * a synchronous host lookup using a lock and a cvar.  as noted above
     * the callback will occur re-entrantly from an unspecified thread.  the
     * host lookup cannot be canceled (cancelation can be layered above this
     * by having the callback implementation return without doing anything).
     */
    nsresult ResolveHost(const char      *hostname,
                         PRBool           bypassCache,
                         nsResolveHostCB *callback);

    /**
     * removes the specified callback from the nsHostRecord for the given
     * hostname.  this function executes the callback if the callback is
     * still pending with the status failure code NS_ERROR_ABORT.
     */
    void DetachCallback(const char      *hostname,
                        nsResolveHostCB *callback);

private:
    nsHostResolver(PRUint32 maxCacheEntries=50, PRUint32 maxCacheLifetime=1);
   ~nsHostResolver();

    nsresult Init();
    nsresult IssueLookup(nsHostRecord *);
    PRBool   GetHostToLookup(nsHostRecord **);
    void     OnLookupComplete(nsHostRecord *, nsresult, PRAddrInfo *);

    static void PR_CALLBACK ThreadFunc(void *);

    PRInt32       mRefCount;
    PRUint32      mMaxCacheEntries;
    PRUint32      mMaxCacheLifetime;
    PRLock       *mLock;
    PRCondVar    *mIdleThreadCV; // non-null if idle thread
    PRBool        mHaveIdleThread;
    PRUint32      mThreadCount;
    PLDHashTable  mDB;
    PRCList       mPendingQ;
    PRCList       mEvictionQ;
    PRUint32      mEvictionQSize;
    PRTime        mCreationTime;
    PRBool        mShutdown;
};

#endif // nsHostResolver_h__
