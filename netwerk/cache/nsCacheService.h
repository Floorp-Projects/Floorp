/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsCacheService_h_
#define _nsCacheService_h_

#include "nsICacheService.h"
#include "nsCacheSession.h"
#include "nsCacheDevice.h"
#include "nsCacheEntry.h"
#include "nsThreadUtils.h"
#include "nsICacheListener.h"
#include "nsIMemoryReporter.h"

#include "prthread.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "mozilla/Telemetry.h"

class nsCacheRequest;
class nsCacheProfilePrefObserver;
class nsDiskCacheDevice;
class nsMemoryCacheDevice;
class nsOfflineCacheDevice;
class nsCacheServiceAutoLock;
class nsITimer;
class mozIStorageService;


/******************************************************************************
 * nsNotifyDoomListener
 *****************************************************************************/

class nsNotifyDoomListener : public nsRunnable {
public:
    nsNotifyDoomListener(nsICacheListener *listener,
                         nsresult status)
        : mListener(listener)      // transfers reference
        , mStatus(status)
    {}

    NS_IMETHOD Run()
    {
        mListener->OnCacheEntryDoomed(mStatus);
        NS_RELEASE(mListener);
        return NS_OK;
    }

private:
    nsICacheListener *mListener;
    nsresult          mStatus;
};

/******************************************************************************
 *  nsCacheService
 ******************************************************************************/

class nsCacheService : public nsICacheServiceInternal,
                       public nsIMemoryReporter
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSICACHESERVICE
    NS_DECL_NSICACHESERVICEINTERNAL
    NS_DECL_NSIMEMORYREPORTER

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
                                             uint32_t           offset,
                                             nsIInputStream **  result);

    static nsresult  OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                              nsCacheAccessMode  mode,
                                              uint32_t           offset,
                                              nsIOutputStream ** result);

    static nsresult  OnDataSizeChange(nsCacheEntry * entry, int32_t deltaSize);

    static nsresult  SetCacheElement(nsCacheEntry * entry, nsISupports * element);

    static nsresult  ValidateEntry(nsCacheEntry * entry);

    static int32_t   CacheCompressionLevel();

    static bool      GetClearingEntries();

    static void      GetDiskCacheDirectory(nsIFile ** result);

    /**
     * Methods called by any cache classes
     */

    static
    nsCacheService * GlobalInstance()   { return gService; }

    static nsresult  DoomEntry(nsCacheEntry * entry);

    static bool      IsStorageEnabledForPolicy_Locked(nsCacheStoragePolicy policy);

    /**
     * Called by disk cache to notify us to use the new max smart size
     */
    static void      MarkStartingFresh();

    /**
     * Methods called by nsApplicationCacheService
     */

    nsresult GetOfflineDevice(nsOfflineCacheDevice ** aDevice);

    /**
     * Creates an offline cache device that works over a specific profile directory.
     * A tool to preload offline cache for profiles different from the current
     * application's profile directory.
     */
    nsresult GetCustomOfflineDevice(nsIFile *aProfileDir,
                                    int32_t aQuota,
                                    nsOfflineCacheDevice **aDevice);

    // This method may be called to release an object while the cache service
    // lock is being held.  If a non-null target is specified and the target
    // does not correspond to the current thread, then the release will be
    // proxied to the specified target.  Otherwise, the object will be added to
    // the list of objects to be released when the cache service is unlocked.
    static void      ReleaseObject_Locked(nsISupports *    object,
                                          nsIEventTarget * target = nullptr);

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
    static void      SetDiskCacheCapacity(int32_t  capacity);
    // Set max size for a disk-cache entry (in KB). -1 disables limit up to
    // 1/8th of disk cache size
    static void      SetDiskCacheMaxEntrySize(int32_t  maxSize);
    // Set max size for a memory-cache entry (in kilobytes). -1 disables
    // limit up to 90% of memory cache size
    static void      SetMemoryCacheMaxEntrySize(int32_t  maxSize);

    static void      SetOfflineCacheEnabled(bool    enabled);
    // Sets the offline cache capacity (in kilobytes)
    static void      SetOfflineCacheCapacity(int32_t  capacity);

    static void      SetMemoryCache();

    static void      SetCacheCompressionLevel(int32_t level);

    // Starts smart cache size computation if disk device is available
    static nsresult  SetDiskSmartSize();

    static void      MoveOrRemoveDiskCache(nsIFile *aOldCacheDir,
                                           nsIFile *aNewCacheDir,
                                           const char *aCacheSubdir);

    nsresult         Init();
    void             Shutdown();

    static bool      IsInitialized()
    {
      if (!gService) {
          return false;
      }
      return gService->mInitialized;
    }

    static void      AssertOwnsLock()
    { gService->mLock.AssertCurrentThreadOwns(); }

    static void      LeavePrivateBrowsing();
    bool             IsDoomListEmpty();

    typedef bool (*DoomCheckFn)(nsCacheEntry* entry);

