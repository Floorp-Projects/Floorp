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
#include "pratom.h"
#include "prcvar.h"
#include "prclist.h"
#include "prnetdb.h"
#include "pldhash.h"
#include "nsISupportsImpl.h"

class nsHostResolver;
class nsHostRecord;
class nsResolveHostCallback;

/* XXX move this someplace more generic */
#define NS_DECL_REFCOUNTED_THREADSAFE                     \
  private:                                                \
    nsAutoRefCnt _refc;                                   \
  public:                                                 \
    PRInt32 AddRef() {                                    \
        return PR_AtomicIncrement((PRInt32*)&_refc);      \
    }                                                     \
    PRInt32 Release() {                                   \
        PRInt32 n = PR_AtomicDecrement((PRInt32*)&_refc); \
        if (n == 0)                                       \
            delete this;                                  \
        return n;                                         \
    }

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
public:
    NS_DECL_REFCOUNTED_THREADSAFE

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

    PRAddrInfo  *addr_info;
    PRNetAddr   *addr;
    PRUint32     expiration; /* measured in minutes since epoch */

    PRBool HasResult() const { return (addr_info || addr) != nsnull; }

private:
    friend class nsHostResolver;

    PRCList callbacks; /* list of callbacks */

    PRBool  resolving; /* true if this record is being resolved, which means
                        * that it is either on the pending queue or owned by
                        * one of the worker threads. */ 

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
public:
    /**
     * host resolver instances are reference counted.
     */
    NS_DECL_REFCOUNTED_THREADSAFE

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
     * callback if the callback is still pending with the status failure code
     * NS_ERROR_ABORT.
     */
    void DetachCallback(const char            *hostname,
                        PRUint16               flags,
                        PRUint16               af,
                        nsResolveHostCallback *callback);

    /**
     * values for the flags parameter passed to ResolveHost and DetachCallback
     * that may be bitwise OR'd together.
     *
     * NOTE: in this implementation, these flags correspond exactly in value
     *       to the flags defined on nsIDNSService.
     */
    enum {
        RES_BYPASS_CACHE = 1 << 0,
        RES_CANON_NAME   = 1 << 1
    };

private:
    nsHostResolver(PRUint32 maxCacheEntries=50, PRUint32 maxCacheLifetime=1);
   ~nsHostResolver();

    nsresult Init();
    nsresult IssueLookup(nsHostRecord *);
    PRBool   GetHostToLookup(nsHostRecord **);
    void     OnLookupComplete(nsHostRecord *, nsresult, PRAddrInfo *);

    PR_STATIC_CALLBACK(void) ThreadFunc(void *);

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
