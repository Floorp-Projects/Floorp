/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheStorageService.h"
#include "CacheFileIOManager.h"
#include "CacheObserver.h"
#include "CacheIndex.h"
#include "CacheIndexIterator.h"
#include "CacheStorage.h"
#include "AppCacheStorage.h"
#include "CacheEntry.h"
#include "CacheFileUtils.h"

#include "OldWrappers.h"
#include "nsCacheService.h"
#include "nsDeleteDir.h"

#include "nsICacheStorageVisitor.h"
#include "nsIObserverService.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsAutoPtr.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsWeakReference.h"
#include "nsXULAppAPI.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Services.h"
#include "mozilla/IntegerPrintfMacros.h"

namespace mozilla {
namespace net {

namespace {

void AppendMemoryStorageID(nsAutoCString &key)
{
  key.Append('/');
  key.Append('M');
}

} // namespace

// Not defining as static or class member of CacheStorageService since
// it would otherwise need to include CacheEntry.h and that then would
// need to be exported to make nsNetModule.cpp compilable.
typedef nsClassHashtable<nsCStringHashKey, CacheEntryTable>
        GlobalEntryTables;

/**
 * Keeps tables of entries.  There is one entries table for each distinct load
 * context type.  The distinction is based on following load context info states:
 * <isPrivate|isAnon|appId|inIsolatedMozBrowser> which builds a mapping key.
 *
 * Thread-safe to access, protected by the service mutex.
 */
static GlobalEntryTables* sGlobalEntryTables;

CacheMemoryConsumer::CacheMemoryConsumer(uint32_t aFlags)
: mReportedMemoryConsumption(0)
, mFlags(aFlags)
{
}

void
CacheMemoryConsumer::DoMemoryReport(uint32_t aCurrentSize)
{
  if (!(mFlags & DONT_REPORT) && CacheStorageService::Self()) {
    CacheStorageService::Self()->OnMemoryConsumptionChange(this, aCurrentSize);
  }
}

CacheStorageService::MemoryPool::MemoryPool(EType aType)
: mType(aType)
, mMemorySize(0)
{
}

CacheStorageService::MemoryPool::~MemoryPool()
{
  if (mMemorySize != 0) {
    NS_ERROR("Network cache reported memory consumption is not at 0, probably leaking?");
  }
}

uint32_t
CacheStorageService::MemoryPool::Limit() const
{
  switch (mType) {
  case DISK:
    return CacheObserver::MetadataMemoryLimit();
  case MEMORY:
    return CacheObserver::MemoryCacheCapacity();
  }

  MOZ_CRASH("Bad pool type");
  return 0;
}

NS_IMPL_ISUPPORTS(CacheStorageService,
                  nsICacheStorageService,
                  nsIMemoryReporter,
                  nsITimerCallback,
                  nsICacheTesting,
                  nsINamed)

CacheStorageService* CacheStorageService::sSelf = nullptr;

CacheStorageService::CacheStorageService()
: mLock("CacheStorageService.mLock")
, mForcedValidEntriesLock("CacheStorageService.mForcedValidEntriesLock")
, mShutdown(false)
, mDiskPool(MemoryPool::DISK)
, mMemoryPool(MemoryPool::MEMORY)
{
  CacheFileIOManager::Init();

  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!sSelf);

  sSelf = this;
  sGlobalEntryTables = new GlobalEntryTables();

  RegisterStrongMemoryReporter(this);
}

CacheStorageService::~CacheStorageService()
{
  LOG(("CacheStorageService::~CacheStorageService"));
  sSelf = nullptr;
}

void CacheStorageService::Shutdown()
{
  mozilla::MutexAutoLock lock(mLock);

  if (mShutdown)
    return;

  LOG(("CacheStorageService::Shutdown - start"));

  mShutdown = true;

  nsCOMPtr<nsIRunnable> event =
    NewRunnableMethod("net::CacheStorageService::ShutdownBackground",
                      this,
                      &CacheStorageService::ShutdownBackground);
  Dispatch(event);

#ifdef NS_FREE_PERMANENT_DATA
  sGlobalEntryTables->Clear();
  delete sGlobalEntryTables;
#endif
  sGlobalEntryTables = nullptr;

  LOG(("CacheStorageService::Shutdown - done"));
}

void CacheStorageService::ShutdownBackground()
{
  LOG(("CacheStorageService::ShutdownBackground - start"));

  MOZ_ASSERT(IsOnManagementThread());

  {
    mozilla::MutexAutoLock lock(mLock);

    // Cancel purge timer to avoid leaking.
    if (mPurgeTimer) {
      LOG(("  freeing the timer"));
      mPurgeTimer->Cancel();
    }
  }

#ifdef NS_FREE_PERMANENT_DATA
  Pool(false).mFrecencyArray.Clear();
  Pool(false).mExpirationArray.Clear();
  Pool(true).mFrecencyArray.Clear();
  Pool(true).mExpirationArray.Clear();
#endif

  LOG(("CacheStorageService::ShutdownBackground - done"));
}

// Internal management methods

namespace {

// WalkCacheRunnable
// Base class for particular storage entries visiting
class WalkCacheRunnable : public Runnable
                        , public CacheStorageService::EntryInfoCallback
{
protected:
  WalkCacheRunnable(nsICacheStorageVisitor* aVisitor, bool aVisitEntries)
    : Runnable("net::WalkCacheRunnable")
    , mService(CacheStorageService::Self())
    , mCallback(aVisitor)
    , mSize(0)
    , mNotifyStorage(true)
    , mVisitEntries(aVisitEntries)
    , mCancel(false)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual ~WalkCacheRunnable()
  {
    if (mCallback) {
      ProxyReleaseMainThread(
        "WalkCacheRunnable::mCallback", mCallback);
    }
  }

  RefPtr<CacheStorageService> mService;
  nsCOMPtr<nsICacheStorageVisitor> mCallback;

  uint64_t mSize;

  bool mNotifyStorage : 1;
  bool mVisitEntries : 1;

  Atomic<bool> mCancel;
};

// WalkMemoryCacheRunnable
// Responsible to visit memory storage and walk
// all entries on it asynchronously.
class WalkMemoryCacheRunnable : public WalkCacheRunnable
{
public:
  WalkMemoryCacheRunnable(nsILoadContextInfo *aLoadInfo,
                          bool aVisitEntries,
                          nsICacheStorageVisitor* aVisitor)
    : WalkCacheRunnable(aVisitor, aVisitEntries)
  {
    CacheFileUtils::AppendKeyPrefix(aLoadInfo, mContextKey);
    MOZ_ASSERT(NS_IsMainThread());
  }

  nsresult Walk()
  {
    return mService->Dispatch(this);
  }

private:
  NS_IMETHOD Run() override
  {
    if (CacheStorageService::IsOnManagementThread()) {
      LOG(("WalkMemoryCacheRunnable::Run - collecting [this=%p]", this));
      // First, walk, count and grab all entries from the storage

      mozilla::MutexAutoLock lock(CacheStorageService::Self()->Lock());

      if (!CacheStorageService::IsRunning())
        return NS_ERROR_NOT_INITIALIZED;

      CacheEntryTable* entries;
      if (sGlobalEntryTables->Get(mContextKey, &entries)) {
        for (auto iter = entries->Iter(); !iter.Done(); iter.Next()) {
          CacheEntry* entry = iter.UserData();

          // Ignore disk entries
          if (entry->IsUsingDisk()) {
            continue;
          }

          mSize += entry->GetMetadataMemoryConsumption();

          int64_t size;
          if (NS_SUCCEEDED(entry->GetDataSize(&size))) {
            mSize += size;
          }
          mEntryArray.AppendElement(entry);
        }
      }

      // Next, we dispatch to the main thread
    } else if (NS_IsMainThread()) {
      LOG(("WalkMemoryCacheRunnable::Run - notifying [this=%p]", this));

      if (mNotifyStorage) {
        LOG(("  storage"));

        // Second, notify overall storage info
        mCallback->OnCacheStorageInfo(mEntryArray.Length(), mSize,
                                      CacheObserver::MemoryCacheCapacity(), nullptr);
        if (!mVisitEntries)
          return NS_OK; // done

        mNotifyStorage = false;

      } else {
        LOG(("  entry [left=%zu, canceled=%d]", mEntryArray.Length(), (bool)mCancel));

        // Third, notify each entry until depleted or canceled
        if (!mEntryArray.Length() || mCancel) {
          mCallback->OnCacheEntryVisitCompleted();
          return NS_OK; // done
        }

        // Grab the next entry
        RefPtr<CacheEntry> entry = mEntryArray[0];
        mEntryArray.RemoveElementAt(0);

        // Invokes this->OnEntryInfo, that calls the callback with all
        // information of the entry.
        CacheStorageService::GetCacheEntryInfo(entry, this);
      }
    } else {
      MOZ_CRASH("Bad thread");
      return NS_ERROR_FAILURE;
    }

    NS_DispatchToMainThread(this);
    return NS_OK;
  }

  virtual ~WalkMemoryCacheRunnable()
  {
    if (mCallback)
      ProxyReleaseMainThread(
        "WalkMemoryCacheRunnable::mCallback", mCallback);
  }

  virtual void OnEntryInfo(const nsACString & aURISpec, const nsACString & aIdEnhance,
                           int64_t aDataSize, int32_t aFetchCount,
                           uint32_t aLastModifiedTime, uint32_t aExpirationTime,
                           bool aPinned, nsILoadContextInfo* aInfo) override
  {
    nsresult rv;

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), aURISpec);
    if (NS_FAILED(rv)) {
      return;
    }

