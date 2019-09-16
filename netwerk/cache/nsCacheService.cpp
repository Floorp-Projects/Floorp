/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCacheService.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/FileUtils.h"

#include "necko-config.h"

#include "nsCache.h"
#include "nsCacheRequest.h"
#include "nsCacheEntry.h"
#include "nsCacheEntryDescriptor.h"
#include "nsCacheDevice.h"
#include "nsICacheVisitor.h"
#include "nsDiskCacheDeviceSQL.h"
#include "nsCacheUtils.h"
#include "../cache2/CacheObserver.h"
#include "nsINamed.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIFile.h"
#include "nsIOService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "nsDeleteDir.h"
#include "nsNetCID.h"
#include <math.h>  // for log()
#include "mozilla/Services.h"
#include "nsITimer.h"
#include "mozIStorageService.h"

#include "mozilla/net/NeckoCommon.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::net;

/******************************************************************************
 * nsCacheProfilePrefObserver
 *****************************************************************************/
#define OFFLINE_CACHE_ENABLE_PREF "browser.cache.offline.enable"
#define OFFLINE_CACHE_DIR_PREF "browser.cache.offline.parent_directory"
#define OFFLINE_CACHE_CAPACITY_PREF "browser.cache.offline.capacity"
#define OFFLINE_CACHE_CAPACITY 512000

#define CACHE_COMPRESSION_LEVEL 1

static const char* observerList[] = {
    "profile-before-change",        "profile-do-change",
    NS_XPCOM_SHUTDOWN_OBSERVER_ID,  "last-pb-context-exited",
    "suspend_process_notification", "resume_process_notification"};

static const char* prefList[] = {
    OFFLINE_CACHE_ENABLE_PREF,
    OFFLINE_CACHE_CAPACITY_PREF,
    OFFLINE_CACHE_DIR_PREF,
    nullptr,
};

class nsCacheProfilePrefObserver : public nsIObserver {
  virtual ~nsCacheProfilePrefObserver() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsCacheProfilePrefObserver()
      : mHaveProfile(false),
        mOfflineCacheEnabled(false),
        mOfflineCacheCapacity(0),
        mCacheCompressionLevel(CACHE_COMPRESSION_LEVEL),
        mSanitizeOnShutdown(false),
        mClearCacheOnShutdown(false) {}

  nsresult Install();
  void Remove();
  nsresult ReadPrefs(nsIPrefBranch* branch);

  nsIFile* DiskCacheParentDirectory() { return mDiskCacheParentDirectory; }

  bool OfflineCacheEnabled();
  int32_t OfflineCacheCapacity() { return mOfflineCacheCapacity; }
  nsIFile* OfflineCacheParentDirectory() {
    return mOfflineCacheParentDirectory;
  }

  int32_t CacheCompressionLevel();

  bool SanitizeAtShutdown() {
    return mSanitizeOnShutdown && mClearCacheOnShutdown;
  }

  void PrefChanged(const char* aPref);

 private:
  bool mHaveProfile;

  nsCOMPtr<nsIFile> mDiskCacheParentDirectory;

  bool mOfflineCacheEnabled;
  int32_t mOfflineCacheCapacity;  // in kilobytes
  nsCOMPtr<nsIFile> mOfflineCacheParentDirectory;

  int32_t mCacheCompressionLevel;

  bool mSanitizeOnShutdown;
  bool mClearCacheOnShutdown;
};

NS_IMPL_ISUPPORTS(nsCacheProfilePrefObserver, nsIObserver)

class nsBlockOnCacheThreadEvent : public Runnable {
 public:
  nsBlockOnCacheThreadEvent()
      : mozilla::Runnable("nsBlockOnCacheThreadEvent") {}
  NS_IMETHOD Run() override {
    nsCacheServiceAutoLock autoLock(LOCK_TELEM(NSBLOCKONCACHETHREADEVENT_RUN));
    CACHE_LOG_DEBUG(("nsBlockOnCacheThreadEvent [%p]\n", this));
    nsCacheService::gService->mNotified = true;
    nsCacheService::gService->mCondVar.Notify();
    return NS_OK;
  }
};

nsresult nsCacheProfilePrefObserver::Install() {
  // install profile-change observer
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) return NS_ERROR_FAILURE;

  nsresult rv, rv2 = NS_OK;
  for (auto& observer : observerList) {
    rv = observerService->AddObserver(this, observer, false);
    if (NS_FAILED(rv)) rv2 = rv;
  }

  // install preferences observer
  nsCOMPtr<nsIPrefBranch> branch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!branch) return NS_ERROR_FAILURE;

  Preferences::RegisterCallbacks(
      PREF_CHANGE_METHOD(nsCacheProfilePrefObserver::PrefChanged), prefList,
      this);

  // Determine if we have a profile already
  //     Install() is called *after* the profile-after-change notification
  //     when there is only a single profile, or it is specified on the
  //     commandline at startup.
  //     In that case, we detect the presence of a profile by the existence
  //     of the NS_APP_USER_PROFILE_50_DIR directory.

  nsCOMPtr<nsIFile> directory;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(directory));
  if (NS_SUCCEEDED(rv)) mHaveProfile = true;

  rv = ReadPrefs(branch);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv2;
}

void nsCacheProfilePrefObserver::Remove() {
  // remove Observer Service observers
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    for (auto& observer : observerList) {
      obs->RemoveObserver(this, observer);
    }
  }

  // remove Pref Service observers
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs) return;
  Preferences::UnregisterCallbacks(
      PREF_CHANGE_METHOD(nsCacheProfilePrefObserver::PrefChanged), prefList,
      this);
}

NS_IMETHODIMP
nsCacheProfilePrefObserver::Observe(nsISupports* subject, const char* topic,
                                    const char16_t* data_unicode) {
  NS_ConvertUTF16toUTF8 data(data_unicode);
  CACHE_LOG_INFO(("Observe [topic=%s data=%s]\n", topic, data.get()));

  if (!nsCacheService::IsInitialized()) {
    if (!strcmp("resume_process_notification", topic)) {
      // A suspended process has a closed cache, so re-open it here.
      nsCacheService::GlobalInstance()->Init();
    }
    return NS_OK;
  }

  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, topic)) {
    // xpcom going away, shutdown cache service
    nsCacheService::GlobalInstance()->Shutdown();
  } else if (!strcmp("profile-before-change", topic)) {
    // profile before change
    mHaveProfile = false;

    // XXX shutdown devices
    nsCacheService::OnProfileShutdown();
  } else if (!strcmp("suspend_process_notification", topic)) {
    // A suspended process may never return, so shutdown the cache to reduce
    // cache corruption.
    nsCacheService::GlobalInstance()->Shutdown();
  } else if (!strcmp("profile-do-change", topic)) {
    // profile after change
    mHaveProfile = true;
    nsCOMPtr<nsIPrefBranch> branch = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!branch) {
      return NS_ERROR_FAILURE;
    }
    (void)ReadPrefs(branch);
    nsCacheService::OnProfileChanged();

  } else if (!strcmp("last-pb-context-exited", topic)) {
    nsCacheService::LeavePrivateBrowsing();
  }

  return NS_OK;
}

void nsCacheProfilePrefObserver::PrefChanged(const char* aPref) {
  // ignore pref changes until we're done switch profiles
  if (!mHaveProfile) return;
  // which preference changed?
  nsresult rv;
  if (!strcmp(OFFLINE_CACHE_ENABLE_PREF, aPref)) {
    rv = Preferences::GetBool(OFFLINE_CACHE_ENABLE_PREF, &mOfflineCacheEnabled);
    if (NS_FAILED(rv)) return;
    nsCacheService::SetOfflineCacheEnabled(OfflineCacheEnabled());

  } else if (!strcmp(OFFLINE_CACHE_CAPACITY_PREF, aPref)) {
    int32_t capacity = 0;
    rv = Preferences::GetInt(OFFLINE_CACHE_CAPACITY_PREF, &capacity);
    if (NS_FAILED(rv)) return;
    mOfflineCacheCapacity = std::max(0, capacity);
    nsCacheService::SetOfflineCacheCapacity(mOfflineCacheCapacity);
#if 0
    } else if (!strcmp(OFFLINE_CACHE_DIR_PREF, aPref)) {
        // XXX We probaby don't want to respond to this pref except after
        // XXX profile changes.  Ideally, there should be some kind of user
        // XXX notification that the pref change won't take effect until
        // XXX the next time the profile changes (browser launch)
#endif
  }
}

