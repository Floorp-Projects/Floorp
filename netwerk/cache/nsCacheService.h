/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef _nsCacheService_h_
#define _nsCacheService_h_

#include "nsICacheService.h"
#include "nsCacheSession.h"
#include "nsCacheDevice.h"
#include "nsCacheEntry.h"

#include "prthread.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"

class nsCacheRequest;
class nsCacheProfilePrefObserver;
class nsDiskCacheDevice;
class nsMemoryCacheDevice;
class nsOfflineCacheDevice;
class nsCacheServiceAutoLock;
class nsITimer;


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
    static nsresult
    Create(nsISupports* outer, const nsIID& iid, void* *result);


    /**
     * Methods called by nsCacheSession
     */
    static nsresult  OpenCacheEntry(nsCacheSession *           session,
                                    const nsACString &         key,
                                    nsCacheAccessMode          accessRequested,
                                    bool                       blockingMode,
                                    nsICacheListener *         listener,
                                    nsICacheEntryDescriptor ** result);

    static nsresult  EvictEntriesForSession(nsCacheSession *   session);

    static nsresult  IsStorageEnabledForPolicy(nsCacheStoragePolicy  storagePolicy,
                                               bool *              result);

    static nsresult  DoomEntry(nsCacheSession   *session,
                               const nsACString &key,
                               nsICacheListener *listener);

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

    static PRInt32   CacheCompressionLevel();

    /**
     * Methods called by any cache classes
     */

    static
    nsCacheService * GlobalInstance()   { return gService; }

    static PRInt64   MemoryDeviceSize();
    
    static nsresult  DoomEntry(nsCacheEntry * entry);

    static bool      IsStorageEnabledForPolicy_Locked(nsCacheStoragePolicy policy);

    /**
     * Methods called by nsApplicationCacheService
     */

    nsresult GetOfflineDevice(nsOfflineCacheDevice ** aDevice);

    // This method may be called to release an object while the cache service
    // lock is being held.  If a non-null target is specified and the target
    // does not correspond to the current thread, then the release will be
    // proxied to the specified target.  Otherwise, the object will be added to
    // the list of objects to be released when the cache service is unlocked.
    static void      ReleaseObject_Locked(nsISupports *    object,
                                          nsIEventTarget * target = nsnull);

    static nsresult DispatchToCacheIOThread(nsIRunnable* event);

    // Calling this method will block the calling thread until all pending
    // events on the cache-io thread has finished. The calling thread must
    // hold the cache-lock
    static nsresult SyncWithCacheIOThread();


    /**
     * Methods called by nsCacheProfilePrefObserver
     */
    static void      OnProfileShutdown(bool cleanse);
    static void      OnProfileChanged();

    static void      SetDiskCacheEnabled(bool    enabled);
    // Sets the disk cache capacity (in kilobytes)
    static void      SetDiskCacheCapacity(PRInt32  capacity);
    // Set max size for a disk-cache entry (in KB). -1 disables limit up to
    // 1/8th of disk cache size
    static void      SetDiskCacheMaxEntrySize(PRInt32  maxSize);
    // Set max size for a memory-cache entry (in kilobytes). -1 disables
    // limit up to 90% of memory cache size
    static void      SetMemoryCacheMaxEntrySize(PRInt32  maxSize);

    static void      SetOfflineCacheEnabled(bool    enabled);
    // Sets the offline cache capacity (in kilobytes)
    static void      SetOfflineCacheCapacity(PRInt32  capacity);

    static void      SetMemoryCache();

    static void      SetCacheCompressionLevel(PRInt32 level);

    static void      OnEnterExitPrivateBrowsing();

    // Starts smart cache size computation if disk device is available
    static nsresult  SetDiskSmartSize();

    nsresult         Init();
    void             Shutdown();

    static void      AssertOwnsLock()
    { gService->mLock.AssertCurrentThreadOwns(); }

private:
    friend class nsCacheServiceAutoLock;
    friend class nsOfflineCacheDevice;
    friend class nsProcessRequestEvent;
    friend class nsSetSmartSizeEvent;
    friend class nsBlockOnCacheThreadEvent;
    friend class nsSetDiskSmartSizeCallback;
    friend class nsDoomEvent;

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
                                   bool               blockingMode,
                                   nsICacheListener * listener,
                                   nsCacheRequest **  request);

    nsresult         DoomEntry_Internal(nsCacheEntry * entry,
                                        bool doProcessPendingRequests);

    nsresult         EvictEntriesForClient(const char *          clientID,
                                           nsCacheStoragePolicy  storagePolicy);

    // Notifies request listener asynchronously on the request's thread, and
    // releases the descriptor on the request's thread.  If this method fails,
    // the descriptor is not released.
    nsresult         NotifyListener(nsCacheRequest *          request,
                                    nsICacheEntryDescriptor * descriptor,
                                    nsCacheAccessMode         accessGranted,
                                    nsresult                  error);

    nsresult         ActivateEntry(nsCacheRequest * request,
                                   nsCacheEntry ** entry,
                                   nsCacheEntry ** doomedEntry);

    nsCacheDevice *  EnsureEntryHasDevice(nsCacheEntry * entry);

    nsCacheEntry *   SearchCacheDevices(nsCString * key, nsCacheStoragePolicy policy, bool *collision);

    void             DeactivateEntry(nsCacheEntry * entry);

    nsresult         ProcessRequest(nsCacheRequest *           request,
                                    bool                       calledFromOpenCacheEntry,
                                    nsICacheEntryDescriptor ** result);

    nsresult         ProcessPendingRequests(nsCacheEntry * entry);

    void             ClearPendingRequests(nsCacheEntry * entry);
    void             ClearDoomList(void);
    void             ClearActiveEntries(void);
    void             DoomActiveEntries(void);

    static
    PLDHashOperator  DeactivateAndClearEntry(PLDHashTable *    table,
                                             PLDHashEntryHdr * hdr,
                                             PRUint32          number,
                                             void *            arg);
    static
    PLDHashOperator  RemoveActiveEntry(PLDHashTable *    table,
                                       PLDHashEntryHdr * hdr,
                                       PRUint32          number,
                                       void *            arg);
#if defined(PR_LOGGING)
    void LogCacheStatistics();
#endif

    nsresult         SetDiskSmartSize_Locked();

    /**
     *  Data Members
     */

    static nsCacheService *         gService;  // there can be only one...
    
    nsCacheProfilePrefObserver *    mObserver;
    
    mozilla::Mutex                  mLock;
    mozilla::CondVar                mCondVar;

    nsCOMPtr<nsIThread>             mCacheIOThread;

    nsTArray<nsISupports*>          mDoomedObjects;
    nsCOMPtr<nsITimer>              mSmartSizeTimer;
    
    bool                            mInitialized;
    bool                            mClearingEntries;
    
    bool                            mEnableMemoryDevice;
    bool                            mEnableDiskDevice;
    bool                            mEnableOfflineDevice;

    nsMemoryCacheDevice *           mMemoryDevice;
    nsDiskCacheDevice *             mDiskDevice;
    nsOfflineCacheDevice *          mOfflineDevice;

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