    rv = mCallback->OnCacheEntryInfo(uri, aIdEnhance, aDataSize, aFetchCount,
                                     aLastModifiedTime, aExpirationTime,
                                     aPinned, aInfo);
    if (NS_FAILED(rv)) {
      LOG(("  callback failed, canceling the walk"));
      mCancel = true;
    }
  }

private:
  nsCString mContextKey;
  nsTArray<RefPtr<CacheEntry> > mEntryArray;
};

// WalkDiskCacheRunnable
// Using the cache index information to get the list of files per context.
class WalkDiskCacheRunnable : public WalkCacheRunnable
{
public:
  WalkDiskCacheRunnable(nsILoadContextInfo *aLoadInfo,
                        bool aVisitEntries,
                        nsICacheStorageVisitor* aVisitor)
    : WalkCacheRunnable(aVisitor, aVisitEntries)
    , mLoadInfo(aLoadInfo)
    , mPass(COLLECT_STATS)
    , mCount(0)
  {
  }

  nsresult Walk()
  {
    // TODO, bug 998693
    // Initial index build should be forced here so that about:cache soon
    // after startup gives some meaningfull results.

    // Dispatch to the INDEX level in hope that very recent cache entries
    // information gets to the index list before we grab the index iterator
    // for the first time.  This tries to avoid miss of entries that has
    // been created right before the visit is required.
    RefPtr<CacheIOThread> thread = CacheFileIOManager::IOThread();
    NS_ENSURE_TRUE(thread, NS_ERROR_NOT_INITIALIZED);

    return thread->Dispatch(this, CacheIOThread::INDEX);
  }

private:
  // Invokes OnCacheEntryInfo callback for each single found entry.
  // There is one instance of this class per one entry.
  class OnCacheEntryInfoRunnable : public Runnable
  {
  public:
    explicit OnCacheEntryInfoRunnable(WalkDiskCacheRunnable* aWalker)
      : Runnable("net::WalkDiskCacheRunnable::OnCacheEntryInfoRunnable")
      , mWalker(aWalker)
      , mDataSize(0)
      , mFetchCount(0)
      , mLastModifiedTime(0)
      , mExpirationTime(0)
      , mPinned(false)
    {
    }

    NS_IMETHOD Run() override
    {
      MOZ_ASSERT(NS_IsMainThread());

      nsresult rv;

      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), mURISpec);
      if (NS_FAILED(rv)) {
        return NS_OK;
      }

      rv = mWalker->mCallback->OnCacheEntryInfo(
        uri, mIdEnhance, mDataSize, mFetchCount,
        mLastModifiedTime, mExpirationTime, mPinned, mInfo);
      if (NS_FAILED(rv)) {
        mWalker->mCancel = true;
      }

      return NS_OK;
    }

    RefPtr<WalkDiskCacheRunnable> mWalker;

    nsCString mURISpec;
    nsCString mIdEnhance;
    int64_t mDataSize;
    int32_t mFetchCount;
    uint32_t mLastModifiedTime;
    uint32_t mExpirationTime;
    bool mPinned;
    nsCOMPtr<nsILoadContextInfo> mInfo;
  };

  NS_IMETHOD Run() override
  {
    // The main loop
    nsresult rv;

    if (CacheStorageService::IsOnManagementThread()) {
      switch (mPass) {
      case COLLECT_STATS:
        // Get quickly the cache stats.
        uint32_t size;
        rv = CacheIndex::GetCacheStats(mLoadInfo, &size, &mCount);
        if (NS_FAILED(rv)) {
          if (mVisitEntries) {
            // both onStorageInfo and onCompleted are expected
            NS_DispatchToMainThread(this);
          }
          return NS_DispatchToMainThread(this);
        }

        mSize = static_cast<uint64_t>(size) << 10;

        // Invoke onCacheStorageInfo with valid information.
        NS_DispatchToMainThread(this);

        if (!mVisitEntries) {
          return NS_OK; // done
        }

        mPass = ITERATE_METADATA;
        MOZ_FALLTHROUGH;

      case ITERATE_METADATA:
        // Now grab the context iterator.
        if (!mIter) {
          rv = CacheIndex::GetIterator(mLoadInfo, true, getter_AddRefs(mIter));
          if (NS_FAILED(rv)) {
            // Invoke onCacheEntryVisitCompleted now
            return NS_DispatchToMainThread(this);
          }
        }

        while (!mCancel && !CacheObserver::ShuttingDown()) {
          if (CacheIOThread::YieldAndRerun())
            return NS_OK;

          SHA1Sum::Hash hash;
          rv = mIter->GetNextHash(&hash);
          if (NS_FAILED(rv))
            break; // done (or error?)

          // This synchronously invokes OnEntryInfo on this class where we
          // redispatch to the main thread for the consumer callback.
          CacheFileIOManager::GetEntryInfo(&hash, this);
        }

        // Invoke onCacheEntryVisitCompleted on the main thread
        NS_DispatchToMainThread(this);
      }
    } else if (NS_IsMainThread()) {
      if (mNotifyStorage) {
        nsCOMPtr<nsIFile> dir;
        CacheFileIOManager::GetCacheDirectory(getter_AddRefs(dir));
        mCallback->OnCacheStorageInfo(mCount, mSize, CacheObserver::DiskCacheCapacity(), dir);
        mNotifyStorage = false;
      } else {
        mCallback->OnCacheEntryVisitCompleted();
      }
    } else {
      MOZ_CRASH("Bad thread");
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  virtual void OnEntryInfo(const nsACString & aURISpec, const nsACString & aIdEnhance,
                           int64_t aDataSize, int32_t aFetchCount,
                           uint32_t aLastModifiedTime, uint32_t aExpirationTime,
                           bool aPinned, nsILoadContextInfo* aInfo) override
  {
    // Called directly from CacheFileIOManager::GetEntryInfo.

    // Invoke onCacheEntryInfo on the main thread for this entry.
    RefPtr<OnCacheEntryInfoRunnable> info = new OnCacheEntryInfoRunnable(this);
    info->mURISpec = aURISpec;
    info->mIdEnhance = aIdEnhance;
    info->mDataSize = aDataSize;
    info->mFetchCount = aFetchCount;
    info->mLastModifiedTime = aLastModifiedTime;
    info->mExpirationTime = aExpirationTime;
    info->mPinned = aPinned;
    info->mInfo = aInfo;

    NS_DispatchToMainThread(info);
  }

  RefPtr<nsILoadContextInfo> mLoadInfo;
  enum {
    // First, we collect stats for the load context.
    COLLECT_STATS,

    // Second, if demanded, we iterate over the entries gethered
    // from the iterator and call CacheFileIOManager::GetEntryInfo
    // for each found entry.
    ITERATE_METADATA,
  } mPass;

  RefPtr<CacheIndexIterator> mIter;
  uint32_t mCount;
};

} // namespace

void CacheStorageService::DropPrivateBrowsingEntries()
{
  mozilla::MutexAutoLock lock(mLock);

  if (mShutdown)
    return;

  nsTArray<nsCString> keys;
  for (auto iter = sGlobalEntryTables->Iter(); !iter.Done(); iter.Next()) {
    const nsACString& key = iter.Key();
    nsCOMPtr<nsILoadContextInfo> info = CacheFileUtils::ParseKey(key);
    if (info && info->IsPrivate()) {
      keys.AppendElement(key);
    }
  }

  for (uint32_t i = 0; i < keys.Length(); ++i) {
    DoomStorageEntries(keys[i], nullptr, true, false, nullptr);
  }
}

namespace {

class CleaupCacheDirectoriesRunnable : public Runnable
{
public:
  NS_DECL_NSIRUNNABLE
  static bool Post();

private:
  CleaupCacheDirectoriesRunnable()
    : Runnable("net::CleaupCacheDirectoriesRunnable")
  {
    nsCacheService::GetDiskCacheDirectory(getter_AddRefs(mCache1Dir));
    CacheFileIOManager::GetCacheDirectory(getter_AddRefs(mCache2Dir));
#if defined(MOZ_WIDGET_ANDROID)
    CacheFileIOManager::GetProfilelessCacheDirectory(getter_AddRefs(mCache2Profileless));
#endif
  }

  virtual ~CleaupCacheDirectoriesRunnable() = default;
  nsCOMPtr<nsIFile> mCache1Dir, mCache2Dir;
#if defined(MOZ_WIDGET_ANDROID)
  nsCOMPtr<nsIFile> mCache2Profileless;
#endif
};

// static
bool CleaupCacheDirectoriesRunnable::Post()
{
  // To obtain the cache1 directory we must unfortunately instantiate the old cache
  // service despite it may not be used at all...  This also initializes nsDeleteDir.
  nsCOMPtr<nsICacheService> service = do_GetService(NS_CACHESERVICE_CONTRACTID);
  if (!service)
    return false;

  nsCOMPtr<nsIEventTarget> thread;
  service->GetCacheIOTarget(getter_AddRefs(thread));
  if (!thread)
    return false;

  RefPtr<CleaupCacheDirectoriesRunnable> r = new CleaupCacheDirectoriesRunnable();
  thread->Dispatch(r, NS_DISPATCH_NORMAL);
  return true;
}

NS_IMETHODIMP CleaupCacheDirectoriesRunnable::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (mCache1Dir) {
    nsDeleteDir::RemoveOldTrashes(mCache1Dir);
  }
  if (mCache2Dir) {
    nsDeleteDir::RemoveOldTrashes(mCache2Dir);
  }
#if defined(MOZ_WIDGET_ANDROID)
  if (mCache2Profileless) {
    nsDeleteDir::RemoveOldTrashes(mCache2Profileless);
    // Always delete the profileless cache on Android
    nsDeleteDir::DeleteDir(mCache2Profileless, true, 30000);
  }
#endif