nsresult nsCacheProfilePrefObserver::ReadPrefs(nsIPrefBranch* branch) {
  nsresult rv = NS_OK;

  if (!mDiskCacheParentDirectory) {
    nsCOMPtr<nsIFile> directory;

    // try to get the disk cache parent directory
    rv = NS_GetSpecialDirectory(NS_APP_CACHE_PARENT_DIR,
                                getter_AddRefs(directory));
    if (NS_FAILED(rv)) {
      // try to get the profile directory (there may not be a profile yet)
      nsCOMPtr<nsIFile> profDir;
      NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                             getter_AddRefs(profDir));
      NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                             getter_AddRefs(directory));
      if (!directory)
        directory = profDir;
      else if (profDir) {
        nsCacheService::MoveOrRemoveDiskCache(profDir, directory, "Cache");
      }
    }
    // use file cache in build tree only if asked, to avoid cache dir litter
    if (!directory && PR_GetEnv("NECKO_DEV_ENABLE_DISK_CACHE")) {
      rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                  getter_AddRefs(directory));
    }
    if (directory) {
      mDiskCacheParentDirectory = directory;
    }
  }

  // read offline cache device prefs
  mOfflineCacheEnabled = true;  // presume offline cache is enabled
  (void)branch->GetBoolPref(OFFLINE_CACHE_ENABLE_PREF, &mOfflineCacheEnabled);

  mOfflineCacheCapacity = OFFLINE_CACHE_CAPACITY;
  (void)branch->GetIntPref(OFFLINE_CACHE_CAPACITY_PREF, &mOfflineCacheCapacity);
  mOfflineCacheCapacity = std::max(0, mOfflineCacheCapacity);

  (void)branch->GetComplexValue(OFFLINE_CACHE_DIR_PREF,  // ignore error
                                NS_GET_IID(nsIFile),
                                getter_AddRefs(mOfflineCacheParentDirectory));

  if (!mOfflineCacheParentDirectory) {
    nsCOMPtr<nsIFile> directory;

    // try to get the offline cache parent directory
    rv = NS_GetSpecialDirectory(NS_APP_CACHE_PARENT_DIR,
                                getter_AddRefs(directory));
    if (NS_FAILED(rv)) {
      // try to get the profile directory (there may not be a profile yet)
      nsCOMPtr<nsIFile> profDir;
      NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                             getter_AddRefs(profDir));
      NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                             getter_AddRefs(directory));
      if (!directory)
        directory = profDir;
      else if (profDir) {
        nsCacheService::MoveOrRemoveDiskCache(profDir, directory,
                                              "OfflineCache");
      }
    }
#if DEBUG
    if (!directory) {
      // use current process directory during development
      rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                  getter_AddRefs(directory));
    }
#endif
    if (directory) {
      mOfflineCacheParentDirectory = directory;
    }
  }

  return rv;
}

nsresult nsCacheService::DispatchToCacheIOThread(nsIRunnable* event) {
  if (!gService || !gService->mCacheIOThread) return NS_ERROR_NOT_AVAILABLE;
  return gService->mCacheIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

nsresult nsCacheService::SyncWithCacheIOThread() {
  if (!gService || !gService->mCacheIOThread) return NS_ERROR_NOT_AVAILABLE;
  gService->mLock.AssertCurrentThreadOwns();

  nsCOMPtr<nsIRunnable> event = new nsBlockOnCacheThreadEvent();

  // dispatch event - it will notify the monitor when it's done
  nsresult rv = gService->mCacheIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed dispatching block-event");
    return NS_ERROR_UNEXPECTED;
  }

  // wait until notified, then return
  gService->mNotified = false;
  while (!gService->mNotified) {
    gService->mCondVar.Wait();
  }

  return NS_OK;
}

bool nsCacheProfilePrefObserver::OfflineCacheEnabled() {
  if ((mOfflineCacheCapacity == 0) || (!mOfflineCacheParentDirectory))
    return false;

  return mOfflineCacheEnabled;
}

int32_t nsCacheProfilePrefObserver::CacheCompressionLevel() {
  return mCacheCompressionLevel;
}

/******************************************************************************
 * nsProcessRequestEvent
 *****************************************************************************/

class nsProcessRequestEvent : public Runnable {
 public:
  explicit nsProcessRequestEvent(nsCacheRequest* aRequest)
      : mozilla::Runnable("nsProcessRequestEvent") {
    mRequest = aRequest;
  }

  NS_IMETHOD Run() override {
    nsresult rv;

    NS_ASSERTION(mRequest->mListener,
                 "Sync OpenCacheEntry() posted to background thread!");

    nsCacheServiceAutoLock lock(LOCK_TELEM(NSPROCESSREQUESTEVENT_RUN));
    rv = nsCacheService::gService->ProcessRequest(mRequest, false, nullptr);

    // Don't delete the request if it was queued
    if (!(mRequest->IsBlocking() && rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION))
      delete mRequest;

    return NS_OK;
  }

 protected:
  virtual ~nsProcessRequestEvent() = default;

 private:
  nsCacheRequest* mRequest;
};

/******************************************************************************
 * nsDoomEvent
 *****************************************************************************/

class nsDoomEvent : public Runnable {
 public:
  nsDoomEvent(nsCacheSession* session, const nsACString& key,
              nsICacheListener* listener)
      : mozilla::Runnable("nsDoomEvent") {
    mKey = *session->ClientID();
    mKey.Append(':');
    mKey.Append(key);
    mStoragePolicy = session->StoragePolicy();
    mListener = listener;
    mEventTarget = GetCurrentThreadEventTarget();
    // We addref the listener here and release it in nsNotifyDoomListener
    // on the callers thread. If posting of nsNotifyDoomListener event fails
    // we leak the listener which is better than releasing it on a wrong
    // thread.
    NS_IF_ADDREF(mListener);
  }

  NS_IMETHOD Run() override {
    nsCacheServiceAutoLock lock;

    bool foundActive = true;
    nsresult status = NS_ERROR_NOT_AVAILABLE;
    nsCacheEntry* entry;
    entry = nsCacheService::gService->mActiveEntries.GetEntry(&mKey);
    if (!entry) {
      bool collision = false;
      foundActive = false;
      entry = nsCacheService::gService->SearchCacheDevices(
          &mKey, mStoragePolicy, &collision);
    }

    if (entry) {
      status = NS_OK;
      nsCacheService::gService->DoomEntry_Internal(entry, foundActive);
    }

    if (mListener) {
      mEventTarget->Dispatch(new nsNotifyDoomListener(mListener, status),
                             NS_DISPATCH_NORMAL);
      // posted event will release the reference on the correct thread
      mListener = nullptr;
    }

    return NS_OK;
  }

 private:
  nsCString mKey;
  nsCacheStoragePolicy mStoragePolicy;
  nsICacheListener* mListener;
  nsCOMPtr<nsIEventTarget> mEventTarget;
};

/******************************************************************************
 * nsCacheService
 *****************************************************************************/
nsCacheService* nsCacheService::gService = nullptr;

NS_IMPL_ISUPPORTS(nsCacheService, nsICacheService, nsICacheServiceInternal)

nsCacheService::nsCacheService()
    : mObserver(nullptr),
      mLock("nsCacheService.mLock"),
      mCondVar(mLock, "nsCacheService.mCondVar"),
      mNotified(false),
      mTimeStampLock("nsCacheService.mTimeStampLock"),
      mInitialized(false),
      mClearingEntries(false),
      mEnableOfflineDevice(false),
      mOfflineDevice(nullptr),
      mDoomedEntries{},
      mTotalEntries(0),
      mCacheHits(0),
      mCacheMisses(0),
      mMaxKeyLength(0),
      mMaxDataSize(0),
      mMaxMetaSize(0),
      mDeactivateFailures(0),
      mDeactivatedUnboundEntries(0) {
  NS_ASSERTION(gService == nullptr, "multiple nsCacheService instances!");
  gService = this;

  // create list of cache devices
  PR_INIT_CLIST(&mDoomedEntries);
}

nsCacheService::~nsCacheService() {
  if (mInitialized)  // Shutdown hasn't been called yet.
    (void)Shutdown();

  if (mObserver) {
    mObserver->Remove();
    NS_RELEASE(mObserver);
  }

  gService = nullptr;
}

