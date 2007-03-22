/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is nsCacheService.h, released
 * February 10, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan  <gordon@netscape.com>
 *   Patrick C. Beard <beard@netscape.com>
 *   Darin Fisher     <darin@netscape.com>
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


#ifndef _nsCacheService_h_
#define _nsCacheService_h_

#include "nsICacheService.h"
#include "nsCacheSession.h"
#include "nsCacheDevice.h"
#include "nsCacheEntry.h"

#include "prlock.h"
#include "prthread.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsProxiedService.h"
#include "nsTArray.h"

class nsCacheRequest;
class nsCacheProfilePrefObserver;
class nsDiskCacheDevice;
class nsMemoryCacheDevice;
class nsCacheServiceAutoLock;


/******************************************************************************
 *  nsCacheService
 ******************************************************************************/

class nsCacheService : public nsICacheService
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHESERVICE
    
    nsCacheService();
    virtual ~nsCacheService();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* outer, const nsIID& iid, void* *result);


    /**
     * Methods called by nsCacheSession
     */
    static nsresult  OpenCacheEntry(nsCacheSession *           session,
                                    const nsACString &         key,
                                    nsCacheAccessMode          accessRequested,
                                    PRBool                     blockingMode,
                                    nsICacheListener *         listener,
                                    nsICacheEntryDescriptor ** result);

    static nsresult  EvictEntriesForSession(nsCacheSession *   session);

    static nsresult  IsStorageEnabledForPolicy(nsCacheStoragePolicy  storagePolicy,
                                               PRBool *              result);

    /**
     * Methods called by nsCacheEntryDescriptor
     */

    static void      CloseDescriptor(nsCacheEntryDescriptor * descriptor);

    static nsresult  GetFileForEntry(nsCacheEntry *         entry,
                                     nsIFile **             result);

    static nsresult  OpenInputStreamForEntry(nsCacheEntry *     entry,
                                             nsCacheAccessMode  mode,
                                             PRUint32           offset,
                                             nsIInputStream **  result);

    static nsresult  OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                              nsCacheAccessMode  mode,
                                              PRUint32           offset,
                                              nsIOutputStream ** result);

    static nsresult  OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize);

    static nsresult  SetCacheElement(nsCacheEntry * entry, nsISupports * element);

    static nsresult  ValidateEntry(nsCacheEntry * entry);


    /**
     * Methods called by any cache classes
     */

    static
    nsCacheService * GlobalInstance()   { return gService; }
    
    static nsresult  DoomEntry(nsCacheEntry * entry);

    static PRBool    IsStorageEnabledForPolicy_Locked(nsCacheStoragePolicy policy);

    // This method may be called to release an object while the cache service
    // lock is being held.  If a non-null target is specified and the target
    // does not correspond to the current thread, then the release will be
    // proxied to the specified target.  Otherwise, the object will be added to
    // the list of objects to be released when the cache service is unlocked.
    static void      ReleaseObject_Locked(nsISupports *    object,
                                          nsIEventTarget * target = nsnull);

    /**
     * Methods called by nsCacheProfilePrefObserver
     */
    static void      OnProfileShutdown(PRBool cleanse);
    static void      OnProfileChanged();

    static void      SetDiskCacheEnabled(PRBool  enabled);
    static void      SetDiskCacheCapacity(PRInt32  capacity);

    static void      SetOfflineCacheEnabled(PRBool  enabled);
    static void      SetOfflineCacheCapacity(PRInt32  capacity);

    static void      SetMemoryCache();

    nsresult         Init();
    void             Shutdown();
private:
    friend class nsCacheServiceAutoLock;

    /**
     * Internal Methods
     */

    static void      Lock();
    static void      Unlock();

    nsresult         CreateDiskDevice();
    nsresult         CreateOfflineDevice();
    nsresult         CreateMemoryDevice();

    nsresult         CreateRequest(nsCacheSession *   session,
                                   const nsACString & clientKey,
                                   nsCacheAccessMode  accessRequested,
                                   PRBool             blockingMode,
                                   nsICacheListener * listener,
                                   nsCacheRequest **  request);

    nsresult         DoomEntry_Internal(nsCacheEntry * entry);

    nsresult         EvictEntriesForClient(const char *          clientID,
                                           nsCacheStoragePolicy  storagePolicy);

    // Notifies request listener asynchronously on the request's thread, and
    // releases the descriptor on the request's thread.  If this method fails,
    // the descriptor is not released.
    nsresult         NotifyListener(nsCacheRequest *          request,
                                    nsICacheEntryDescriptor * descriptor,
                                    nsCacheAccessMode         accessGranted,
                                    nsresult                  error);

    nsresult         ActivateEntry(nsCacheRequest * request, nsCacheEntry ** entry);

    nsCacheDevice *  EnsureEntryHasDevice(nsCacheEntry * entry);

    nsCacheEntry *   SearchCacheDevices(nsCString * key, nsCacheStoragePolicy policy, PRBool *collision);

    void             DeactivateEntry(nsCacheEntry * entry);

    nsresult         ProcessRequest(nsCacheRequest *           request,
                                    PRBool                     calledFromOpenCacheEntry,
                                    nsICacheEntryDescriptor ** result);

    nsresult         ProcessPendingRequests(nsCacheEntry * entry);

    void             ClearPendingRequests(nsCacheEntry * entry);
    void             ClearDoomList(void);
    void             ClearActiveEntries(void);
    void             DoomActiveEntries(void);

    static
    PLDHashOperator PR_CALLBACK  DeactivateAndClearEntry(PLDHashTable *    table,
                                                         PLDHashEntryHdr * hdr,
                                                         PRUint32          number,
                                                         void *            arg);
    static
    PLDHashOperator PR_CALLBACK  RemoveActiveEntry(PLDHashTable *    table,
                                                   PLDHashEntryHdr * hdr,
                                                   PRUint32          number,
                                                   void *            arg);
#if defined(PR_LOGGING)
    void LogCacheStatistics();
#endif

    /**
     *  Data Members
     */

    static nsCacheService *         gService;  // there can be only one...
    
    nsCacheProfilePrefObserver *    mObserver;
    
    PRLock *                        mLock;

#if defined(DEBUG)
    PRThread *                      mLockedThread;  // The thread holding mLock
#endif

    nsTArray<nsISupports*>          mDoomedObjects;
    
    PRBool                          mInitialized;
    
    PRBool                          mEnableMemoryDevice;
    PRBool                          mEnableDiskDevice;
    PRBool                          mEnableOfflineDevice;

    nsMemoryCacheDevice *           mMemoryDevice;
    nsDiskCacheDevice *             mDiskDevice;
    nsDiskCacheDevice *             mOfflineDevice;

    nsCacheEntryHashTable           mActiveEntries;
    PRCList                         mDoomedEntries;

    // stats
    
    PRUint32                        mTotalEntries;
    PRUint32                        mCacheHits;
    PRUint32                        mCacheMisses;
    PRUint32                        mMaxKeyLength;
    PRUint32                        mMaxDataSize;
    PRUint32                        mMaxMetaSize;

    // Unexpected error totals
    PRUint32                        mDeactivateFailures;
    PRUint32                        mDeactivatedUnboundEntries;
};

/******************************************************************************
 *  nsCacheServiceAutoLock
 ******************************************************************************/

// Instantiate this class to acquire the cache service lock for a particular
// execution scope.
class nsCacheServiceAutoLock {
public:
    nsCacheServiceAutoLock() {
        nsCacheService::Lock();
    }
    ~nsCacheServiceAutoLock() {
        nsCacheService::Unlock();
    }
};

#endif // _nsCacheService_h_