  if (mCache1Dir) {
    nsDeleteDir::DeleteDir(mCache1Dir, true, 30000);
  }

  return NS_OK;
}

} // namespace

// static
void CacheStorageService::CleaupCacheDirectories()
{
  // Make sure we schedule just once in case CleaupCacheDirectories gets called
  // multiple times from some reason.
  static bool runOnce = CleaupCacheDirectoriesRunnable::Post();
  if (!runOnce) {
    NS_WARNING("Could not start cache trashes cleanup");
  }
}

// Helper methods

// static
bool CacheStorageService::IsOnManagementThread()
{
  RefPtr<CacheStorageService> service = Self();
  if (!service)
    return false;

  nsCOMPtr<nsIEventTarget> target = service->Thread();
  if (!target)
    return false;

  bool currentThread;
  nsresult rv = target->IsOnCurrentThread(&currentThread);
  return NS_SUCCEEDED(rv) && currentThread;
}

already_AddRefed<nsIEventTarget> CacheStorageService::Thread() const
{
  return CacheFileIOManager::IOTarget();
}

nsresult CacheStorageService::Dispatch(nsIRunnable* aEvent)
{
  RefPtr<CacheIOThread> cacheIOThread = CacheFileIOManager::IOThread();
  if (!cacheIOThread)
    return NS_ERROR_NOT_AVAILABLE;

  return cacheIOThread->Dispatch(aEvent, CacheIOThread::MANAGEMENT);
}

// nsICacheStorageService