nsresult nsCacheService::Init() {
  // Thie method must be called on the main thread because mCacheIOThread must
  // only be modified on the main thread.
  if (!NS_IsMainThread()) {
    NS_ERROR("nsCacheService::Init called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_ASSERTION(!mInitialized, "nsCacheService already initialized.");
  if (mInitialized) return NS_ERROR_ALREADY_INITIALIZED;

  if (mozilla::net::IsNeckoChild()) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv;

  mStorageService = do_GetService("@mozilla.org/storage/service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewNamedThread("Cache I/O", getter_AddRefs(mCacheIOThread));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create cache IO thread");
  }

  rv = nsDeleteDir::Init();
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't initialize nsDeleteDir");
  }

  // initialize hashtable for active cache entries
  mActiveEntries.Init();

  // create profile/preference observer
  if (!mObserver) {
    mObserver = new nsCacheProfilePrefObserver();
    NS_ADDREF(mObserver);
    mObserver->Install();
  }

  mEnableOfflineDevice = mObserver->OfflineCacheEnabled();

  mInitialized = true;
  return NS_OK;
}

void nsCacheService::Shutdown() {
  // This method must be called on the main thread because mCacheIOThread must
  // only be modified on the main thread.
  if (!NS_IsMainThread()) {
    MOZ_CRASH("nsCacheService::Shutdown called off the main thread");
  }

  nsCOMPtr<nsIThread> cacheIOThread;
  Telemetry::AutoTimer<Telemetry::NETWORK_DISK_CACHE_SHUTDOWN> totalTimer;

  bool shouldSanitize = false;
  nsCOMPtr<nsIFile> parentDir;

  {
    nsCacheServiceAutoLock lock(LOCK_TELEM(NSCACHESERVICE_SHUTDOWN));
    NS_ASSERTION(
        mInitialized,
        "can't shutdown nsCacheService unless it has been initialized.");
    if (!mInitialized) return;

    mClearingEntries = true;
    DoomActiveEntries(nullptr);
  }

  CloseAllStreams();

  {
    nsCacheServiceAutoLock lock(LOCK_TELEM(NSCACHESERVICE_SHUTDOWN));
    NS_ASSERTION(mInitialized, "Bad state");

    mInitialized = false;

    // Clear entries
    ClearDoomList();

    // Make sure to wait for any pending cache-operations before
    // proceeding with destructive actions (bug #620660)
    (void)SyncWithCacheIOThread();
    mActiveEntries.Shutdown();

    // obtain the disk cache directory in case we need to sanitize it
    parentDir = mObserver->DiskCacheParentDirectory();
    shouldSanitize = mObserver->SanitizeAtShutdown();

    if (mOfflineDevice) mOfflineDevice->Shutdown();

    NS_IF_RELEASE(mOfflineDevice);

    for (auto iter = mCustomOfflineDevices.Iter(); !iter.Done(); iter.Next()) {
      iter.Data()->Shutdown();
      iter.Remove();
    }

    LogCacheStatistics();

    mClearingEntries = false;
    mCacheIOThread.swap(cacheIOThread);
  }

  if (cacheIOThread) nsShutdownThread::BlockingShutdown(cacheIOThread);

  if (shouldSanitize) {
    nsresult rv = parentDir->AppendNative(NS_LITERAL_CSTRING("Cache"));
    if (NS_SUCCEEDED(rv)) {
      bool exists;
      if (NS_SUCCEEDED(parentDir->Exists(&exists)) && exists)
        nsDeleteDir::DeleteDir(parentDir, false);
    }
    Telemetry::AutoTimer<Telemetry::NETWORK_DISK_CACHE_SHUTDOWN_CLEAR_PRIVATE>
        timer;
    nsDeleteDir::Shutdown(shouldSanitize);
  } else {
    Telemetry::AutoTimer<Telemetry::NETWORK_DISK_CACHE_DELETEDIR_SHUTDOWN>
        timer;
    nsDeleteDir::Shutdown(shouldSanitize);
  }
}

nsresult nsCacheService::Create(nsISupports* aOuter, const nsIID& aIID,
                                void** aResult) {
  nsresult rv;

  if (aOuter != nullptr) return NS_ERROR_NO_AGGREGATION;

  RefPtr<nsCacheService> cacheService = new nsCacheService();
  rv = cacheService->Init();
  if (NS_SUCCEEDED(rv)) {
    rv = cacheService->QueryInterface(aIID, aResult);
  }
  return rv;
}