private:
    friend class nsCacheServiceAutoLock;
    friend class nsOfflineCacheDevice;
    friend class nsProcessRequestEvent;
    friend class nsSetSmartSizeEvent;
    friend class nsBlockOnCacheThreadEvent;
    friend class nsSetDiskSmartSizeCallback;
    friend class nsDoomEvent;
    friend class nsDisableOldMaxSmartSizePrefEvent;
    friend class nsDiskCacheMap;
    friend class nsAsyncDoomEvent;
    friend class nsCacheEntryDescriptor;

    /**
     * Internal Methods
     */

    static void      Lock(::mozilla::Telemetry::ID mainThreadLockerID);
    static void      Unlock();
    void             LockAcquired();
    void             LockReleased();

    nsresult         CreateDiskDevice();
    nsresult         CreateOfflineDevice();
    nsresult         CreateCustomOfflineDevice(nsIFile *aProfileDir,
                                               int32_t aQuota,
                                               nsOfflineCacheDevice **aDevice);
    nsresult         CreateMemoryDevice();

    nsresult         RemoveCustomOfflineDevice(nsOfflineCacheDevice *aDevice);

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

    void             ClearDoomList(void);
    void             DoomActiveEntries(DoomCheckFn check);
    void             CloseAllStreams();
    void             FireClearNetworkCacheStoredAnywhereNotification();

    static
    PLDHashOperator  GetActiveEntries(PLDHashTable *    table,
                                      PLDHashEntryHdr * hdr,
                                      uint32_t          number,
                                      void *            arg);
    static
    PLDHashOperator  RemoveActiveEntry(PLDHashTable *    table,
                                       PLDHashEntryHdr * hdr,
                                       uint32_t          number,
                                       void *            arg);

    static
    PLDHashOperator  ShutdownCustomCacheDeviceEnum(const nsAString& aProfileDir,
                                                   nsRefPtr<nsOfflineCacheDevice>& aDevice,
                                                   void* aUserArg);
#if defined(PR_LOGGING)
    void LogCacheStatistics();
#endif

    nsresult         SetDiskSmartSize_Locked();

    /**
     *  Data Members
     */

    static nsCacheService *         gService;  // there can be only one...

    nsCOMPtr<mozIStorageService>    mStorageService;

    nsCacheProfilePrefObserver *    mObserver;

    mozilla::Mutex                  mLock;
    mozilla::CondVar                mCondVar;

    mozilla::Mutex                  mTimeStampLock;
    mozilla::TimeStamp              mLockAcquiredTimeStamp;

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

    nsRefPtrHashtable<nsStringHashKey, nsOfflineCacheDevice> mCustomOfflineDevices;

    nsCacheEntryHashTable           mActiveEntries;
    PRCList                         mDoomedEntries;

    // stats

    uint32_t                        mTotalEntries;
    uint32_t                        mCacheHits;
    uint32_t                        mCacheMisses;
    uint32_t                        mMaxKeyLength;
    uint32_t                        mMaxDataSize;
    uint32_t                        mMaxMetaSize;

    // Unexpected error totals
    uint32_t                        mDeactivateFailures;
    uint32_t                        mDeactivatedUnboundEntries;
};

/******************************************************************************
 *  nsCacheServiceAutoLock
 ******************************************************************************/

#define LOCK_TELEM(x) \
  (::mozilla::Telemetry::CACHE_SERVICE_LOCK_WAIT_MAINTHREAD_##x)

// Instantiate this class to acquire the cache service lock for a particular
// execution scope.
class nsCacheServiceAutoLock {
public:
    nsCacheServiceAutoLock(mozilla::Telemetry::ID mainThreadLockerID) {
        nsCacheService::Lock(mainThreadLockerID);
    }
    ~nsCacheServiceAutoLock() {
        nsCacheService::Unlock();
    }
};

#endif // _nsCacheService_h_