NS_IMETHODIMP CacheStorageService::MemoryCacheStorage(nsILoadContextInfo *aLoadContextInfo,
                                                      nsICacheStorage * *_retval)
{
  NS_ENSURE_ARG(aLoadContextInfo);
  NS_ENSURE_ARG(_retval);

  nsCOMPtr<nsICacheStorage> storage = new CacheStorage(
    aLoadContextInfo, false, false, false, false);
  storage.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::DiskCacheStorage(nsILoadContextInfo *aLoadContextInfo,
                                                    bool aLookupAppCache,
                                                    nsICacheStorage * *_retval)
{
  NS_ENSURE_ARG(aLoadContextInfo);
  NS_ENSURE_ARG(_retval);

  // TODO save some heap granularity - cache commonly used storages.

  // When disk cache is disabled, still provide a storage, but just keep stuff
  // in memory.
  bool useDisk = CacheObserver::UseDiskCache();

  nsCOMPtr<nsICacheStorage> storage = new CacheStorage(
    aLoadContextInfo, useDisk, aLookupAppCache, false /* size limit */, false /* don't pin */);
  storage.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::PinningCacheStorage(nsILoadContextInfo *aLoadContextInfo,
                                                       nsICacheStorage * *_retval)
{
  NS_ENSURE_ARG(aLoadContextInfo);
  NS_ENSURE_ARG(_retval);

  // When disk cache is disabled don't pretend we cache.
  if (!CacheObserver::UseDiskCache()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsICacheStorage> storage = new CacheStorage(
    aLoadContextInfo, true /* use disk */, false /* no appcache */, true /* ignore size checks */, true /* pin */);
  storage.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::AppCacheStorage(nsILoadContextInfo *aLoadContextInfo,
                                                   nsIApplicationCache *aApplicationCache,
                                                   nsICacheStorage * *_retval)
{
  NS_ENSURE_ARG(aLoadContextInfo);
  NS_ENSURE_ARG(_retval);

  nsCOMPtr<nsICacheStorage> storage;
  // Using classification since cl believes we want to instantiate this method
  // having the same name as the desired class...
  storage = new mozilla::net::AppCacheStorage(aLoadContextInfo, aApplicationCache);

  storage.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::SynthesizedCacheStorage(nsILoadContextInfo *aLoadContextInfo,
                                                           nsICacheStorage * *_retval)
{
  NS_ENSURE_ARG(aLoadContextInfo);
  NS_ENSURE_ARG(_retval);

  nsCOMPtr<nsICacheStorage> storage = new CacheStorage(
    aLoadContextInfo, false, false, true /* skip size checks for synthesized cache */, false /* no pinning */);
  storage.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::Clear()
{
  nsresult rv;

  // Tell the index to block notification to AsyncGetDiskConsumption.
  // Will be allowed again from CacheFileContextEvictor::EvictEntries()
  // when all the context have been removed from disk.
  CacheIndex::OnAsyncEviction(true);

  mozilla::MutexAutoLock lock(mLock);

  {
    mozilla::MutexAutoLock forcedValidEntriesLock(mForcedValidEntriesLock);
    mForcedValidEntries.Clear();
  }

  NS_ENSURE_TRUE(!mShutdown, NS_ERROR_NOT_INITIALIZED);

  nsTArray<nsCString> keys;
  for (auto iter = sGlobalEntryTables->Iter(); !iter.Done(); iter.Next()) {
    keys.AppendElement(iter.Key());
  }

  for (uint32_t i = 0; i < keys.Length(); ++i) {
    DoomStorageEntries(keys[i], nullptr, true, false, nullptr);
  }

  // Passing null as a load info means to evict all contexts.
  // EvictByContext() respects the entry pinning.  EvictAll() does not.
  rv = CacheFileIOManager::EvictByContext(nullptr, false, EmptyString());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::ClearOrigin(nsIPrincipal* aPrincipal)
{
  nsresult rv;

  if (NS_WARN_IF(!aPrincipal)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString origin;
  rv = nsContentUtils::GetUTFOrigin(aPrincipal, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ClearOriginInternal(origin, aPrincipal->OriginAttributesRef(), true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ClearOriginInternal(origin, aPrincipal->OriginAttributesRef(), false);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static bool
RemoveExactEntry(CacheEntryTable* aEntries,
                 nsACString const& aKey,
                 CacheEntry* aEntry,
                 bool aOverwrite)
{
  RefPtr<CacheEntry> existingEntry;
  if (!aEntries->Get(aKey, getter_AddRefs(existingEntry))) {
    LOG(("RemoveExactEntry [entry=%p already gone]", aEntry));
    return false; // Already removed...
  }

  if (!aOverwrite && existingEntry != aEntry) {
    LOG(("RemoveExactEntry [entry=%p already replaced]", aEntry));
    return false; // Already replaced...
  }

  LOG(("RemoveExactEntry [entry=%p removed]", aEntry));
  aEntries->Remove(aKey);
  return true;
}

nsresult
CacheStorageService::ClearOriginInternal(const nsAString& aOrigin,
                                         const OriginAttributes& aOriginAttributes,
                                         bool aAnonymous)
{
  nsresult rv;

  RefPtr<LoadContextInfo> info =
    GetLoadContextInfo(aAnonymous, aOriginAttributes);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_FAILURE;
  }

  mozilla::MutexAutoLock lock(mLock);

  if (sGlobalEntryTables) {
    for (auto iter = sGlobalEntryTables->Iter(); !iter.Done(); iter.Next()) {
      bool matches = false;
      rv = CacheFileUtils::KeyMatchesLoadContextInfo(iter.Key(), info,
                                                     &matches);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!matches) {
        continue;
      }

      CacheEntryTable* table = iter.UserData();
      MOZ_ASSERT(table);

      nsTArray<RefPtr<CacheEntry>> entriesToDelete;

      for (auto entryIter = table->Iter(); !entryIter.Done(); entryIter.Next()) {
        CacheEntry* entry = entryIter.UserData();

        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), entry->GetURI());
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString origin;
        rv = nsContentUtils::GetUTFOrigin(uri, origin);
        NS_ENSURE_SUCCESS(rv, rv);

        if (origin != aOrigin) {
          continue;
        }

        entriesToDelete.AppendElement(entry);
      }

      for (RefPtr<CacheEntry>& entry : entriesToDelete) {
        nsAutoCString entryKey;
        rv = entry->HashingKey(entryKey);
        if (NS_FAILED(rv)) {
          NS_ERROR("aEntry->HashingKey() failed?");
          return rv;
        }

        RemoveExactEntry(table, entryKey, entry, false /* don't overwrite */);
      }
    }
  }

  rv = CacheFileIOManager::EvictByContext(info, false /* pinned */, aOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::PurgeFromMemory(uint32_t aWhat)
{
  uint32_t what;

  switch (aWhat) {
  case PURGE_DISK_DATA_ONLY:
    what = CacheEntry::PURGE_DATA_ONLY_DISK_BACKED;
    break;

  case PURGE_DISK_ALL:
    what = CacheEntry::PURGE_WHOLE_ONLY_DISK_BACKED;
    break;

  case PURGE_EVERYTHING:
    what = CacheEntry::PURGE_WHOLE;
    break;

  default:
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIRunnable> event =
    new PurgeFromMemoryRunnable(this, what);

  return Dispatch(event);
}

NS_IMETHODIMP CacheStorageService::PurgeFromMemoryRunnable::Run()
{
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(nullptr, "cacheservice:purge-memory-pools", nullptr);
    }

    return NS_OK;
  }

  if (mService) {
    // TODO not all flags apply to both pools
    mService->Pool(true).PurgeAll(mWhat);
    mService->Pool(false).PurgeAll(mWhat);
    mService = nullptr;
  }

  NS_DispatchToMainThread(this);
  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::AsyncGetDiskConsumption(
  nsICacheStorageConsumptionObserver* aObserver)
{
  NS_ENSURE_ARG(aObserver);

  nsresult rv;

  rv = CacheIndex::AsyncGetDiskConsumption(aObserver);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::GetIoTarget(nsIEventTarget** aEventTarget)
{
  NS_ENSURE_ARG(aEventTarget);

  nsCOMPtr<nsIEventTarget> ioTarget = CacheFileIOManager::IOTarget();
  ioTarget.forget(aEventTarget);

  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::AsyncVisitAllStorages(
  nsICacheStorageVisitor* aVisitor,
  bool aVisitEntries)
{
  LOG(("CacheStorageService::AsyncVisitAllStorages [cb=%p]", aVisitor));
  NS_ENSURE_FALSE(mShutdown, NS_ERROR_NOT_INITIALIZED);

  // Walking the disk cache also walks the memory cache.
  RefPtr<WalkDiskCacheRunnable> event =
    new WalkDiskCacheRunnable(nullptr, aVisitEntries, aVisitor);
  return event->Walk();

  return NS_OK;
}

// Methods used by CacheEntry for management of in-memory structures.

namespace {

class FrecencyComparator
{
public:
  bool Equals(CacheEntry* a, CacheEntry* b) const {
    return a->GetFrecency() == b->GetFrecency();
  }
  bool LessThan(CacheEntry* a, CacheEntry* b) const {
    return a->GetFrecency() < b->GetFrecency();
  }
};

class ExpirationComparator
{
public:
  bool Equals(CacheEntry* a, CacheEntry* b) const {
    return a->GetExpirationTime() == b->GetExpirationTime();
  }
  bool LessThan(CacheEntry* a, CacheEntry* b) const {
    return a->GetExpirationTime() < b->GetExpirationTime();
  }
};

} // namespace

void
CacheStorageService::RegisterEntry(CacheEntry* aEntry)
{
  MOZ_ASSERT(IsOnManagementThread());

  if (mShutdown || !aEntry->CanRegister())
    return;

  TelemetryRecordEntryCreation(aEntry);

  LOG(("CacheStorageService::RegisterEntry [entry=%p]", aEntry));

  MemoryPool& pool = Pool(aEntry->IsUsingDisk());
  pool.mFrecencyArray.AppendElement(aEntry);
  pool.mExpirationArray.AppendElement(aEntry);

  aEntry->SetRegistered(true);
}

void
CacheStorageService::UnregisterEntry(CacheEntry* aEntry)
{
  MOZ_ASSERT(IsOnManagementThread());

  if (!aEntry->IsRegistered())
    return;

  TelemetryRecordEntryRemoval(aEntry);

  LOG(("CacheStorageService::UnregisterEntry [entry=%p]", aEntry));

  MemoryPool& pool = Pool(aEntry->IsUsingDisk());
  mozilla::DebugOnly<bool> removedFrecency = pool.mFrecencyArray.RemoveElement(aEntry);
  mozilla::DebugOnly<bool> removedExpiration = pool.mExpirationArray.RemoveElement(aEntry);

  MOZ_ASSERT(mShutdown || (removedFrecency && removedExpiration));

  // Note: aEntry->CanRegister() since now returns false
  aEntry->SetRegistered(false);
}

static bool
AddExactEntry(CacheEntryTable* aEntries,
              nsACString const& aKey,
              CacheEntry* aEntry,
              bool aOverwrite)
{
  RefPtr<CacheEntry> existingEntry;
  if (!aOverwrite && aEntries->Get(aKey, getter_AddRefs(existingEntry))) {
    bool equals = existingEntry == aEntry;
    LOG(("AddExactEntry [entry=%p equals=%d]", aEntry, equals));
    return equals; // Already there...
  }

  LOG(("AddExactEntry [entry=%p put]", aEntry));
  aEntries->Put(aKey, aEntry);
  return true;
}

bool
CacheStorageService::RemoveEntry(CacheEntry* aEntry, bool aOnlyUnreferenced)
{
  LOG(("CacheStorageService::RemoveEntry [entry=%p]", aEntry));

  nsAutoCString entryKey;
  nsresult rv = aEntry->HashingKey(entryKey);
  if (NS_FAILED(rv)) {
    NS_ERROR("aEntry->HashingKey() failed?");
    return false;
  }

  mozilla::MutexAutoLock lock(mLock);

  if (mShutdown) {
    LOG(("  after shutdown"));
    return false;
  }

  if (aOnlyUnreferenced) {
    if (aEntry->IsReferenced()) {
      LOG(("  still referenced, not removing"));
      return false;
    }

    if (!aEntry->IsUsingDisk() && IsForcedValidEntry(aEntry->GetStorageID(), entryKey)) {
      LOG(("  forced valid, not removing"));
      return false;
    }
  }

  CacheEntryTable* entries;
  if (sGlobalEntryTables->Get(aEntry->GetStorageID(), &entries))
    RemoveExactEntry(entries, entryKey, aEntry, false /* don't overwrite */);

  nsAutoCString memoryStorageID(aEntry->GetStorageID());
  AppendMemoryStorageID(memoryStorageID);

  if (sGlobalEntryTables->Get(memoryStorageID, &entries))
    RemoveExactEntry(entries, entryKey, aEntry, false /* don't overwrite */);

  return true;
}

void
CacheStorageService::RecordMemoryOnlyEntry(CacheEntry* aEntry,
                                           bool aOnlyInMemory,
                                           bool aOverwrite)
{
  LOG(("CacheStorageService::RecordMemoryOnlyEntry [entry=%p, memory=%d, overwrite=%d]",
    aEntry, aOnlyInMemory, aOverwrite));
  // This method is responsible to put this entry to a special record hashtable
  // that contains only entries that are stored in memory.
  // Keep in mind that every entry, regardless of whether is in-memory-only or not
  // is always recorded in the storage master hash table, the one identified by
  // CacheEntry.StorageID().

  mLock.AssertCurrentThreadOwns();

  if (mShutdown) {
    LOG(("  after shutdown"));
    return;
  }

  nsresult rv;

  nsAutoCString entryKey;
  rv = aEntry->HashingKey(entryKey);
  if (NS_FAILED(rv)) {
    NS_ERROR("aEntry->HashingKey() failed?");
    return;
  }

  CacheEntryTable* entries = nullptr;
  nsAutoCString memoryStorageID(aEntry->GetStorageID());
  AppendMemoryStorageID(memoryStorageID);

  if (!sGlobalEntryTables->Get(memoryStorageID, &entries)) {
    if (!aOnlyInMemory) {
      LOG(("  not recorded as memory only"));
      return;
    }

    entries = new CacheEntryTable(CacheEntryTable::MEMORY_ONLY);
    sGlobalEntryTables->Put(memoryStorageID, entries);
    LOG(("  new memory-only storage table for %s", memoryStorageID.get()));
  }

  if (aOnlyInMemory) {
    AddExactEntry(entries, entryKey, aEntry, aOverwrite);
  }
  else {
    RemoveExactEntry(entries, entryKey, aEntry, aOverwrite);
  }
}

// Checks if a cache entry is forced valid (will be loaded directly from cache
// without further validation) - see nsICacheEntry.idl for further details
bool CacheStorageService::IsForcedValidEntry(nsACString const &aContextKey,
                                             nsACString const &aEntryKey)
{
  return IsForcedValidEntry(aContextKey + aEntryKey);
}

bool CacheStorageService::IsForcedValidEntry(nsACString const &aContextEntryKey)
{
  mozilla::MutexAutoLock lock(mForcedValidEntriesLock);

  TimeStamp validUntil;

  if (!mForcedValidEntries.Get(aContextEntryKey, &validUntil)) {
    return false;
  }

  if (validUntil.IsNull()) {
    return false;
  }

  // Entry timeout not reached yet
  if (TimeStamp::NowLoRes() <= validUntil) {
    return true;
  }

  // Entry timeout has been reached
  mForcedValidEntries.Remove(aContextEntryKey);
  return false;
}

// Allows a cache entry to be loaded directly from cache without further
// validation - see nsICacheEntry.idl for further details
void CacheStorageService::ForceEntryValidFor(nsACString const &aContextKey,
                                             nsACString const &aEntryKey,
                                             uint32_t aSecondsToTheFuture)
{
  mozilla::MutexAutoLock lock(mForcedValidEntriesLock);

  TimeStamp now = TimeStamp::NowLoRes();
  ForcedValidEntriesPrune(now);

  // This will be the timeout
  TimeStamp validUntil = now + TimeDuration::FromSeconds(aSecondsToTheFuture);

  mForcedValidEntries.Put(aContextKey + aEntryKey, validUntil);
}

void CacheStorageService::RemoveEntryForceValid(nsACString const &aContextKey,
                                                nsACString const &aEntryKey)
{
  mozilla::MutexAutoLock lock(mForcedValidEntriesLock);

  LOG(("CacheStorageService::RemoveEntryForceValid context='%s' entryKey=%s",
       aContextKey.BeginReading(), aEntryKey.BeginReading()));
  mForcedValidEntries.Remove(aContextKey + aEntryKey);
}

// Cleans out the old entries in mForcedValidEntries
void CacheStorageService::ForcedValidEntriesPrune(TimeStamp &now)
{
  static TimeDuration const oneMinute = TimeDuration::FromSeconds(60);
  static TimeStamp dontPruneUntil = now + oneMinute;
  if (now < dontPruneUntil)
    return;

  for (auto iter = mForcedValidEntries.Iter(); !iter.Done(); iter.Next()) {
    if (iter.Data() < now) {
      iter.Remove();
    }
  }
  dontPruneUntil = now + oneMinute;
}

void
CacheStorageService::OnMemoryConsumptionChange(CacheMemoryConsumer* aConsumer,
                                               uint32_t aCurrentMemoryConsumption)
{
  LOG(("CacheStorageService::OnMemoryConsumptionChange [consumer=%p, size=%u]",
    aConsumer, aCurrentMemoryConsumption));

  uint32_t savedMemorySize = aConsumer->mReportedMemoryConsumption;
  if (savedMemorySize == aCurrentMemoryConsumption)
    return;

  // Exchange saved size with current one.
  aConsumer->mReportedMemoryConsumption = aCurrentMemoryConsumption;

  bool usingDisk = !(aConsumer->mFlags & CacheMemoryConsumer::MEMORY_ONLY);
  bool overLimit = Pool(usingDisk).OnMemoryConsumptionChange(
    savedMemorySize, aCurrentMemoryConsumption);

  if (!overLimit)
    return;

  // It's likely the timer has already been set when we get here,
  // check outside the lock to save resources.
  if (mPurgeTimer)
    return;

  // We don't know if this is called under the service lock or not,
  // hence rather dispatch.
  RefPtr<nsIEventTarget> cacheIOTarget = Thread();
  if (!cacheIOTarget)
    return;

  // Dispatch as a priority task, we want to set the purge timer
  // ASAP to prevent vain redispatch of this event.
  nsCOMPtr<nsIRunnable> event =
    NewRunnableMethod("net::CacheStorageService::SchedulePurgeOverMemoryLimit",
                      this,
                      &CacheStorageService::SchedulePurgeOverMemoryLimit);
  cacheIOTarget->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
}

bool
CacheStorageService::MemoryPool::OnMemoryConsumptionChange(uint32_t aSavedMemorySize,
                                                           uint32_t aCurrentMemoryConsumption)
{
  mMemorySize -= aSavedMemorySize;
  mMemorySize += aCurrentMemoryConsumption;

  LOG(("  mMemorySize=%u (+%u,-%u)", uint32_t(mMemorySize), aCurrentMemoryConsumption, aSavedMemorySize));

  // Bypass purging when memory has not grew up significantly
  if (aCurrentMemoryConsumption <= aSavedMemorySize)
    return false;

  return mMemorySize > Limit();
}

void
CacheStorageService::SchedulePurgeOverMemoryLimit()
{
  LOG(("CacheStorageService::SchedulePurgeOverMemoryLimit"));

  mozilla::MutexAutoLock lock(mLock);

  if (mShutdown) {
    LOG(("  past shutdown"));
    return;
  }

  if (mPurgeTimer) {
    LOG(("  timer already up"));
    return;
  }

  mPurgeTimer = NS_NewTimer();
  if (mPurgeTimer) {
    nsresult rv;
    rv = mPurgeTimer->InitWithCallback(this, 1000, nsITimer::TYPE_ONE_SHOT);
    LOG(("  timer init rv=0x%08" PRIx32, static_cast<uint32_t>(rv)));
  }
}

NS_IMETHODIMP
CacheStorageService::Notify(nsITimer* aTimer)
{
  LOG(("CacheStorageService::Notify"));

  mozilla::MutexAutoLock lock(mLock);

  if (aTimer == mPurgeTimer) {
    mPurgeTimer = nullptr;

    nsCOMPtr<nsIRunnable> event =
      NewRunnableMethod("net::CacheStorageService::PurgeOverMemoryLimit",
                        this,
                        &CacheStorageService::PurgeOverMemoryLimit);
    Dispatch(event);
  }

  return NS_OK;
}

NS_IMETHODIMP
CacheStorageService::GetName(nsACString& aName)
{
  aName.AssignLiteral("CacheStorageService");
  return NS_OK;
}

void
CacheStorageService::PurgeOverMemoryLimit()
{
  MOZ_ASSERT(IsOnManagementThread());

  LOG(("CacheStorageService::PurgeOverMemoryLimit"));

  static TimeDuration const kFourSeconds = TimeDuration::FromSeconds(4);
  TimeStamp now = TimeStamp::NowLoRes();

  if (!mLastPurgeTime.IsNull() && now - mLastPurgeTime < kFourSeconds) {
    LOG(("  bypassed, too soon"));
    return;
  }

  mLastPurgeTime = now;

  Pool(true).PurgeOverMemoryLimit();
  Pool(false).PurgeOverMemoryLimit();
}

void
CacheStorageService::MemoryPool::PurgeOverMemoryLimit()
{
  TimeStamp start(TimeStamp::Now());

  uint32_t const memoryLimit = Limit();
  if (mMemorySize > memoryLimit) {
    LOG(("  memory data consumption over the limit, abandon expired entries"));
    PurgeExpired();
  }

  bool frecencyNeedsSort = true;

  // No longer makes sense since:
  // Memory entries are never purged partially, only as a whole when the memory
  // cache limit is overreached.
  // Disk entries throw the data away ASAP so that only metadata are kept.
  // TODO when this concept of two separate pools is found working, the code should
  // clean up.
#if 0
  if (mMemorySize > memoryLimit) {
    LOG(("  memory data consumption over the limit, abandon disk backed data"));
    PurgeByFrecency(frecencyNeedsSort, CacheEntry::PURGE_DATA_ONLY_DISK_BACKED);
  }

  if (mMemorySize > memoryLimit) {
    LOG(("  metadata consumtion over the limit, abandon disk backed entries"));
    PurgeByFrecency(frecencyNeedsSort, CacheEntry::PURGE_WHOLE_ONLY_DISK_BACKED);
  }
#endif

  if (mMemorySize > memoryLimit) {
    LOG(("  memory data consumption over the limit, abandon any entry"));
    PurgeByFrecency(frecencyNeedsSort, CacheEntry::PURGE_WHOLE);
  }

  LOG(("  purging took %1.2fms", (TimeStamp::Now() - start).ToMilliseconds()));
}

void
CacheStorageService::MemoryPool::PurgeExpired()
{
  MOZ_ASSERT(IsOnManagementThread());

  mExpirationArray.Sort(ExpirationComparator());
  uint32_t now = NowInSeconds();

  uint32_t const memoryLimit = Limit();

  for (uint32_t i = 0; mMemorySize > memoryLimit && i < mExpirationArray.Length();) {
    if (CacheIOThread::YieldAndRerun())
      return;

    RefPtr<CacheEntry> entry = mExpirationArray[i];

    uint32_t expirationTime = entry->GetExpirationTime();
    if (expirationTime > 0 && expirationTime <= now &&
        entry->Purge(CacheEntry::PURGE_WHOLE)) {
      LOG(("  purged expired, entry=%p, exptime=%u (now=%u)",
        entry.get(), entry->GetExpirationTime(), now));
      continue;
    }

    // not purged, move to the next one
    ++i;
  }
}

void
CacheStorageService::MemoryPool::PurgeByFrecency(bool &aFrecencyNeedsSort, uint32_t aWhat)
{
  MOZ_ASSERT(IsOnManagementThread());

  if (aFrecencyNeedsSort) {
    mFrecencyArray.Sort(FrecencyComparator());
    aFrecencyNeedsSort = false;
  }

  uint32_t const memoryLimit = Limit();

  for (uint32_t i = 0; mMemorySize > memoryLimit && i < mFrecencyArray.Length();) {
    if (CacheIOThread::YieldAndRerun())
      return;

    RefPtr<CacheEntry> entry = mFrecencyArray[i];

    if (entry->Purge(aWhat)) {
      LOG(("  abandoned (%d), entry=%p, frecency=%1.10f",
        aWhat, entry.get(), entry->GetFrecency()));
      continue;
    }

    // not purged, move to the next one
    ++i;
  }
}

void
CacheStorageService::MemoryPool::PurgeAll(uint32_t aWhat)
{
  LOG(("CacheStorageService::MemoryPool::PurgeAll aWhat=%d", aWhat));
  MOZ_ASSERT(IsOnManagementThread());

  for (uint32_t i = 0; i < mFrecencyArray.Length();) {
    if (CacheIOThread::YieldAndRerun())
      return;

    RefPtr<CacheEntry> entry = mFrecencyArray[i];

    if (entry->Purge(aWhat)) {
      LOG(("  abandoned entry=%p", entry.get()));
      continue;
    }

    // not purged, move to the next one
    ++i;
  }
}

// Methods exposed to and used by CacheStorage.

nsresult
CacheStorageService::AddStorageEntry(CacheStorage const* aStorage,
                                     const nsACString & aURI,
                                     const nsACString & aIdExtension,
                                     bool aReplace,
                                     CacheEntryHandle** aResult)
{
  NS_ENSURE_FALSE(mShutdown, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG(aStorage);

  nsAutoCString contextKey;
  CacheFileUtils::AppendKeyPrefix(aStorage->LoadInfo(), contextKey);

  return AddStorageEntry(contextKey, aURI, aIdExtension,
                         aStorage->WriteToDisk(),
                         aStorage->SkipSizeCheck(),
                         aStorage->Pinning(),
                         aReplace,
                         aResult);
}

nsresult
CacheStorageService::AddStorageEntry(const nsACString& aContextKey,
                                     const nsACString & aURI,
                                     const nsACString & aIdExtension,
                                     bool aWriteToDisk,
                                     bool aSkipSizeCheck,
                                     bool aPin,
                                     bool aReplace,
                                     CacheEntryHandle** aResult)
{
  nsresult rv;

  nsAutoCString entryKey;
  rv = CacheEntry::HashingKey(EmptyCString(), aIdExtension, aURI, entryKey);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("CacheStorageService::AddStorageEntry [entryKey=%s, contextKey=%s]",
    entryKey.get(), aContextKey.BeginReading()));

  RefPtr<CacheEntry> entry;
  RefPtr<CacheEntryHandle> handle;

  {
    mozilla::MutexAutoLock lock(mLock);

    NS_ENSURE_FALSE(mShutdown, NS_ERROR_NOT_INITIALIZED);

    // Ensure storage table
    CacheEntryTable* entries;
    if (!sGlobalEntryTables->Get(aContextKey, &entries)) {
      entries = new CacheEntryTable(CacheEntryTable::ALL_ENTRIES);
      sGlobalEntryTables->Put(aContextKey, entries);
      LOG(("  new storage entries table for context '%s'", aContextKey.BeginReading()));
    }

    bool entryExists = entries->Get(entryKey, getter_AddRefs(entry));

    if (entryExists && !aReplace) {
      // check whether we want to turn this entry to a memory-only.
      if (MOZ_UNLIKELY(!aWriteToDisk) && MOZ_LIKELY(entry->IsUsingDisk())) {
        LOG(("  entry is persistent but we want mem-only, replacing it"));
        aReplace = true;
      }
    }

    // If truncate is demanded, delete and doom the current entry
    if (entryExists && aReplace) {
      entries->Remove(entryKey);

      LOG(("  dooming entry %p for %s because of OPEN_TRUNCATE", entry.get(), entryKey.get()));
      // On purpose called under the lock to prevent races of doom and open on I/O thread
      // No need to remove from both memory-only and all-entries tables.  The new entry
      // will overwrite the shadow entry in its ctor.
      entry->DoomAlreadyRemoved();

      entry = nullptr;
      entryExists = false;

      // Would only lead to deleting force-valid timestamp again.  We don't need the
      // replace information anymore after this point anyway.
      aReplace = false;
    }

    // Ensure entry for the particular URL
    if (!entryExists) {
      // When replacing with a new entry, always remove the current force-valid timestamp,
      // this is the only place to do it.
      if (aReplace) {
        RemoveEntryForceValid(aContextKey, entryKey);
      }

      // Entry is not in the hashtable or has just been truncated...
      entry = new CacheEntry(aContextKey, aURI, aIdExtension, aWriteToDisk, aSkipSizeCheck, aPin);
      entries->Put(entryKey, entry);
      LOG(("  new entry %p for %s", entry.get(), entryKey.get()));
    }

    if (entry) {
      // Here, if this entry was not for a long time referenced by any consumer,
      // gets again first 'handles count' reference.
      handle = entry->NewHandle();
    }
  }

  handle.forget(aResult);
  return NS_OK;
}

nsresult
CacheStorageService::CheckStorageEntry(CacheStorage const* aStorage,
                                       const nsACString & aURI,
                                       const nsACString & aIdExtension,
                                       bool* aResult)
{
  nsresult rv;

  nsAutoCString contextKey;
  CacheFileUtils::AppendKeyPrefix(aStorage->LoadInfo(), contextKey);

  if (!aStorage->WriteToDisk()) {
    AppendMemoryStorageID(contextKey);
  }

  LOG(("CacheStorageService::CheckStorageEntry [uri=%s, eid=%s, contextKey=%s]",
    aURI.BeginReading(), aIdExtension.BeginReading(), contextKey.get()));

  {
    mozilla::MutexAutoLock lock(mLock);

    NS_ENSURE_FALSE(mShutdown, NS_ERROR_NOT_INITIALIZED);

    nsAutoCString entryKey;
    rv = CacheEntry::HashingKey(EmptyCString(), aIdExtension, aURI, entryKey);
    NS_ENSURE_SUCCESS(rv, rv);

    CacheEntryTable* entries;
    if ((*aResult = sGlobalEntryTables->Get(contextKey, &entries)) &&
        entries->GetWeak(entryKey, aResult)) {
      LOG(("  found in hash tables"));
      return NS_OK;
    }
  }

  if (!aStorage->WriteToDisk()) {
    // Memory entry, nothing more to do.
    LOG(("  not found in hash tables"));
    return NS_OK;
  }

  // Disk entry, not found in the hashtable, check the index.
  nsAutoCString fileKey;
  rv = CacheEntry::HashingKey(contextKey, aIdExtension, aURI, fileKey);

  CacheIndex::EntryStatus status;
  rv = CacheIndex::HasEntry(fileKey, &status);
  if (NS_FAILED(rv) || status == CacheIndex::DO_NOT_KNOW) {
    LOG(("  index doesn't know, rv=0x%08" PRIx32, static_cast<uint32_t>(rv)));
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResult = status == CacheIndex::EXISTS;
  LOG(("  %sfound in index", *aResult ? "" : "not "));
  return NS_OK;
}

nsresult
CacheStorageService::GetCacheIndexEntryAttrs(CacheStorage const* aStorage,
                                             const nsACString &aURI,
                                             const nsACString &aIdExtension,
                                             bool *aHasAltData,
                                             uint32_t *aFileSizeKb)
{
  nsresult rv;

  nsAutoCString contextKey;
  CacheFileUtils::AppendKeyPrefix(aStorage->LoadInfo(), contextKey);

  LOG(("CacheStorageService::GetCacheIndexEntryAttrs [uri=%s, eid=%s, contextKey=%s]",
    aURI.BeginReading(), aIdExtension.BeginReading(), contextKey.get()));

  nsAutoCString fileKey;
  rv = CacheEntry::HashingKey(contextKey, aIdExtension, aURI, fileKey);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aHasAltData = false;
  *aFileSizeKb = 0;
  auto closure = [&aHasAltData, &aFileSizeKb](const CacheIndexEntry *entry) {
    *aHasAltData = entry->GetHasAltData();
    *aFileSizeKb = entry->GetFileSize();
  };

  CacheIndex::EntryStatus status;
  rv = CacheIndex::HasEntry(fileKey, &status, closure);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (status != CacheIndex::EXISTS) {
    return NS_ERROR_CACHE_KEY_NOT_FOUND;
  }

  return NS_OK;
}


namespace {

class CacheEntryDoomByKeyCallback : public CacheFileIOListener
                                  , public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  explicit CacheEntryDoomByKeyCallback(nsICacheEntryDoomCallback* aCallback)
    : mCallback(aCallback)
    , mResult(NS_ERROR_NOT_INITIALIZED)
  {
  }

private:
  virtual ~CacheEntryDoomByKeyCallback();

  NS_IMETHOD OnFileOpened(CacheFileHandle *aHandle, nsresult aResult) override { return NS_OK; }
  NS_IMETHOD OnDataWritten(CacheFileHandle *aHandle, const char *aBuf, nsresult aResult) override { return NS_OK; }
  NS_IMETHOD OnDataRead(CacheFileHandle *aHandle, char *aBuf, nsresult aResult) override { return NS_OK; }
  NS_IMETHOD OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult) override;
  NS_IMETHOD OnEOFSet(CacheFileHandle *aHandle, nsresult aResult) override { return NS_OK; }
  NS_IMETHOD OnFileRenamed(CacheFileHandle *aHandle, nsresult aResult) override { return NS_OK; }

  nsCOMPtr<nsICacheEntryDoomCallback> mCallback;
  nsresult mResult;
};

CacheEntryDoomByKeyCallback::~CacheEntryDoomByKeyCallback()
{
  if (mCallback)
    ProxyReleaseMainThread(
      "CacheEntryDoomByKeyCallback::mCallback", mCallback);
}

NS_IMETHODIMP CacheEntryDoomByKeyCallback::OnFileDoomed(CacheFileHandle *aHandle,
                                                        nsresult aResult)
{
  if (!mCallback)
    return NS_OK;

  mResult = aResult;
  if (NS_IsMainThread()) {
    Run();
  } else {
    NS_DispatchToMainThread(this);
  }

  return NS_OK;
}

NS_IMETHODIMP CacheEntryDoomByKeyCallback::Run()
{
  mCallback->OnCacheEntryDoomed(mResult);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(CacheEntryDoomByKeyCallback, CacheFileIOListener, nsIRunnable);

} // namespace

nsresult
CacheStorageService::DoomStorageEntry(CacheStorage const* aStorage,
                                      const nsACString & aURI,
                                      const nsACString & aIdExtension,
                                      nsICacheEntryDoomCallback* aCallback)
{
  LOG(("CacheStorageService::DoomStorageEntry"));

  NS_ENSURE_ARG(aStorage);

  nsAutoCString contextKey;
  CacheFileUtils::AppendKeyPrefix(aStorage->LoadInfo(), contextKey);

  nsAutoCString entryKey;
  nsresult rv = CacheEntry::HashingKey(EmptyCString(), aIdExtension, aURI, entryKey);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<CacheEntry> entry;
  {
    mozilla::MutexAutoLock lock(mLock);

    NS_ENSURE_FALSE(mShutdown, NS_ERROR_NOT_INITIALIZED);

    CacheEntryTable* entries;
    if (sGlobalEntryTables->Get(contextKey, &entries)) {
      if (entries->Get(entryKey, getter_AddRefs(entry))) {
        if (aStorage->WriteToDisk() || !entry->IsUsingDisk()) {
          // When evicting from disk storage, purge
          // When evicting from memory storage and the entry is memory-only, purge
          LOG(("  purging entry %p for %s [storage use disk=%d, entry use disk=%d]",
            entry.get(), entryKey.get(), aStorage->WriteToDisk(), entry->IsUsingDisk()));
          entries->Remove(entryKey);
        }
        else {
          // Otherwise, leave it
          LOG(("  leaving entry %p for %s [storage use disk=%d, entry use disk=%d]",
            entry.get(), entryKey.get(), aStorage->WriteToDisk(), entry->IsUsingDisk()));
          entry = nullptr;
        }
      }
    }

    if (!entry) {
      RemoveEntryForceValid(contextKey, entryKey);
    }
  }

  if (entry) {
    LOG(("  dooming entry %p for %s", entry.get(), entryKey.get()));
    return entry->AsyncDoom(aCallback);
  }

  LOG(("  no entry loaded for %s", entryKey.get()));

  if (aStorage->WriteToDisk()) {
    nsAutoCString contextKey;
    CacheFileUtils::AppendKeyPrefix(aStorage->LoadInfo(), contextKey);

    rv = CacheEntry::HashingKey(contextKey, aIdExtension, aURI, entryKey);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("  dooming file only for %s", entryKey.get()));

    RefPtr<CacheEntryDoomByKeyCallback> callback(
      new CacheEntryDoomByKeyCallback(aCallback));
    rv = CacheFileIOManager::DoomFileByKey(entryKey, callback);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  class Callback : public Runnable
  {
  public:
    explicit Callback(nsICacheEntryDoomCallback* aCallback)
      : mozilla::Runnable("Callback")
      , mCallback(aCallback)
    {
    }
    NS_IMETHOD Run() override
    {
      mCallback->OnCacheEntryDoomed(NS_ERROR_NOT_AVAILABLE);
      return NS_OK;
    }
    nsCOMPtr<nsICacheEntryDoomCallback> mCallback;
  };

  if (aCallback) {
    RefPtr<Runnable> callback = new Callback(aCallback);
    return NS_DispatchToMainThread(callback);
  }

  return NS_OK;
}

nsresult
CacheStorageService::DoomStorageEntries(CacheStorage const* aStorage,
                                        nsICacheEntryDoomCallback* aCallback)
{
  LOG(("CacheStorageService::DoomStorageEntries"));

  NS_ENSURE_FALSE(mShutdown, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG(aStorage);

  nsAutoCString contextKey;
  CacheFileUtils::AppendKeyPrefix(aStorage->LoadInfo(), contextKey);

  mozilla::MutexAutoLock lock(mLock);

  return DoomStorageEntries(contextKey, aStorage->LoadInfo(),
                            aStorage->WriteToDisk(), aStorage->Pinning(),
                            aCallback);
}

nsresult
CacheStorageService::DoomStorageEntries(const nsACString& aContextKey,
                                        nsILoadContextInfo* aContext,
                                        bool aDiskStorage,
                                        bool aPinned,
                                        nsICacheEntryDoomCallback* aCallback)
{
  LOG(("CacheStorageService::DoomStorageEntries [context=%s]", aContextKey.BeginReading()));

  mLock.AssertCurrentThreadOwns();

  NS_ENSURE_TRUE(!mShutdown, NS_ERROR_NOT_INITIALIZED);

  nsAutoCString memoryStorageID(aContextKey);
  AppendMemoryStorageID(memoryStorageID);

  if (aDiskStorage) {
    LOG(("  dooming disk+memory storage of %s", aContextKey.BeginReading()));

    // Walk one by one and remove entries according their pin status
    CacheEntryTable *diskEntries, *memoryEntries;
    if (sGlobalEntryTables->Get(aContextKey, &diskEntries)) {
      sGlobalEntryTables->Get(memoryStorageID, &memoryEntries);

      for (auto iter = diskEntries->Iter(); !iter.Done(); iter.Next()) {
        auto entry = iter.Data();
        if (entry->DeferOrBypassRemovalOnPinStatus(aPinned)) {
          continue;
        }

        if (memoryEntries) {
          RemoveExactEntry(memoryEntries, iter.Key(), entry, false);
        }
        iter.Remove();
      }
    }

    if (aContext && !aContext->IsPrivate()) {
      LOG(("  dooming disk entries"));
      CacheFileIOManager::EvictByContext(aContext, aPinned, EmptyString());
    }
  } else {
    LOG(("  dooming memory-only storage of %s", aContextKey.BeginReading()));

    // Remove the memory entries table from the global tables.
    // Since we store memory entries also in the disk entries table
    // we need to remove the memory entries from the disk table one
    // by one manually.
    nsAutoPtr<CacheEntryTable> memoryEntries;
    sGlobalEntryTables->Remove(memoryStorageID, &memoryEntries);

    CacheEntryTable* diskEntries;
    if (memoryEntries && sGlobalEntryTables->Get(aContextKey, &diskEntries)) {
      for (auto iter = memoryEntries->Iter(); !iter.Done(); iter.Next()) {
        auto entry = iter.Data();
        RemoveExactEntry(diskEntries, iter.Key(), entry, false);
      }
    }
  }

  {
    mozilla::MutexAutoLock lock(mForcedValidEntriesLock);

    if (aContext) {
      for (auto iter = mForcedValidEntries.Iter(); !iter.Done(); iter.Next()) {
        bool matches;
        DebugOnly<nsresult> rv = CacheFileUtils::KeyMatchesLoadContextInfo(
          iter.Key(), aContext, &matches);
        MOZ_ASSERT(NS_SUCCEEDED(rv));

        if (matches) {
          iter.Remove();
        }
      }
    } else {
      mForcedValidEntries.Clear();
    }
  }

  // An artificial callback.  This is a candidate for removal tho.  In the new
  // cache any 'doom' or 'evict' function ensures that the entry or entries
  // being doomed is/are not accessible after the function returns.  So there is
  // probably no need for a callback - has no meaning.  But for compatibility
  // with the old cache that is still in the tree we keep the API similar to be
  // able to make tests as well as other consumers work for now.
  class Callback : public Runnable
  {
  public:
    explicit Callback(nsICacheEntryDoomCallback* aCallback)
      : mozilla::Runnable("Callback")
      , mCallback(aCallback)
    {
    }
    NS_IMETHOD Run() override
    {
      mCallback->OnCacheEntryDoomed(NS_OK);
      return NS_OK;
    }
    nsCOMPtr<nsICacheEntryDoomCallback> mCallback;
  };

  if (aCallback) {
    RefPtr<Runnable> callback = new Callback(aCallback);
    return NS_DispatchToMainThread(callback);
  }

  return NS_OK;
}

nsresult
CacheStorageService::WalkStorageEntries(CacheStorage const* aStorage,
                                        bool aVisitEntries,
                                        nsICacheStorageVisitor* aVisitor)
{
  LOG(("CacheStorageService::WalkStorageEntries [cb=%p, visitentries=%d]", aVisitor, aVisitEntries));
  NS_ENSURE_FALSE(mShutdown, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG(aStorage);

  if (aStorage->WriteToDisk()) {
    RefPtr<WalkDiskCacheRunnable> event =
      new WalkDiskCacheRunnable(aStorage->LoadInfo(), aVisitEntries, aVisitor);
    return event->Walk();
  }

  RefPtr<WalkMemoryCacheRunnable> event =
    new WalkMemoryCacheRunnable(aStorage->LoadInfo(), aVisitEntries, aVisitor);
  return event->Walk();
}

void
CacheStorageService::CacheFileDoomed(nsILoadContextInfo* aLoadContextInfo,
                                     const nsACString & aIdExtension,
                                     const nsACString & aURISpec)
{
  nsAutoCString contextKey;
  CacheFileUtils::AppendKeyPrefix(aLoadContextInfo, contextKey);

  nsAutoCString entryKey;
  CacheEntry::HashingKey(EmptyCString(), aIdExtension, aURISpec, entryKey);

  mozilla::MutexAutoLock lock(mLock);

  if (mShutdown) {
    return;
  }

  CacheEntryTable* entries;
  RefPtr<CacheEntry> entry;

  if (sGlobalEntryTables->Get(contextKey, &entries) &&
      entries->Get(entryKey, getter_AddRefs(entry))) {
    if (entry->IsFileDoomed()) {
      // Need to remove under the lock to avoid possible race leading
      // to duplication of the entry per its key.
      RemoveExactEntry(entries, entryKey, entry, false);
      entry->DoomAlreadyRemoved();
    }

    // Entry found, but it's not the entry that has been found doomed
    // by the lower eviction layer.  Just leave everything unchanged.
    return;
  }

  RemoveEntryForceValid(contextKey, entryKey);
}

bool
CacheStorageService::GetCacheEntryInfo(nsILoadContextInfo* aLoadContextInfo,
                                       const nsACString & aIdExtension,
                                       const nsACString & aURISpec,
                                       EntryInfoCallback *aCallback)
{
  nsAutoCString contextKey;
  CacheFileUtils::AppendKeyPrefix(aLoadContextInfo, contextKey);

  nsAutoCString entryKey;
  CacheEntry::HashingKey(EmptyCString(), aIdExtension, aURISpec, entryKey);

  RefPtr<CacheEntry> entry;
  {
    mozilla::MutexAutoLock lock(mLock);

    if (mShutdown) {
      return false;
    }

    CacheEntryTable* entries;
    if (!sGlobalEntryTables->Get(contextKey, &entries)) {
      return false;
    }

    if (!entries->Get(entryKey, getter_AddRefs(entry))) {
      return false;
    }
  }

  GetCacheEntryInfo(entry, aCallback);
  return true;
}

// static
void
CacheStorageService::GetCacheEntryInfo(CacheEntry* aEntry,
                                       EntryInfoCallback *aCallback)
{
  nsCString const uriSpec = aEntry->GetURI();
  nsCString const enhanceId = aEntry->GetEnhanceID();

  nsAutoCString entryKey;
  aEntry->HashingKeyWithStorage(entryKey);

  nsCOMPtr<nsILoadContextInfo> info =
    CacheFileUtils::ParseKey(entryKey);

  uint32_t dataSize;
  if (NS_FAILED(aEntry->GetStorageDataSize(&dataSize))) {
    dataSize = 0;
  }
  int32_t fetchCount;
  if (NS_FAILED(aEntry->GetFetchCount(&fetchCount))) {
    fetchCount = 0;
  }
  uint32_t lastModified;
  if (NS_FAILED(aEntry->GetLastModified(&lastModified))) {
    lastModified = 0;
  }
  uint32_t expirationTime;
  if (NS_FAILED(aEntry->GetExpirationTime(&expirationTime))) {
    expirationTime = 0;
  }

  aCallback->OnEntryInfo(uriSpec, enhanceId, dataSize,
                         fetchCount, lastModified, expirationTime,
                         aEntry->IsPinned(), info);
}

// static
uint32_t CacheStorageService::CacheQueueSize(bool highPriority)
{
  RefPtr<CacheIOThread> thread = CacheFileIOManager::IOThread();
  // The thread will be null at shutdown.
  if (!thread) {
    return 0;
  }
  return thread->QueueSize(highPriority);
}

// Telemetry collection

namespace {

bool TelemetryEntryKey(CacheEntry const* entry, nsAutoCString& key)
{
  nsAutoCString entryKey;
  nsresult rv = entry->HashingKey(entryKey);
  if (NS_FAILED(rv))
    return false;

  if (entry->GetStorageID().IsEmpty()) {
    // Hopefully this will be const-copied, saves some memory
    key = entryKey;
  } else {
    key.Assign(entry->GetStorageID());
    key.Append(':');
    key.Append(entryKey);
  }

  return true;
}

} // namespace

void
CacheStorageService::TelemetryPrune(TimeStamp &now)
{
  static TimeDuration const oneMinute = TimeDuration::FromSeconds(60);
  static TimeStamp dontPruneUntil = now + oneMinute;
  if (now < dontPruneUntil)
    return;

  static TimeDuration const fifteenMinutes = TimeDuration::FromSeconds(900);
  for (auto iter = mPurgeTimeStamps.Iter(); !iter.Done(); iter.Next()) {
    if (now - iter.Data() > fifteenMinutes) {
      // We are not interested in resurrection of entries after 15 minutes
      // of time.  This is also the limit for the telemetry.
      iter.Remove();
    }
  }
  dontPruneUntil = now + oneMinute;
}

void
CacheStorageService::TelemetryRecordEntryCreation(CacheEntry const* entry)
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  nsAutoCString key;
  if (!TelemetryEntryKey(entry, key))
    return;

  TimeStamp now = TimeStamp::NowLoRes();
  TelemetryPrune(now);

  // When an entry is craeted (registered actually) we check if there is
  // a timestamp marked when this very same cache entry has been removed
  // (deregistered) because of over-memory-limit purging.  If there is such
  // a timestamp found accumulate telemetry on how long the entry was away.
  TimeStamp timeStamp;
  if (!mPurgeTimeStamps.Get(key, &timeStamp))
    return;

  mPurgeTimeStamps.Remove(key);

  Telemetry::AccumulateTimeDelta(Telemetry::HTTP_CACHE_ENTRY_RELOAD_TIME,
                                 timeStamp, TimeStamp::NowLoRes());
}

void
CacheStorageService::TelemetryRecordEntryRemoval(CacheEntry const* entry)
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  // Doomed entries must not be considered, we are only interested in purged
  // entries.  Note that the mIsDoomed flag is always set before deregistration
  // happens.
  if (entry->IsDoomed())
    return;

  nsAutoCString key;
  if (!TelemetryEntryKey(entry, key))
    return;

  // When an entry is removed (deregistered actually) we put a timestamp for this
  // entry to the hashtable so that when the entry is created (registered) again
  // we know how long it was away.  Also accumulate number of AsyncOpen calls on
  // the entry, this tells us how efficiently the pool actually works.

  TimeStamp now = TimeStamp::NowLoRes();
  TelemetryPrune(now);
  mPurgeTimeStamps.Put(key, now);

  Telemetry::Accumulate(Telemetry::HTTP_CACHE_ENTRY_REUSE_COUNT, entry->UseCount());
  Telemetry::AccumulateTimeDelta(Telemetry::HTTP_CACHE_ENTRY_ALIVE_TIME,
                                 entry->LoadStart(), TimeStamp::NowLoRes());
}

// nsIMemoryReporter

size_t
CacheStorageService::SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

  size_t n = 0;
  // The elemets are referenced by sGlobalEntryTables and are reported from there
  n += Pool(true).mFrecencyArray.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += Pool(true).mExpirationArray.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += Pool(false).mFrecencyArray.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += Pool(false).mExpirationArray.ShallowSizeOfExcludingThis(mallocSizeOf);
  // Entries reported manually in CacheStorageService::CollectReports callback
  if (sGlobalEntryTables) {
    n += sGlobalEntryTables->ShallowSizeOfIncludingThis(mallocSizeOf);
  }
  n += mPurgeTimeStamps.SizeOfExcludingThis(mallocSizeOf);

  return n;
}

size_t
CacheStorageService::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
}

NS_IMETHODIMP
CacheStorageService::CollectReports(nsIHandleReportCallback* aHandleReport,
                                    nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/network/cache2/io", KIND_HEAP, UNITS_BYTES,
    CacheFileIOManager::SizeOfIncludingThis(MallocSizeOf),
    "Memory used by the cache IO manager.");

  MOZ_COLLECT_REPORT(
    "explicit/network/cache2/index", KIND_HEAP, UNITS_BYTES,
    CacheIndex::SizeOfIncludingThis(MallocSizeOf),
    "Memory used by the cache index.");

  MutexAutoLock lock(mLock);

  // Report the service instance, this doesn't report entries, done lower
  MOZ_COLLECT_REPORT(
    "explicit/network/cache2/service", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(MallocSizeOf),
    "Memory used by the cache storage service.");

  // Report all entries, each storage separately (by the context key)
  //
  // References are:
  // sGlobalEntryTables to N CacheEntryTable
  // CacheEntryTable to N CacheEntry
  // CacheEntry to 1 CacheFile
  // CacheFile to
  //   N CacheFileChunk (keeping the actual data)
  //   1 CacheFileMetadata (keeping http headers etc.)
  //   1 CacheFileOutputStream
  //   N CacheFileInputStream
  if (sGlobalEntryTables) {
    for (auto iter1 = sGlobalEntryTables->Iter(); !iter1.Done(); iter1.Next()) {
      CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

      CacheEntryTable* table = iter1.UserData();

      size_t size = 0;
      mozilla::MallocSizeOf mallocSizeOf = CacheStorageService::MallocSizeOf;

      size += table->ShallowSizeOfIncludingThis(mallocSizeOf);
      for (auto iter2 = table->Iter(); !iter2.Done(); iter2.Next()) {
        size += iter2.Key().SizeOfExcludingThisIfUnshared(mallocSizeOf);

        // Bypass memory-only entries, those will be reported when iterating the
        // memory only table. Memory-only entries are stored in both ALL_ENTRIES
        // and MEMORY_ONLY hashtables.
        RefPtr<mozilla::net::CacheEntry> const& entry = iter2.Data();
        if (table->Type() == CacheEntryTable::MEMORY_ONLY ||
            entry->IsUsingDisk()) {
          size += entry->SizeOfIncludingThis(mallocSizeOf);
        }
      }

      // These key names are not privacy-sensitive.
      aHandleReport->Callback(
        EmptyCString(),
        nsPrintfCString("explicit/network/cache2/%s-storage(%s)",
          table->Type() == CacheEntryTable::MEMORY_ONLY ? "memory" : "disk",
          iter1.Key().BeginReading()),
        nsIMemoryReporter::KIND_HEAP, nsIMemoryReporter::UNITS_BYTES, size,
        NS_LITERAL_CSTRING("Memory used by the cache storage."),
        aData);
    }
  }

  return NS_OK;
}

// nsICacheTesting

NS_IMETHODIMP
CacheStorageService::IOThreadSuspender::Run()
{
  MonitorAutoLock mon(mMon);
  while (!mSignaled) {
    mon.Wait();
  }
  return NS_OK;
}

void
CacheStorageService::IOThreadSuspender::Notify()
{
  MonitorAutoLock mon(mMon);
  mSignaled = true;
  mon.Notify();
}

NS_IMETHODIMP
CacheStorageService::SuspendCacheIOThread(uint32_t aLevel)
{
  RefPtr<CacheIOThread> thread = CacheFileIOManager::IOThread();
  if (!thread) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(!mActiveIOSuspender);
  mActiveIOSuspender = new IOThreadSuspender();
  return thread->Dispatch(mActiveIOSuspender, aLevel);
}

NS_IMETHODIMP
CacheStorageService::ResumeCacheIOThread()
{
  MOZ_ASSERT(mActiveIOSuspender);

  RefPtr<IOThreadSuspender> suspender;
  suspender.swap(mActiveIOSuspender);
  suspender->Notify();
  return NS_OK;
}

NS_IMETHODIMP
CacheStorageService::Flush(nsIObserver* aObserver)
{
  RefPtr<CacheIOThread> thread = CacheFileIOManager::IOThread();
  if (!thread) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Adding as weak, the consumer is responsible to keep the reference
  // until notified.
  observerService->AddObserver(aObserver, "cacheservice:purge-memory-pools", false);

  // This runnable will do the purging and when done, notifies the above observer.
  // We dispatch it to the CLOSE level, so all data writes scheduled up to this time
  // will be done before this purging happens.
  RefPtr<CacheStorageService::PurgeFromMemoryRunnable> r =
    new CacheStorageService::PurgeFromMemoryRunnable(this, CacheEntry::PURGE_WHOLE);

  return thread->Dispatch(r, CacheIOThread::WRITE);
}

} // namespace net
} // namespace mozilla