NS_IMETHODIMP
nsCacheService::CreateSession(const char* clientID,
                              nsCacheStoragePolicy storagePolicy,
                              bool streamBased, nsICacheSession** result) {
  *result = nullptr;

  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsCacheService::CreateSessionInternal(
    const char* clientID, nsCacheStoragePolicy storagePolicy, bool streamBased,
    nsICacheSession** result) {
  RefPtr<nsCacheSession> session =
      new nsCacheSession(clientID, storagePolicy, streamBased);
  session.forget(result);

  return NS_OK;
}

nsresult nsCacheService::EvictEntriesForSession(nsCacheSession* session) {
  NS_ASSERTION(gService, "nsCacheService::gService is null.");
  return gService->EvictEntriesForClient(session->ClientID()->get(),
                                         session->StoragePolicy());
}

namespace {

class EvictionNotifierRunnable : public Runnable {
 public:
  explicit EvictionNotifierRunnable(nsISupports* aSubject)
      : mozilla::Runnable("EvictionNotifierRunnable"), mSubject(aSubject) {}

  NS_DECL_NSIRUNNABLE

 private:
  nsCOMPtr<nsISupports> mSubject;
};

NS_IMETHODIMP
EvictionNotifierRunnable::Run() {
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  if (obsSvc) {
    obsSvc->NotifyObservers(mSubject, NS_CACHESERVICE_EMPTYCACHE_TOPIC_ID,
                            nullptr);
  }
  return NS_OK;
}

}  // namespace

nsresult nsCacheService::EvictEntriesForClient(
    const char* clientID, nsCacheStoragePolicy storagePolicy) {
  RefPtr<EvictionNotifierRunnable> r =
      new EvictionNotifierRunnable(NS_ISUPPORTS_CAST(nsICacheService*, this));
  NS_DispatchToMainThread(r);

  nsCacheServiceAutoLock lock(LOCK_TELEM(NSCACHESERVICE_EVICTENTRIESFORCLIENT));
  nsresult res = NS_OK;

  // Only clear the offline cache if it has been specifically asked for.
  if (storagePolicy == nsICache::STORE_OFFLINE) {
    if (mEnableOfflineDevice) {
      nsresult rv = NS_OK;
      if (!mOfflineDevice) rv = CreateOfflineDevice();
      if (mOfflineDevice) rv = mOfflineDevice->EvictEntries(clientID);
      if (NS_FAILED(rv)) res = rv;
    }
  }

  return res;
}

nsresult nsCacheService::IsStorageEnabledForPolicy(
    nsCacheStoragePolicy storagePolicy, bool* result) {
  if (gService == nullptr) return NS_ERROR_NOT_AVAILABLE;
  nsCacheServiceAutoLock lock(
      LOCK_TELEM(NSCACHESERVICE_ISSTORAGEENABLEDFORPOLICY));

  *result = nsCacheService::IsStorageEnabledForPolicy_Locked(storagePolicy);
  return NS_OK;
}

nsresult nsCacheService::DoomEntry(nsCacheSession* session,
                                   const nsACString& key,
                                   nsICacheListener* listener) {
  CACHE_LOG_DEBUG(("Dooming entry for session %p, key %s\n", session,
                   PromiseFlatCString(key).get()));
  if (!gService || !gService->mInitialized) return NS_ERROR_NOT_INITIALIZED;

  return DispatchToCacheIOThread(new nsDoomEvent(session, key, listener));
}

bool nsCacheService::IsStorageEnabledForPolicy_Locked(
    nsCacheStoragePolicy storagePolicy) {
  if (gService->mEnableOfflineDevice &&
      storagePolicy == nsICache::STORE_OFFLINE) {
    return true;
  }

  return false;
}

NS_IMETHODIMP nsCacheService::VisitEntries(nsICacheVisitor* visitor) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsCacheService::VisitEntriesInternal(nsICacheVisitor* visitor) {
  NS_ENSURE_ARG_POINTER(visitor);

  nsCacheServiceAutoLock lock(LOCK_TELEM(NSCACHESERVICE_VISITENTRIES));

  if (!mEnableOfflineDevice) return NS_ERROR_NOT_AVAILABLE;

  // XXX record the fact that a visitation is in progress,
  // XXX i.e. keep list of visitors in progress.

  nsresult rv = NS_OK;

  if (mEnableOfflineDevice) {
    if (!mOfflineDevice) {
      rv = CreateOfflineDevice();
      if (NS_FAILED(rv)) return rv;
    }
    rv = mOfflineDevice->Visit(visitor);
    if (NS_FAILED(rv)) return rv;
  }

  // XXX notify any shutdown process that visitation is complete for THIS
  // visitor.
  // XXX keep queue of visitors

  return NS_OK;
}

void nsCacheService::FireClearNetworkCacheStoredAnywhereNotification() {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> obsvc = mozilla::services::GetObserverService();
  if (obsvc) {
    obsvc->NotifyObservers(nullptr, "network-clear-cache-stored-anywhere",
                           nullptr);
  }
}

NS_IMETHODIMP nsCacheService::EvictEntries(nsCacheStoragePolicy storagePolicy) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsCacheService::EvictEntriesInternal(
    nsCacheStoragePolicy storagePolicy) {
  if (storagePolicy == nsICache::STORE_ANYWHERE) {
    // if not called on main thread, dispatch the notification to the main
    // thread to notify observers
    if (!NS_IsMainThread()) {
      nsCOMPtr<nsIRunnable> event = NewRunnableMethod(
          "nsCacheService::FireClearNetworkCacheStoredAnywhereNotification",
          this,
          &nsCacheService::FireClearNetworkCacheStoredAnywhereNotification);
      NS_DispatchToMainThread(event);
    } else {
      // else you're already on main thread - notify observers
      FireClearNetworkCacheStoredAnywhereNotification();
    }
  }
  return EvictEntriesForClient(nullptr, storagePolicy);
}

NS_IMETHODIMP nsCacheService::GetCacheIOTarget(
    nsIEventTarget** aCacheIOTarget) {
  NS_ENSURE_ARG_POINTER(aCacheIOTarget);

  // Because mCacheIOThread can only be changed on the main thread, it can be
  // read from the main thread without the lock. This is useful to prevent
  // blocking the main thread on other cache operations.
  if (!NS_IsMainThread()) {
    Lock(LOCK_TELEM(NSCACHESERVICE_GETCACHEIOTARGET));
  }

  nsresult rv;
  if (mCacheIOThread) {
    NS_ADDREF(*aCacheIOTarget = mCacheIOThread);
    rv = NS_OK;
  } else {
    *aCacheIOTarget = nullptr;
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  if (!NS_IsMainThread()) {
    Unlock();
  }

  return rv;
}

NS_IMETHODIMP nsCacheService::GetLockHeldTime(double* aLockHeldTime) {
  MutexAutoLock lock(mTimeStampLock);

  if (mLockAcquiredTimeStamp.IsNull()) {
    *aLockHeldTime = 0.0;
  } else {
    *aLockHeldTime =
        (TimeStamp::Now() - mLockAcquiredTimeStamp).ToMilliseconds();
  }

  return NS_OK;
}

/**
 * Internal Methods
 */
nsresult nsCacheService::GetOfflineDevice(nsOfflineCacheDevice** aDevice) {
  if (!mOfflineDevice) {
    nsresult rv = CreateOfflineDevice();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ADDREF(*aDevice = mOfflineDevice);
  return NS_OK;
}

nsresult nsCacheService::GetCustomOfflineDevice(
    nsIFile* aProfileDir, int32_t aQuota, nsOfflineCacheDevice** aDevice) {
  nsresult rv;

  nsAutoString profilePath;
  rv = aProfileDir->GetPath(profilePath);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mCustomOfflineDevices.Get(profilePath, aDevice)) {
    rv = CreateCustomOfflineDevice(aProfileDir, aQuota, aDevice);
    NS_ENSURE_SUCCESS(rv, rv);

    (*aDevice)->SetAutoShutdown();
    mCustomOfflineDevices.Put(profilePath, *aDevice);
  }

  return NS_OK;
}

nsresult nsCacheService::CreateOfflineDevice() {
  CACHE_LOG_INFO(("Creating default offline device"));

  if (mOfflineDevice) return NS_OK;
  if (!nsCacheService::IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = CreateCustomOfflineDevice(
      mObserver->OfflineCacheParentDirectory(),
      mObserver->OfflineCacheCapacity(), &mOfflineDevice);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsCacheService::CreateCustomOfflineDevice(
    nsIFile* aProfileDir, int32_t aQuota, nsOfflineCacheDevice** aDevice) {
  NS_ENSURE_ARG(aProfileDir);

  if (MOZ_LOG_TEST(gCacheLog, LogLevel::Info)) {
    CACHE_LOG_INFO(("Creating custom offline device, %s, %d",
                    aProfileDir->HumanReadablePath().get(), aQuota));
  }

  if (!mInitialized) return NS_ERROR_NOT_AVAILABLE;
  if (!mEnableOfflineDevice) return NS_ERROR_NOT_AVAILABLE;

  RefPtr<nsOfflineCacheDevice> device = new nsOfflineCacheDevice();

  // set the preferences
  device->SetCacheParentDirectory(aProfileDir);
  device->SetCapacity(aQuota);

  nsresult rv = device->InitWithSqlite(mStorageService);
  if (NS_FAILED(rv)) {
    CACHE_LOG_DEBUG(("OfflineDevice->InitWithSqlite() failed (0x%.8" PRIx32
                     ")\n",
                     static_cast<uint32_t>(rv)));
    CACHE_LOG_DEBUG(("    - disabling offline cache for this session.\n"));
    device = nullptr;
  }

  device.forget(aDevice);
  return rv;
}

nsresult nsCacheService::RemoveCustomOfflineDevice(
    nsOfflineCacheDevice* aDevice) {
  nsCOMPtr<nsIFile> profileDir = aDevice->BaseDirectory();
  if (!profileDir) return NS_ERROR_UNEXPECTED;

  nsAutoString profilePath;
  nsresult rv = profileDir->GetPath(profilePath);
  NS_ENSURE_SUCCESS(rv, rv);

  mCustomOfflineDevices.Remove(profilePath);
  return NS_OK;
}

nsresult nsCacheService::CreateRequest(nsCacheSession* session,
                                       const nsACString& clientKey,
                                       nsCacheAccessMode accessRequested,
                                       bool blockingMode,
                                       nsICacheListener* listener,
                                       nsCacheRequest** request) {
  NS_ASSERTION(request, "CreateRequest: request is null");

  nsAutoCString key(*session->ClientID());
  key.Append(':');
  key.Append(clientKey);

  if (mMaxKeyLength < key.Length()) mMaxKeyLength = key.Length();

  // create request
  *request =
      new nsCacheRequest(key, listener, accessRequested, blockingMode, session);

  if (!listener) return NS_OK;  // we're sync, we're done.

  // get the request's thread
  (*request)->mEventTarget = GetCurrentThreadEventTarget();

  return NS_OK;
}

class nsCacheListenerEvent : public Runnable {
 public:
  nsCacheListenerEvent(nsICacheListener* listener,
                       nsICacheEntryDescriptor* descriptor,
                       nsCacheAccessMode accessGranted, nsresult status)
      : mozilla::Runnable("nsCacheListenerEvent"),
        mListener(listener)  // transfers reference
        ,
        mDescriptor(descriptor)  // transfers reference (may be null)
        ,
        mAccessGranted(accessGranted),
        mStatus(status) {}

  NS_IMETHOD Run() override {
    mListener->OnCacheEntryAvailable(mDescriptor, mAccessGranted, mStatus);

    NS_RELEASE(mListener);
    NS_IF_RELEASE(mDescriptor);
    return NS_OK;
  }

 private:
  // We explicitly leak mListener or mDescriptor if Run is not called
  // because otherwise we cannot guarantee that they are destroyed on
  // the right thread.

  nsICacheListener* mListener;
  nsICacheEntryDescriptor* mDescriptor;
  nsCacheAccessMode mAccessGranted;
  nsresult mStatus;
};

nsresult nsCacheService::NotifyListener(nsCacheRequest* request,
                                        nsICacheEntryDescriptor* descriptor,
                                        nsCacheAccessMode accessGranted,
                                        nsresult status) {
  NS_ASSERTION(request->mEventTarget, "no thread set in async request!");

  // Swap ownership, and release listener on target thread...
  nsICacheListener* listener = request->mListener;
  request->mListener = nullptr;

  nsCOMPtr<nsIRunnable> ev =
      new nsCacheListenerEvent(listener, descriptor, accessGranted, status);
  if (!ev) {
    // Better to leak listener and descriptor if we fail because we don't
    // want to destroy them inside the cache service lock or on potentially
    // the wrong thread.
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return request->mEventTarget->Dispatch(ev, NS_DISPATCH_NORMAL);
}

nsresult nsCacheService::ProcessRequest(nsCacheRequest* request,
                                        bool calledFromOpenCacheEntry,
                                        nsICacheEntryDescriptor** result) {
  // !!! must be called with mLock held !!!
  nsresult rv;
  nsCacheEntry* entry = nullptr;
  nsCacheEntry* doomedEntry = nullptr;
  nsCacheAccessMode accessGranted = nsICache::ACCESS_NONE;
  if (result) *result = nullptr;

  while (true) {  // Activate entry loop
    rv = ActivateEntry(request, &entry,
                       &doomedEntry);  // get the entry for this request
    if (NS_FAILED(rv)) break;

    while (true) {  // Request Access loop
      NS_ASSERTION(entry, "no entry in Request Access loop!");
      // entry->RequestAccess queues request on entry
      rv = entry->RequestAccess(request, &accessGranted);
      if (rv != NS_ERROR_CACHE_WAIT_FOR_VALIDATION) break;

      if (request->IsBlocking()) {
        if (request->mListener) {
          // async exits - validate, doom, or close will resume
          return rv;
        }

        // XXX this is probably wrong...
        Unlock();
        rv = request->WaitForValidation();
        Lock(LOCK_TELEM(NSCACHESERVICE_PROCESSREQUEST));
      }

      PR_REMOVE_AND_INIT_LINK(request);
      if (NS_FAILED(rv))
        break;  // non-blocking mode returns WAIT_FOR_VALIDATION error
      // okay, we're ready to process this request, request access again
    }
    if (rv != NS_ERROR_CACHE_ENTRY_DOOMED) break;

    if (entry->IsNotInUse()) {
      // this request was the last one keeping it around, so get rid of it
      DeactivateEntry(entry);
    }
    // loop back around to look for another entry
  }

  if (NS_SUCCEEDED(rv) && request->mProfileDir) {
    // Custom cache directory has been demanded.  Preset the cache device.
    if (entry->StoragePolicy() != nsICache::STORE_OFFLINE) {
      // Failsafe check: this is implemented only for offline cache atm.
      rv = NS_ERROR_FAILURE;
    } else {
      RefPtr<nsOfflineCacheDevice> customCacheDevice;
      rv = GetCustomOfflineDevice(request->mProfileDir, -1,
                                  getter_AddRefs(customCacheDevice));
      if (NS_SUCCEEDED(rv)) entry->SetCustomCacheDevice(customCacheDevice);
    }
  }

  nsICacheEntryDescriptor* descriptor = nullptr;

  if (NS_SUCCEEDED(rv))
    rv = entry->CreateDescriptor(request, accessGranted, &descriptor);

  // If doomedEntry is set, ActivatEntry() doomed an existing entry and
  // created a new one for that cache-key. However, any pending requests
  // on the doomed entry were not processed and we need to do that here.
  // This must be done after adding the created entry to list of active
  // entries (which is done in ActivateEntry()) otherwise the hashkeys crash
  // (see bug ##561313). It is also important to do this after creating a
  // descriptor for this request, or some other request may end up being
  // executed first for the newly created entry.
  // Finally, it is worth to emphasize that if doomedEntry is set,
  // ActivateEntry() created a new entry for the request, which will be
  // initialized by RequestAccess() and they both should have returned NS_OK.
  if (doomedEntry) {
    (void)ProcessPendingRequests(doomedEntry);
    if (doomedEntry->IsNotInUse()) DeactivateEntry(doomedEntry);
    doomedEntry = nullptr;
  }

  if (request->mListener) {  // Asynchronous

    if (NS_FAILED(rv) && calledFromOpenCacheEntry && request->IsBlocking())
      return rv;  // skip notifying listener, just return rv to caller

    // call listener to report error or descriptor
    nsresult rv2 = NotifyListener(request, descriptor, accessGranted, rv);
    if (NS_FAILED(rv2) && NS_SUCCEEDED(rv)) {
      rv = rv2;  // trigger delete request
    }
  } else {  // Synchronous
    *result = descriptor;
  }
  return rv;
}

nsresult nsCacheService::OpenCacheEntry(nsCacheSession* session,
                                        const nsACString& key,
                                        nsCacheAccessMode accessRequested,
                                        bool blockingMode,
                                        nsICacheListener* listener,
                                        nsICacheEntryDescriptor** result) {
  CACHE_LOG_DEBUG(
      ("Opening entry for session %p, key %s, mode %d, blocking %d\n", session,
       PromiseFlatCString(key).get(), accessRequested, blockingMode));
  if (result) *result = nullptr;

  if (!gService || !gService->mInitialized) return NS_ERROR_NOT_INITIALIZED;

  nsCacheRequest* request = nullptr;

  nsresult rv = gService->CreateRequest(session, key, accessRequested,
                                        blockingMode, listener, &request);
  if (NS_FAILED(rv)) return rv;

  CACHE_LOG_DEBUG(("Created request %p\n", request));

  // Process the request on the background thread if we are on the main thread
  // and the the request is asynchronous
  if (NS_IsMainThread() && listener && gService->mCacheIOThread) {
    nsCOMPtr<nsIRunnable> ev = new nsProcessRequestEvent(request);
    rv = DispatchToCacheIOThread(ev);

    // delete request if we didn't post the event
    if (NS_FAILED(rv)) delete request;
  } else {
    nsCacheServiceAutoLock lock(LOCK_TELEM(NSCACHESERVICE_OPENCACHEENTRY));
    rv = gService->ProcessRequest(request, true, result);

    // delete requests that have completed
    if (!(listener && blockingMode &&
          (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION)))
      delete request;
  }

  return rv;
}

nsresult nsCacheService::ActivateEntry(nsCacheRequest* request,
                                       nsCacheEntry** result,
                                       nsCacheEntry** doomedEntry) {
  CACHE_LOG_DEBUG(("Activate entry for request %p\n", request));
  if (!mInitialized || mClearingEntries) return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = NS_OK;

  NS_ASSERTION(request != nullptr, "ActivateEntry called with no request");
  if (result) *result = nullptr;
  if (doomedEntry) *doomedEntry = nullptr;
  if ((!request) || (!result) || (!doomedEntry)) return NS_ERROR_NULL_POINTER;

  // check if the request can be satisfied
  if (!request->IsStreamBased()) return NS_ERROR_FAILURE;
  if (!IsStorageEnabledForPolicy_Locked(request->StoragePolicy()))
    return NS_ERROR_FAILURE;

  // search active entries (including those not bound to device)
  nsCacheEntry* entry = mActiveEntries.GetEntry(&(request->mKey));
  CACHE_LOG_DEBUG(("Active entry for request %p is %p\n", request, entry));

  if (!entry) {
    // search cache devices for entry
    bool collision = false;
    entry = SearchCacheDevices(&(request->mKey), request->StoragePolicy(),
                               &collision);
    CACHE_LOG_DEBUG(
        ("Device search for request %p returned %p\n", request, entry));
    // When there is a hashkey collision just refuse to cache it...
    if (collision) return NS_ERROR_CACHE_IN_USE;

    if (entry) entry->MarkInitialized();
  } else {
    NS_ASSERTION(entry->IsActive(), "Inactive entry found in mActiveEntries!");
  }

  if (entry) {
    ++mCacheHits;
    entry->Fetched();
  } else {
    ++mCacheMisses;
  }

  if (entry && ((request->AccessRequested() == nsICache::ACCESS_WRITE) ||
                ((request->StoragePolicy() != nsICache::STORE_OFFLINE) &&
                 (entry->mExpirationTime <= SecondsFromPRTime(PR_Now()) &&
                  request->WillDoomEntriesIfExpired()))))

  {
    // this is FORCE-WRITE request or the entry has expired
    // we doom entry without processing pending requests, but store it in
    // doomedEntry which causes pending requests to be processed below
    rv = DoomEntry_Internal(entry, false);
    *doomedEntry = entry;
    if (NS_FAILED(rv)) {
      // XXX what to do?  Increment FailedDooms counter?
    }
    entry = nullptr;
  }

  if (!entry) {
    if (!(request->AccessRequested() & nsICache::ACCESS_WRITE)) {
      // this is a READ-ONLY request
      rv = NS_ERROR_CACHE_KEY_NOT_FOUND;
      goto error;
    }

    entry = new nsCacheEntry(request->mKey, request->IsStreamBased(),
                             request->StoragePolicy());
    if (!entry) return NS_ERROR_OUT_OF_MEMORY;

    if (request->IsPrivate()) entry->MarkPrivate();

    entry->Fetched();
    ++mTotalEntries;

    // XXX  we could perform an early bind in some cases based on storage policy
  }

  if (!entry->IsActive()) {
    rv = mActiveEntries.AddEntry(entry);
    if (NS_FAILED(rv)) goto error;
    CACHE_LOG_DEBUG(("Added entry %p to mActiveEntries\n", entry));
    entry->MarkActive();  // mark entry active, because it's now in
                          // mActiveEntries
  }
  *result = entry;
  return NS_OK;

error:
  *result = nullptr;
  delete entry;
  return rv;
}

nsCacheEntry* nsCacheService::SearchCacheDevices(nsCString* key,
                                                 nsCacheStoragePolicy policy,
                                                 bool* collision) {
  Telemetry::AutoTimer<Telemetry::CACHE_DEVICE_SEARCH_2> timer;
  nsCacheEntry* entry = nullptr;

  *collision = false;
  if (policy == nsICache::STORE_OFFLINE ||
      (policy == nsICache::STORE_ANYWHERE && gIOService->IsOffline())) {
    if (mEnableOfflineDevice) {
      if (!mOfflineDevice) {
        nsresult rv = CreateOfflineDevice();
        if (NS_FAILED(rv)) return nullptr;
      }

      entry = mOfflineDevice->FindEntry(key, collision);
    }
  }

  return entry;
}

nsCacheDevice* nsCacheService::EnsureEntryHasDevice(nsCacheEntry* entry) {
  nsCacheDevice* device = entry->CacheDevice();
  // return device if found, possibly null if the entry is doomed i.e prevent
  // doomed entries to bind to a device (see e.g. bugs #548406 and #596443)
  if (device || entry->IsDoomed()) return device;

  if (!device && entry->IsStreamData() && entry->IsAllowedOffline() &&
      mEnableOfflineDevice) {
    if (!mOfflineDevice) {
      (void)CreateOfflineDevice();  // ignore the error (check for
                                    // mOfflineDevice instead)
    }

    device = entry->CustomCacheDevice() ? entry->CustomCacheDevice()
                                        : mOfflineDevice;

    if (device) {
      entry->MarkBinding();
      nsresult rv = device->BindEntry(entry);
      entry->ClearBinding();
      if (NS_FAILED(rv)) device = nullptr;
    }
  }

  if (device) entry->SetCacheDevice(device);
  return device;
}

nsresult nsCacheService::DoomEntry(nsCacheEntry* entry) {
  return gService->DoomEntry_Internal(entry, true);
}

nsresult nsCacheService::DoomEntry_Internal(nsCacheEntry* entry,
                                            bool doProcessPendingRequests) {
  if (entry->IsDoomed()) return NS_OK;

  CACHE_LOG_DEBUG(("Dooming entry %p\n", entry));
  nsresult rv = NS_OK;
  entry->MarkDoomed();

  NS_ASSERTION(!entry->IsBinding(), "Dooming entry while binding device.");
  nsCacheDevice* device = entry->CacheDevice();
  if (device) device->DoomEntry(entry);

  if (entry->IsActive()) {
    // remove from active entries
    mActiveEntries.RemoveEntry(entry);
    CACHE_LOG_DEBUG(("Removed entry %p from mActiveEntries\n", entry));
    entry->MarkInactive();
  }

  // put on doom list to wait for descriptors to close
  NS_ASSERTION(PR_CLIST_IS_EMPTY(entry), "doomed entry still on device list");
  PR_APPEND_LINK(entry, &mDoomedEntries);

  // handle pending requests only if we're supposed to
  if (doProcessPendingRequests) {
    // tell pending requests to get on with their lives...
    rv = ProcessPendingRequests(entry);

    // All requests have been removed, but there may still be open descriptors
    if (entry->IsNotInUse()) {
      DeactivateEntry(entry);  // tell device to get rid of it
    }
  }
  return rv;
}

void nsCacheService::OnProfileShutdown() {
  if (!gService || !gService->mInitialized) {
    // The cache service has been shut down, but someone is still holding
    // a reference to it. Ignore this call.
    return;
  }

  {
    nsCacheServiceAutoLock lock(LOCK_TELEM(NSCACHESERVICE_ONPROFILESHUTDOWN));
    gService->mClearingEntries = true;
    gService->DoomActiveEntries(nullptr);
  }

  gService->CloseAllStreams();

  nsCacheServiceAutoLock lock(LOCK_TELEM(NSCACHESERVICE_ONPROFILESHUTDOWN));
  gService->ClearDoomList();

  // Make sure to wait for any pending cache-operations before
  // proceeding with destructive actions (bug #620660)
  (void)SyncWithCacheIOThread();

  if (gService->mOfflineDevice && gService->mEnableOfflineDevice) {
    gService->mOfflineDevice->Shutdown();
  }
  for (auto iter = gService->mCustomOfflineDevices.Iter(); !iter.Done();
       iter.Next()) {
    iter.Data()->Shutdown();
    iter.Remove();
  }

  gService->mEnableOfflineDevice = false;

  gService->mClearingEntries = false;
}

void nsCacheService::OnProfileChanged() {
  if (!gService) return;

  CACHE_LOG_DEBUG(("nsCacheService::OnProfileChanged"));

  nsCacheServiceAutoLock lock(LOCK_TELEM(NSCACHESERVICE_ONPROFILECHANGED));

  gService->mEnableOfflineDevice = gService->mObserver->OfflineCacheEnabled();

  if (gService->mOfflineDevice) {
    gService->mOfflineDevice->SetCacheParentDirectory(
        gService->mObserver->OfflineCacheParentDirectory());
    gService->mOfflineDevice->SetCapacity(
        gService->mObserver->OfflineCacheCapacity());

    // XXX initialization of mOfflineDevice could be made lazily, if
    // mEnableOfflineDevice is false
    nsresult rv =
        gService->mOfflineDevice->InitWithSqlite(gService->mStorageService);
    if (NS_FAILED(rv)) {
      NS_ERROR(
          "nsCacheService::OnProfileChanged: Re-initializing offline device "
          "failed");
      gService->mEnableOfflineDevice = false;
      // XXX delete mOfflineDevice?
    }
  }
}

void nsCacheService::SetOfflineCacheEnabled(bool enabled) {
  if (!gService) return;
  nsCacheServiceAutoLock lock(
      LOCK_TELEM(NSCACHESERVICE_SETOFFLINECACHEENABLED));
  gService->mEnableOfflineDevice = enabled;
}

void nsCacheService::SetOfflineCacheCapacity(int32_t capacity) {
  if (!gService) return;
  nsCacheServiceAutoLock lock(
      LOCK_TELEM(NSCACHESERVICE_SETOFFLINECACHECAPACITY));

  if (gService->mOfflineDevice) {
    gService->mOfflineDevice->SetCapacity(capacity);
  }

  gService->mEnableOfflineDevice = gService->mObserver->OfflineCacheEnabled();
}

/******************************************************************************
 * static methods for nsCacheEntryDescriptor
 *****************************************************************************/
void nsCacheService::CloseDescriptor(nsCacheEntryDescriptor* descriptor) {
  // ask entry to remove descriptor
  nsCacheEntry* entry = descriptor->CacheEntry();
  bool doomEntry;
  bool stillActive = entry->RemoveDescriptor(descriptor, &doomEntry);

  if (!entry->IsValid()) {
    gService->ProcessPendingRequests(entry);
  }

  if (doomEntry) {
    gService->DoomEntry_Internal(entry, true);
    return;
  }

  if (!stillActive) {
    gService->DeactivateEntry(entry);
  }
}

nsresult nsCacheService::GetFileForEntry(nsCacheEntry* entry,
                                         nsIFile** result) {
  nsCacheDevice* device = gService->EnsureEntryHasDevice(entry);
  if (!device) return NS_ERROR_UNEXPECTED;

  return device->GetFileForEntry(entry, result);
}

nsresult nsCacheService::OpenInputStreamForEntry(nsCacheEntry* entry,
                                                 nsCacheAccessMode mode,
                                                 uint32_t offset,
                                                 nsIInputStream** result) {
  nsCacheDevice* device = gService->EnsureEntryHasDevice(entry);
  if (!device) return NS_ERROR_UNEXPECTED;

  return device->OpenInputStreamForEntry(entry, mode, offset, result);
}

nsresult nsCacheService::OpenOutputStreamForEntry(nsCacheEntry* entry,
                                                  nsCacheAccessMode mode,
                                                  uint32_t offset,
                                                  nsIOutputStream** result) {
  nsCacheDevice* device = gService->EnsureEntryHasDevice(entry);
  if (!device) return NS_ERROR_UNEXPECTED;

  return device->OpenOutputStreamForEntry(entry, mode, offset, result);
}

nsresult nsCacheService::OnDataSizeChange(nsCacheEntry* entry,
                                          int32_t deltaSize) {
  nsCacheDevice* device = gService->EnsureEntryHasDevice(entry);
  if (!device) return NS_ERROR_UNEXPECTED;

  return device->OnDataSizeChange(entry, deltaSize);
}

void nsCacheService::LockAcquired() {
  MutexAutoLock lock(mTimeStampLock);
  mLockAcquiredTimeStamp = TimeStamp::Now();
}

void nsCacheService::LockReleased() {
  MutexAutoLock lock(mTimeStampLock);
  mLockAcquiredTimeStamp = TimeStamp();
}

void nsCacheService::Lock() {
  gService->mLock.Lock();
  gService->LockAcquired();
}

void nsCacheService::Lock(mozilla::Telemetry::HistogramID mainThreadLockerID) {
  mozilla::Telemetry::HistogramID lockerID;
  mozilla::Telemetry::HistogramID generalID;

  if (NS_IsMainThread()) {
    lockerID = mainThreadLockerID;
    generalID = mozilla::Telemetry::CACHE_SERVICE_LOCK_WAIT_MAINTHREAD_2;
  } else {
    lockerID = mozilla::Telemetry::HistogramCount;
    generalID = mozilla::Telemetry::CACHE_SERVICE_LOCK_WAIT_2;
  }

  TimeStamp start(TimeStamp::Now());

  nsCacheService::Lock();

  TimeStamp stop(TimeStamp::Now());

  // Telemetry isn't thread safe on its own, but this is OK because we're
  // protecting it with the cache lock.
  if (lockerID != mozilla::Telemetry::HistogramCount) {
    mozilla::Telemetry::AccumulateTimeDelta(lockerID, start, stop);
  }
  mozilla::Telemetry::AccumulateTimeDelta(generalID, start, stop);
}

void nsCacheService::Unlock() {
  gService->mLock.AssertCurrentThreadOwns();

  nsTArray<nsISupports*> doomed;
  doomed.SwapElements(gService->mDoomedObjects);

  gService->LockReleased();
  gService->mLock.Unlock();

  for (uint32_t i = 0; i < doomed.Length(); ++i) doomed[i]->Release();
}

void nsCacheService::ReleaseObject_Locked(nsISupports* obj,
                                          nsIEventTarget* target) {
  gService->mLock.AssertCurrentThreadOwns();

  bool isCur;
  if (!target || (NS_SUCCEEDED(target->IsOnCurrentThread(&isCur)) && isCur)) {
    gService->mDoomedObjects.AppendElement(obj);
  } else {
    NS_ProxyRelease("nsCacheService::ReleaseObject_Locked::obj", target,
                    dont_AddRef(obj));
  }
}

nsresult nsCacheService::SetCacheElement(nsCacheEntry* entry,
                                         nsISupports* element) {
  entry->SetData(element);
  entry->TouchData();
  return NS_OK;
}

nsresult nsCacheService::ValidateEntry(nsCacheEntry* entry) {
  nsCacheDevice* device = gService->EnsureEntryHasDevice(entry);
  if (!device) return NS_ERROR_UNEXPECTED;

  entry->MarkValid();
  nsresult rv = gService->ProcessPendingRequests(entry);
  NS_ASSERTION(rv == NS_OK, "ProcessPendingRequests failed.");
  // XXX what else should be done?

  return rv;
}

int32_t nsCacheService::CacheCompressionLevel() {
  int32_t level = gService->mObserver->CacheCompressionLevel();
  return level;
}

void nsCacheService::DeactivateEntry(nsCacheEntry* entry) {
  CACHE_LOG_DEBUG(("Deactivating entry %p\n", entry));
  nsresult rv = NS_OK;
  NS_ASSERTION(entry->IsNotInUse(), "### deactivating an entry while in use!");
  nsCacheDevice* device = nullptr;

  if (mMaxDataSize < entry->DataSize()) mMaxDataSize = entry->DataSize();
  if (mMaxMetaSize < entry->MetaDataSize())
    mMaxMetaSize = entry->MetaDataSize();

  if (entry->IsDoomed()) {
    // remove from Doomed list
    PR_REMOVE_AND_INIT_LINK(entry);
  } else if (entry->IsActive()) {
    // remove from active entries
    mActiveEntries.RemoveEntry(entry);
    CACHE_LOG_DEBUG(
        ("Removed deactivated entry %p from mActiveEntries\n", entry));
    entry->MarkInactive();

    // bind entry if necessary to store meta-data
    device = EnsureEntryHasDevice(entry);
    if (!device) {
      CACHE_LOG_DEBUG(
          ("DeactivateEntry: unable to bind active "
           "entry %p\n",
           entry));
      NS_WARNING("DeactivateEntry: unable to bind active entry\n");
      return;
    }
  } else {
    // if mInitialized == false,
    // then we're shutting down and this state is okay.
    NS_ASSERTION(!mInitialized, "DeactivateEntry: bad cache entry state.");
  }

  device = entry->CacheDevice();
  if (device) {
    rv = device->DeactivateEntry(entry);
    if (NS_FAILED(rv)) {
      // increment deactivate failure count
      ++mDeactivateFailures;
    }
  } else {
    // increment deactivating unbound entry statistic
    ++mDeactivatedUnboundEntries;
    delete entry;  // because no one else will
  }
}

nsresult nsCacheService::ProcessPendingRequests(nsCacheEntry* entry) {
  nsresult rv = NS_OK;
  nsCacheRequest* request = (nsCacheRequest*)PR_LIST_HEAD(&entry->mRequestQ);
  nsCacheRequest* nextRequest;
  bool newWriter = false;

  CACHE_LOG_DEBUG((
      "ProcessPendingRequests for %sinitialized %s %salid entry %p\n",
      (entry->IsInitialized() ? "" : "Un"), (entry->IsDoomed() ? "DOOMED" : ""),
      (entry->IsValid() ? "V" : "Inv"), entry));

  if (request == &entry->mRequestQ) return NS_OK;  // no queued requests

  if (!entry->IsDoomed() && entry->IsInvalid()) {
    // 1st descriptor closed w/o MarkValid()
    NS_ASSERTION(PR_CLIST_IS_EMPTY(&entry->mDescriptorQ),
                 "shouldn't be here with open descriptors");

#if DEBUG
    // verify no ACCESS_WRITE requests(shouldn't have any of these)
    while (request != &entry->mRequestQ) {
      NS_ASSERTION(request->AccessRequested() != nsICache::ACCESS_WRITE,
                   "ACCESS_WRITE request should have been given a new entry");
      request = (nsCacheRequest*)PR_NEXT_LINK(request);
    }
    request = (nsCacheRequest*)PR_LIST_HEAD(&entry->mRequestQ);
#endif
    // find first request with ACCESS_READ_WRITE (if any) and promote it to 1st
    // writer
    while (request != &entry->mRequestQ) {
      if (request->AccessRequested() == nsICache::ACCESS_READ_WRITE) {
        newWriter = true;
        CACHE_LOG_DEBUG(("  promoting request %p to 1st writer\n", request));
        break;
      }

      request = (nsCacheRequest*)PR_NEXT_LINK(request);
    }

    if (request == &entry->mRequestQ)  // no requests asked for
                                       // ACCESS_READ_WRITE, back to top
      request = (nsCacheRequest*)PR_LIST_HEAD(&entry->mRequestQ);

    // XXX what should we do if there are only READ requests in queue?
    // XXX serialize their accesses, give them only read access, but force them
    // to check validate flag?
    // XXX or do readers simply presume the entry is valid
    // See fix for bug #467392 below
  }

  nsCacheAccessMode accessGranted = nsICache::ACCESS_NONE;

  while (request != &entry->mRequestQ) {
    nextRequest = (nsCacheRequest*)PR_NEXT_LINK(request);
    CACHE_LOG_DEBUG(("  %sync request %p for %p\n",
                     (request->mListener ? "As" : "S"), request, entry));

    if (request->mListener) {
      // Async request
      PR_REMOVE_AND_INIT_LINK(request);

      if (entry->IsDoomed()) {
        rv = ProcessRequest(request, false, nullptr);
        if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION)
          rv = NS_OK;
        else
          delete request;

        if (NS_FAILED(rv)) {
          // XXX what to do?
        }
      } else if (entry->IsValid() || newWriter) {
        rv = entry->RequestAccess(request, &accessGranted);
        NS_ASSERTION(NS_SUCCEEDED(rv),
                     "if entry is valid, RequestAccess must succeed.");
        // XXX if (newWriter) {
        //       NS_ASSERTION( accessGranted ==
        //                     request->AccessRequested(), "why not?");
        //     }

        // entry->CreateDescriptor dequeues request, and queues descriptor
        nsICacheEntryDescriptor* descriptor = nullptr;
        rv = entry->CreateDescriptor(request, accessGranted, &descriptor);

        // post call to listener to report error or descriptor
        rv = NotifyListener(request, descriptor, accessGranted, rv);
        delete request;
        if (NS_FAILED(rv)) {
          // XXX what to do?
        }

      } else {
        // read-only request to an invalid entry - need to wait for
        // the entry to become valid so we post an event to process
        // the request again later (bug #467392)
        nsCOMPtr<nsIRunnable> ev = new nsProcessRequestEvent(request);
        rv = DispatchToCacheIOThread(ev);
        if (NS_FAILED(rv)) {
          delete request;  // avoid leak
        }
      }
    } else {
      // Synchronous request
      request->WakeUp();
    }
    if (newWriter) break;  // process remaining requests after validation
    request = nextRequest;
  }

  return NS_OK;
}

bool nsCacheService::IsDoomListEmpty() {
  nsCacheEntry* entry = (nsCacheEntry*)PR_LIST_HEAD(&mDoomedEntries);
  return &mDoomedEntries == entry;
}

void nsCacheService::ClearDoomList() {
  nsCacheEntry* entry = (nsCacheEntry*)PR_LIST_HEAD(&mDoomedEntries);

  while (entry != &mDoomedEntries) {
    nsCacheEntry* next = (nsCacheEntry*)PR_NEXT_LINK(entry);

    entry->DetachDescriptors();
    DeactivateEntry(entry);
    entry = next;
  }
}

void nsCacheService::DoomActiveEntries(DoomCheckFn check) {
  AutoTArray<nsCacheEntry*, 8> array;

  for (auto iter = mActiveEntries.Iter(); !iter.Done(); iter.Next()) {
    nsCacheEntry* entry =
        static_cast<nsCacheEntryHashTableEntry*>(iter.Get())->cacheEntry;

    if (check && !check(entry)) {
      continue;
    }

    array.AppendElement(entry);

    // entry is being removed from the active entry list
    entry->MarkInactive();
    iter.Remove();
  }

  uint32_t count = array.Length();
  for (uint32_t i = 0; i < count; ++i) {
    DoomEntry_Internal(array[i], true);
  }
}

void nsCacheService::CloseAllStreams() {
  nsTArray<RefPtr<nsCacheEntryDescriptor::nsInputStreamWrapper> > inputs;
  nsTArray<RefPtr<nsCacheEntryDescriptor::nsOutputStreamWrapper> > outputs;

  {
    nsCacheServiceAutoLock lock(LOCK_TELEM(NSCACHESERVICE_CLOSEALLSTREAMS));

    nsTArray<nsCacheEntry*> entries;

#if DEBUG
    // make sure there is no active entry
    for (auto iter = mActiveEntries.Iter(); !iter.Done(); iter.Next()) {
      auto entry = static_cast<nsCacheEntryHashTableEntry*>(iter.Get());
      entries.AppendElement(entry->cacheEntry);
    }
    NS_ASSERTION(entries.IsEmpty(), "Bad state");
#endif

    // Get doomed entries
    nsCacheEntry* entry = (nsCacheEntry*)PR_LIST_HEAD(&mDoomedEntries);
    while (entry != &mDoomedEntries) {
      nsCacheEntry* next = (nsCacheEntry*)PR_NEXT_LINK(entry);
      entries.AppendElement(entry);
      entry = next;
    }

    // Iterate through all entries and collect input and output streams
    for (size_t i = 0; i < entries.Length(); i++) {
      entry = entries.ElementAt(i);

      nsTArray<RefPtr<nsCacheEntryDescriptor> > descs;
      entry->GetDescriptors(descs);

      for (uint32_t j = 0; j < descs.Length(); j++) {
        if (descs[j]->mOutputWrapper)
          outputs.AppendElement(descs[j]->mOutputWrapper);

        for (size_t k = 0; k < descs[j]->mInputWrappers.Length(); k++)
          inputs.AppendElement(descs[j]->mInputWrappers[k]);
      }
    }
  }

  uint32_t i;
  for (i = 0; i < inputs.Length(); i++) inputs[i]->Close();

  for (i = 0; i < outputs.Length(); i++) outputs[i]->Close();
}

bool nsCacheService::GetClearingEntries() {
  AssertOwnsLock();
  return gService->mClearingEntries;
}

// static
void nsCacheService::GetCacheBaseDirectoty(nsIFile** result) {
  *result = nullptr;
  if (!gService || !gService->mObserver) return;

  nsCOMPtr<nsIFile> directory = gService->mObserver->DiskCacheParentDirectory();
  if (!directory) return;

  directory->Clone(result);
}

// static
void nsCacheService::GetDiskCacheDirectory(nsIFile** result) {
  nsCOMPtr<nsIFile> directory;
  GetCacheBaseDirectoty(getter_AddRefs(directory));
  if (!directory) return;

  nsresult rv = directory->AppendNative(NS_LITERAL_CSTRING("Cache"));
  if (NS_FAILED(rv)) return;

  directory.forget(result);
}

// static
void nsCacheService::GetAppCacheDirectory(nsIFile** result) {
  nsCOMPtr<nsIFile> directory;
  GetCacheBaseDirectoty(getter_AddRefs(directory));
  if (!directory) return;

  nsresult rv = directory->AppendNative(NS_LITERAL_CSTRING("OfflineCache"));
  if (NS_FAILED(rv)) return;

  directory.forget(result);
}

void nsCacheService::LogCacheStatistics() {
  uint32_t hitPercentage = 0;
  double sum = (double)(mCacheHits + mCacheMisses);
  if (sum != 0) {
    hitPercentage = (uint32_t)((((double)mCacheHits) / sum) * 100);
  }
  CACHE_LOG_INFO(("\nCache Service Statistics:\n\n"));
  CACHE_LOG_INFO(("    TotalEntries   = %d\n", mTotalEntries));
  CACHE_LOG_INFO(("    Cache Hits     = %d\n", mCacheHits));
  CACHE_LOG_INFO(("    Cache Misses   = %d\n", mCacheMisses));
  CACHE_LOG_INFO(("    Cache Hit %%    = %d%%\n", hitPercentage));
  CACHE_LOG_INFO(("    Max Key Length = %d\n", mMaxKeyLength));
  CACHE_LOG_INFO(("    Max Meta Size  = %d\n", mMaxMetaSize));
  CACHE_LOG_INFO(("    Max Data Size  = %d\n", mMaxDataSize));
  CACHE_LOG_INFO(("\n"));
  CACHE_LOG_INFO(
      ("    Deactivate Failures         = %d\n", mDeactivateFailures));
  CACHE_LOG_INFO(
      ("    Deactivated Unbound Entries = %d\n", mDeactivatedUnboundEntries));
}

void nsCacheService::MoveOrRemoveDiskCache(nsIFile* aOldCacheDir,
                                           nsIFile* aNewCacheDir,
                                           const char* aCacheSubdir) {
  bool same;
  if (NS_FAILED(aOldCacheDir->Equals(aNewCacheDir, &same)) || same) return;

  nsCOMPtr<nsIFile> aOldCacheSubdir;
  aOldCacheDir->Clone(getter_AddRefs(aOldCacheSubdir));

  nsresult rv = aOldCacheSubdir->AppendNative(nsDependentCString(aCacheSubdir));
  if (NS_FAILED(rv)) return;

  bool exists;
  if (NS_FAILED(aOldCacheSubdir->Exists(&exists)) || !exists) return;

  nsCOMPtr<nsIFile> aNewCacheSubdir;
  aNewCacheDir->Clone(getter_AddRefs(aNewCacheSubdir));

  rv = aNewCacheSubdir->AppendNative(nsDependentCString(aCacheSubdir));
  if (NS_FAILED(rv)) return;

  PathString newPath = aNewCacheSubdir->NativePath();

  if (NS_SUCCEEDED(aNewCacheSubdir->Exists(&exists)) && !exists) {
    // New cache directory does not exist, try to move the old one here
    // rename needs an empty target directory

    // Make sure the parent of the target sub-dir exists
    rv = aNewCacheDir->Create(nsIFile::DIRECTORY_TYPE, 0777);
    if (NS_SUCCEEDED(rv) || NS_ERROR_FILE_ALREADY_EXISTS == rv) {
      PathString oldPath = aOldCacheSubdir->NativePath();
#ifdef XP_WIN
      if (MoveFileW(oldPath.get(), newPath.get()))
#else
      if (rename(oldPath.get(), newPath.get()) == 0)
#endif
      {
        return;
      }
    }
  }

  // Delay delete by 1 minute to avoid IO thrash on startup.
  nsDeleteDir::DeleteDir(aOldCacheSubdir, false, 60000);
}

static bool IsEntryPrivate(nsCacheEntry* entry) { return entry->IsPrivate(); }

void nsCacheService::LeavePrivateBrowsing() {
  nsCacheServiceAutoLock lock;

  gService->DoomActiveEntries(IsEntryPrivate);
}
