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
#include "nsAutoPtr.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsWeakReference.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Services.h"

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
 * <isPrivate|isAnon|appId|inBrowser> which builds a mapping key.
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

uint32_t const
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
                  nsITimerCallback)

CacheStorageService* CacheStorageService::sSelf = nullptr;

CacheStorageService::CacheStorageService()
: mLock("CacheStorageService.mLock")
, mForcedValidEntriesLock("CacheStorageService.mForcedValidEntriesLock")
, mShutdown(false)
, mDiskPool(MemoryPool::DISK)
, mMemoryPool(MemoryPool::MEMORY)
{
  CacheFileIOManager::Init();

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
  if (mShutdown)
    return;

  LOG(("CacheStorageService::Shutdown - start"));

  mShutdown = true;

  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &CacheStorageService::ShutdownBackground);
  Dispatch(event);

  mozilla::MutexAutoLock lock(mLock);
  sGlobalEntryTables->Clear();
  delete sGlobalEntryTables;
  sGlobalEntryTables = nullptr;

  LOG(("CacheStorageService::Shutdown - done"));
}

void CacheStorageService::ShutdownBackground()
{
  MOZ_ASSERT(IsOnManagementThread());

  // Cancel purge timer to avoid leaking.
  if (mPurgeTimer) {
    mPurgeTimer->Cancel();
  }

  Pool(false).mFrecencyArray.Clear();
  Pool(false).mExpirationArray.Clear();
  Pool(true).mFrecencyArray.Clear();
  Pool(true).mExpirationArray.Clear();
}

// Internal management methods

namespace {

// WalkCacheRunnable
// Base class for particular storage entries visiting
class WalkCacheRunnable : public nsRunnable
                        , public CacheStorageService::EntryInfoCallback
{
protected:
  WalkCacheRunnable(nsICacheStorageVisitor* aVisitor,
                    bool aVisitEntries)
    : mService(CacheStorageService::Self())
    , mCallback(aVisitor)
    , mSize(0)
    , mNotifyStorage(true)
    , mVisitEntries(aVisitEntries)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual ~WalkCacheRunnable()
  {
    if (mCallback) {
      ProxyReleaseMainThread(mCallback);
    }
  }

  nsRefPtr<CacheStorageService> mService;
  nsCOMPtr<nsICacheStorageVisitor> mCallback;

  uint64_t mSize;

  bool mNotifyStorage : 1;
  bool mVisitEntries : 1;
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
  NS_IMETHODIMP Run()
  {
    if (CacheStorageService::IsOnManagementThread()) {
      LOG(("WalkMemoryCacheRunnable::Run - collecting [this=%p]", this));
      // First, walk, count and grab all entries from the storage

      mozilla::MutexAutoLock lock(CacheStorageService::Self()->Lock());

      if (!CacheStorageService::IsRunning())
        return NS_ERROR_NOT_INITIALIZED;

      CacheEntryTable* entries;
      if (sGlobalEntryTables->Get(mContextKey, &entries))
        entries->EnumerateRead(&WalkMemoryCacheRunnable::WalkStorage, this);

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
        LOG(("  entry [left=%d]", mEntryArray.Length()));

        // Third, notify each entry until depleted
        if (!mEntryArray.Length()) {
          mCallback->OnCacheEntryVisitCompleted();
          return NS_OK; // done
        }

        // Grab the next entry
        nsRefPtr<CacheEntry> entry = mEntryArray[0];
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
      ProxyReleaseMainThread(mCallback);
  }

  static PLDHashOperator
  WalkStorage(const nsACString& aKey,
              CacheEntry* aEntry,
              void* aClosure)
  {
    WalkMemoryCacheRunnable* walker =
      static_cast<WalkMemoryCacheRunnable*>(aClosure);

    // Ignore disk entries
    if (aEntry->IsUsingDisk())
      return PL_DHASH_NEXT;

    walker->mSize += aEntry->GetMetadataMemoryConsumption();

    int64_t size;
    if (NS_SUCCEEDED(aEntry->GetDataSize(&size)))
      walker->mSize += size;

    walker->mEntryArray.AppendElement(aEntry);
    return PL_DHASH_NEXT;
  }

  virtual void OnEntryInfo(const nsACString & aURISpec, const nsACString & aIdEnhance,
                           int64_t aDataSize, int32_t aFetchCount,
                           uint32_t aLastModifiedTime, uint32_t aExpirationTime)
  {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aURISpec);
    if (NS_FAILED(rv))
      return;

    mCallback->OnCacheEntryInfo(uri, aIdEnhance, aDataSize, aFetchCount,
                                aLastModifiedTime, aExpirationTime);
  }

private:
  nsCString mContextKey;
  nsTArray<nsRefPtr<CacheEntry> > mEntryArray;
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
    nsRefPtr<CacheIOThread> thread = CacheFileIOManager::IOThread();
    NS_ENSURE_TRUE(thread, NS_ERROR_NOT_INITIALIZED);

    return thread->Dispatch(this, CacheIOThread::INDEX);
  }

private:
  // Invokes OnCacheEntryInfo callback for each single found entry.
  // There is one instance of this class per one entry.
  class OnCacheEntryInfoRunnable : public nsRunnable
  {
  public:
    explicit OnCacheEntryInfoRunnable(WalkDiskCacheRunnable* aWalker)
      : mWalker(aWalker)
    {
    }

    NS_IMETHODIMP Run()
    {
      MOZ_ASSERT(NS_IsMainThread());

      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), mURISpec);
      if (NS_FAILED(rv))
        return NS_OK;

      mWalker->mCallback->OnCacheEntryInfo(
        uri, mIdEnhance, mDataSize, mFetchCount,
        mLastModifiedTime, mExpirationTime);
      return NS_OK;
    }

    nsRefPtr<WalkDiskCacheRunnable> mWalker;

    nsCString mURISpec;
    nsCString mIdEnhance;
    int64_t mDataSize;
    int32_t mFetchCount;
    uint32_t mLastModifiedTime;
    uint32_t mExpirationTime;
  };

  NS_IMETHODIMP Run()
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

        mSize = size << 10;

        // Invoke onCacheStorageInfo with valid information.
        NS_DispatchToMainThread(this);

        if (!mVisitEntries) {
          return NS_OK; // done
        }

        mPass = ITERATE_METADATA;
        // no break

      case ITERATE_METADATA:
        // Now grab the context iterator.
        if (!mIter) {
          rv = CacheIndex::GetIterator(mLoadInfo, true, getter_AddRefs(mIter));
          if (NS_FAILED(rv)) {
            // Invoke onCacheEntryVisitCompleted now
            return NS_DispatchToMainThread(this);
          }
        }

        while (true) {
          if (CacheIOThread::YieldAndRerun())
            return NS_OK;

          SHA1Sum::Hash hash;
          rv = mIter->GetNextHash(&hash);
          if (NS_FAILED(rv))
            break; // done (or error?)

          // This synchronously invokes onCacheEntryInfo on this class where we
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
                           uint32_t aLastModifiedTime, uint32_t aExpirationTime)
  {
    // Called directly from CacheFileIOManager::GetEntryInfo.

    // Invoke onCacheEntryInfo on the main thread for this entry.
    nsRefPtr<OnCacheEntryInfoRunnable> info = new OnCacheEntryInfoRunnable(this);
    info->mURISpec = aURISpec;
    info->mIdEnhance = aIdEnhance;
    info->mDataSize = aDataSize;
    info->mFetchCount = aFetchCount;
    info->mLastModifiedTime = aLastModifiedTime;
    info->mExpirationTime = aExpirationTime;

    NS_DispatchToMainThread(info);
  }

  nsRefPtr<nsILoadContextInfo> mLoadInfo;
  enum {
    // First, we collect stats for the load context.
    COLLECT_STATS,

    // Second, if demanded, we iterate over the entries gethered
    // from the iterator and call CacheFileIOManager::GetEntryInfo
    // for each found entry.
    ITERATE_METADATA,
  } mPass;

  nsRefPtr<CacheIndexIterator> mIter;
  uint32_t mCount;
};

PLDHashOperator CollectPrivateContexts(const nsACString& aKey,
                                       CacheEntryTable* aTable,
                                       void* aClosure)
{
  nsCOMPtr<nsILoadContextInfo> info = CacheFileUtils::ParseKey(aKey);
  if (info && info->IsPrivate()) {
    nsTArray<nsCString>* keys = static_cast<nsTArray<nsCString>*>(aClosure);
    keys->AppendElement(aKey);
  }

  return PL_DHASH_NEXT;
}

PLDHashOperator CollectContexts(const nsACString& aKey,
                                       CacheEntryTable* aTable,
                                       void* aClosure)
{
  nsTArray<nsCString>* keys = static_cast<nsTArray<nsCString>*>(aClosure);
  keys->AppendElement(aKey);

  return PL_DHASH_NEXT;
}

} // namespace

void CacheStorageService::DropPrivateBrowsingEntries()
{
  mozilla::MutexAutoLock lock(mLock);

  if (mShutdown)
    return;

  nsTArray<nsCString> keys;
  sGlobalEntryTables->EnumerateRead(&CollectPrivateContexts, &keys);

  for (uint32_t i = 0; i < keys.Length(); ++i)
    DoomStorageEntries(keys[i], nullptr, true, nullptr);
}

namespace {

class CleaupCacheDirectoriesRunnable : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE
  static bool Post(uint32_t aVersion, uint32_t aActive);

private:
  CleaupCacheDirectoriesRunnable(uint32_t aVersion, uint32_t aActive)
    : mVersion(aVersion), mActive(aActive)
  {
    nsCacheService::GetDiskCacheDirectory(getter_AddRefs(mCache1Dir));
    CacheFileIOManager::GetCacheDirectory(getter_AddRefs(mCache2Dir));
#if defined(MOZ_WIDGET_ANDROID)
    CacheFileIOManager::GetProfilelessCacheDirectory(getter_AddRefs(mCache2Profileless));
#endif
  }

  virtual ~CleaupCacheDirectoriesRunnable() {}
  uint32_t mVersion, mActive;
  nsCOMPtr<nsIFile> mCache1Dir, mCache2Dir;
#if defined(MOZ_WIDGET_ANDROID)
  nsCOMPtr<nsIFile> mCache2Profileless;
#endif
};

// static
bool CleaupCacheDirectoriesRunnable::Post(uint32_t aVersion, uint32_t aActive)
{
  // CleaupCacheDirectories is called regardless what cache version is set up to use.
  // To obtain the cache1 directory we must unfortunatelly instantiate the old cache
  // service despite it may not be used at all...  This also initialize nsDeleteDir.
  nsCOMPtr<nsICacheService> service = do_GetService(NS_CACHESERVICE_CONTRACTID);
  if (!service)
    return false;

  nsCOMPtr<nsIEventTarget> thread;
  service->GetCacheIOTarget(getter_AddRefs(thread));
  if (!thread)
    return false;

  nsRefPtr<CleaupCacheDirectoriesRunnable> r =
    new CleaupCacheDirectoriesRunnable(aVersion, aActive);
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

  // Delete the non-active version cache data right now
  if (mVersion == mActive) {
    return NS_OK;
  }

  switch (mVersion) {
  case 0:
    if (mCache1Dir) {
      nsDeleteDir::DeleteDir(mCache1Dir, true, 30000);
    }
    break;
  case 1:
    if (mCache2Dir) {
      nsDeleteDir::DeleteDir(mCache2Dir, true, 30000);
    }
    break;
  }

  return NS_OK;
}

} // namespace

// static
void CacheStorageService::CleaupCacheDirectories(uint32_t aVersion, uint32_t aActive)
{
  // Make sure we schedule just once in case CleaupCacheDirectories gets called
  // multiple times from some reason.
  static bool runOnce = CleaupCacheDirectoriesRunnable::Post(aVersion, aActive);
  if (!runOnce) {
    NS_WARNING("Could not start cache trashes cleanup");
  }
}

// Helper methods

// static
bool CacheStorageService::IsOnManagementThread()
{
  nsRefPtr<CacheStorageService> service = Self();
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
  nsRefPtr<CacheIOThread> cacheIOThread = CacheFileIOManager::IOThread();
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

  nsCOMPtr<nsICacheStorage> storage;
  if (CacheObserver::UseNewCache()) {
    storage = new CacheStorage(aLoadContextInfo, false, false);
  }
  else {
    storage = new _OldStorage(aLoadContextInfo, false, false, false, nullptr);
  }

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

  nsCOMPtr<nsICacheStorage> storage;
  if (CacheObserver::UseNewCache()) {
    storage = new CacheStorage(aLoadContextInfo, useDisk, aLookupAppCache);
  }
  else {
    storage = new _OldStorage(aLoadContextInfo, useDisk, aLookupAppCache, false, nullptr);
  }

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
  if (CacheObserver::UseNewCache()) {
    // Using classification since cl believes we want to instantiate this method
    // having the same name as the desired class...
    storage = new mozilla::net::AppCacheStorage(aLoadContextInfo, aApplicationCache);
  }
  else {
    storage = new _OldStorage(aLoadContextInfo, true, false, true, aApplicationCache);
  }

  storage.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::Clear()
{
  nsresult rv;

  if (CacheObserver::UseNewCache()) {
    {
      mozilla::MutexAutoLock lock(mLock);

      NS_ENSURE_TRUE(!mShutdown, NS_ERROR_NOT_INITIALIZED);

      nsTArray<nsCString> keys;
      sGlobalEntryTables->EnumerateRead(&CollectContexts, &keys);

      for (uint32_t i = 0; i < keys.Length(); ++i)
        DoomStorageEntries(keys[i], nullptr, true, nullptr);
    }

    rv = CacheFileIOManager::EvictAll();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = serv->EvictEntries(nsICache::STORE_ANYWHERE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

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

NS_IMETHODIMP CacheStorageService::AsyncGetDiskConsumption(
  nsICacheStorageConsumptionObserver* aObserver)
{
  NS_ENSURE_ARG(aObserver);

  nsresult rv;

  if (CacheObserver::UseNewCache()) {
    rv = CacheIndex::AsyncGetDiskConsumption(aObserver);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = _OldGetDiskConsumption::Get(aObserver);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::GetIoTarget(nsIEventTarget** aEventTarget)
{
  NS_ENSURE_ARG(aEventTarget);

  if (CacheObserver::UseNewCache()) {
    nsCOMPtr<nsIEventTarget> ioTarget = CacheFileIOManager::IOTarget();
    ioTarget.forget(aEventTarget);
  }
  else {
    nsresult rv;

    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = serv->GetCacheIOTarget(aEventTarget);
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
  pool.mFrecencyArray.InsertElementSorted(aEntry, FrecencyComparator());
  pool.mExpirationArray.InsertElementSorted(aEntry, ExpirationComparator());

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
              nsCString const& aKey,
              CacheEntry* aEntry,
              bool aOverwrite)
{
  nsRefPtr<CacheEntry> existingEntry;
  if (!aOverwrite && aEntries->Get(aKey, getter_AddRefs(existingEntry))) {
    bool equals = existingEntry == aEntry;
    LOG(("AddExactEntry [entry=%p equals=%d]", aEntry, equals));
    return equals; // Already there...
  }

  LOG(("AddExactEntry [entry=%p put]", aEntry));
  aEntries->Put(aKey, aEntry);
  return true;
}

static bool
RemoveExactEntry(CacheEntryTable* aEntries,
                 nsCString const& aKey,
                 CacheEntry* aEntry,
                 bool aOverwrite)
{
  nsRefPtr<CacheEntry> existingEntry;
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

    if (!aEntry->IsUsingDisk() && IsForcedValidEntry(entryKey)) {
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
bool CacheStorageService::IsForcedValidEntry(nsACString &aCacheEntryKey)
{
  mozilla::MutexAutoLock lock(mForcedValidEntriesLock);

  TimeStamp validUntil;

  if (!mForcedValidEntries.Get(aCacheEntryKey, &validUntil)) {
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
  mForcedValidEntries.Remove(aCacheEntryKey);
  return false;
}

// Allows a cache entry to be loaded directly from cache without further
// validation - see nsICacheEntry.idl for further details
void CacheStorageService::ForceEntryValidFor(nsACString &aCacheEntryKey,
                                             uint32_t aSecondsToTheFuture)
{
  mozilla::MutexAutoLock lock(mForcedValidEntriesLock);

  TimeStamp now = TimeStamp::NowLoRes();
  ForcedValidEntriesPrune(now);

  // This will be the timeout
  TimeStamp validUntil = now + TimeDuration::FromSeconds(aSecondsToTheFuture);

  mForcedValidEntries.Put(aCacheEntryKey, validUntil);
}

namespace {

PLDHashOperator PruneForcedValidEntries(
  const nsACString& aKey, TimeStamp& aTimeStamp, void* aClosure)
{
  TimeStamp* now = static_cast<TimeStamp*>(aClosure);
  if (aTimeStamp < *now) {
    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

} // namespace

// Cleans out the old entries in mForcedValidEntries
void CacheStorageService::ForcedValidEntriesPrune(TimeStamp &now)
{
  static TimeDuration const oneMinute = TimeDuration::FromSeconds(60);
  static TimeStamp dontPruneUntil = now + oneMinute;
  if (now < dontPruneUntil)
    return;

  mForcedValidEntries.Enumerate(PruneForcedValidEntries, &now);
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
  nsRefPtr<nsIEventTarget> cacheIOTarget = Thread();
  if (!cacheIOTarget)
    return;

  // Dispatch as a priority task, we want to set the purge timer
  // ASAP to prevent vain redispatch of this event.
  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &CacheStorageService::SchedulePurgeOverMemoryLimit);
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
  mozilla::MutexAutoLock lock(mLock);

  if (mPurgeTimer)
    return;

  mPurgeTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (mPurgeTimer)
    mPurgeTimer->InitWithCallback(this, 1000, nsITimer::TYPE_ONE_SHOT);
}

NS_IMETHODIMP
CacheStorageService::Notify(nsITimer* aTimer)
{
  if (aTimer == mPurgeTimer) {
    mPurgeTimer = nullptr;

    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &CacheStorageService::PurgeOverMemoryLimit);
    Dispatch(event);
  }

  return NS_OK;
}

void
CacheStorageService::PurgeOverMemoryLimit()
{
  MOZ_ASSERT(IsOnManagementThread());

  LOG(("CacheStorageService::PurgeOverMemoryLimit"));

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

    nsRefPtr<CacheEntry> entry = mExpirationArray[i];

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

    nsRefPtr<CacheEntry> entry = mFrecencyArray[i];

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

    nsRefPtr<CacheEntry> entry = mFrecencyArray[i];

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
                                     nsIURI* aURI,
                                     const nsACString & aIdExtension,
                                     bool aCreateIfNotExist,
                                     bool aReplace,
                                     CacheEntryHandle** aResult)
{
  NS_ENSURE_FALSE(mShutdown, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG(aStorage);

  nsAutoCString contextKey;
  CacheFileUtils::AppendKeyPrefix(aStorage->LoadInfo(), contextKey);

  return AddStorageEntry(contextKey, aURI, aIdExtension,
                         aStorage->WriteToDisk(), aCreateIfNotExist, aReplace,
                         aResult);
}

nsresult
CacheStorageService::AddStorageEntry(nsCSubstring const& aContextKey,
                                     nsIURI* aURI,
                                     const nsACString & aIdExtension,
                                     bool aWriteToDisk,
                                     bool aCreateIfNotExist,
                                     bool aReplace,
                                     CacheEntryHandle** aResult)
{
  NS_ENSURE_ARG(aURI);

  nsresult rv;

  nsAutoCString entryKey;
  rv = CacheEntry::HashingKey(EmptyCString(), aIdExtension, aURI, entryKey);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("CacheStorageService::AddStorageEntry [entryKey=%s, contextKey=%s]",
    entryKey.get(), aContextKey.BeginReading()));

  nsRefPtr<CacheEntry> entry;
  nsRefPtr<CacheEntryHandle> handle;

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
      // check whether the file is already doomed or we want to turn this entry
      // to a memory-only.
      if (MOZ_UNLIKELY(entry->IsFileDoomed())) {
        LOG(("  file already doomed, replacing the entry"));
        aReplace = true;
      } else if (MOZ_UNLIKELY(!aWriteToDisk) && MOZ_LIKELY(entry->IsUsingDisk())) {
        LOG(("  entry is persistnet but we want mem-only, replacing it"));
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
    }

    // Ensure entry for the particular URL, if not read/only
    if (!entryExists && (aCreateIfNotExist || aReplace)) {
      // Entry is not in the hashtable or has just been truncated...
      entry = new CacheEntry(aContextKey, aURI, aIdExtension, aWriteToDisk);
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
                                       nsIURI* aURI,
                                       const nsACString & aIdExtension,
                                       bool* aResult)
{
  nsresult rv;

  nsAutoCString contextKey;
  CacheFileUtils::AppendKeyPrefix(aStorage->LoadInfo(), contextKey);

  if (!aStorage->WriteToDisk()) {
    AppendMemoryStorageID(contextKey);
  }

  if (LOG_ENABLED()) {
    nsAutoCString uriSpec;
    aURI->GetAsciiSpec(uriSpec);
    LOG(("CacheStorageService::CheckStorageEntry [uri=%s, eid=%s, contextKey=%s]",
      uriSpec.get(), aIdExtension.BeginReading(), contextKey.get()));
  }

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
    LOG(("  index doesn't know, rv=0x%08x", rv));
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResult = status == CacheIndex::EXISTS;
  LOG(("  %sfound in index", *aResult ? "" : "not "));
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
    : mCallback(aCallback) { }

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
    ProxyReleaseMainThread(mCallback);
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
                                      nsIURI *aURI,
                                      const nsACString & aIdExtension,
                                      nsICacheEntryDoomCallback* aCallback)
{
  LOG(("CacheStorageService::DoomStorageEntry"));

  NS_ENSURE_ARG(aStorage);
  NS_ENSURE_ARG(aURI);

  nsAutoCString contextKey;
  CacheFileUtils::AppendKeyPrefix(aStorage->LoadInfo(), contextKey);

  nsAutoCString entryKey;
  nsresult rv = CacheEntry::HashingKey(EmptyCString(), aIdExtension, aURI, entryKey);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<CacheEntry> entry;
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

    nsRefPtr<CacheEntryDoomByKeyCallback> callback(
      new CacheEntryDoomByKeyCallback(aCallback));
    rv = CacheFileIOManager::DoomFileByKey(entryKey, callback);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  class Callback : public nsRunnable
  {
  public:
    explicit Callback(nsICacheEntryDoomCallback* aCallback) : mCallback(aCallback) { }
    NS_IMETHODIMP Run()
    {
      mCallback->OnCacheEntryDoomed(NS_ERROR_NOT_AVAILABLE);
      return NS_OK;
    }
    nsCOMPtr<nsICacheEntryDoomCallback> mCallback;
  };

  if (aCallback) {
    nsRefPtr<nsRunnable> callback = new Callback(aCallback);
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
                            aStorage->WriteToDisk(), aCallback);
}

nsresult
CacheStorageService::DoomStorageEntries(nsCSubstring const& aContextKey,
                                        nsILoadContextInfo* aContext,
                                        bool aDiskStorage,
                                        nsICacheEntryDoomCallback* aCallback)
{
  mLock.AssertCurrentThreadOwns();

  NS_ENSURE_TRUE(!mShutdown, NS_ERROR_NOT_INITIALIZED);

  nsAutoCString memoryStorageID(aContextKey);
  AppendMemoryStorageID(memoryStorageID);

  if (aDiskStorage) {
    LOG(("  dooming disk+memory storage of %s", aContextKey.BeginReading()));

    // Just remove all entries, CacheFileIOManager will take care of the files.
    sGlobalEntryTables->Remove(aContextKey);
    sGlobalEntryTables->Remove(memoryStorageID);

    if (aContext && !aContext->IsPrivate()) {
      LOG(("  dooming disk entries"));
      CacheFileIOManager::EvictByContext(aContext);
    }
  } else {
    LOG(("  dooming memory-only storage of %s", aContextKey.BeginReading()));

    class MemoryEntriesRemoval {
    public:
      static PLDHashOperator EvictEntry(const nsACString& aKey,
                                        CacheEntry* aEntry,
                                        void* aClosure)
      {
        CacheEntryTable* entries = static_cast<CacheEntryTable*>(aClosure);
        nsCString key(aKey);
        RemoveExactEntry(entries, key, aEntry, false);
        return PL_DHASH_NEXT;
      }
    };

    // Remove the memory entries table from the global tables.
    // Since we store memory entries also in the disk entries table
    // we need to remove the memory entries from the disk table one
    // by one manually.
    nsAutoPtr<CacheEntryTable> memoryEntries;
    sGlobalEntryTables->RemoveAndForget(memoryStorageID, memoryEntries);

    CacheEntryTable* entries;
    sGlobalEntryTables->Get(aContextKey, &entries);
    if (memoryEntries && entries)
      memoryEntries->EnumerateRead(&MemoryEntriesRemoval::EvictEntry, entries);
  }

  // An artificial callback.  This is a candidate for removal tho.  In the new
  // cache any 'doom' or 'evict' function ensures that the entry or entries
  // being doomed is/are not accessible after the function returns.  So there is
  // probably no need for a callback - has no meaning.  But for compatibility
  // with the old cache that is still in the tree we keep the API similar to be
  // able to make tests as well as other consumers work for now.
  class Callback : public nsRunnable
  {
  public:
    explicit Callback(nsICacheEntryDoomCallback* aCallback) : mCallback(aCallback) { }
    NS_IMETHODIMP Run()
    {
      mCallback->OnCacheEntryDoomed(NS_OK);
      return NS_OK;
    }
    nsCOMPtr<nsICacheEntryDoomCallback> mCallback;
  };

  if (aCallback) {
    nsRefPtr<nsRunnable> callback = new Callback(aCallback);
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
    nsRefPtr<WalkDiskCacheRunnable> event =
      new WalkDiskCacheRunnable(aStorage->LoadInfo(), aVisitEntries, aVisitor);
    return event->Walk();
  }

  nsRefPtr<WalkMemoryCacheRunnable> event =
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

  if (mShutdown)
    return;

  CacheEntryTable* entries;
  if (!sGlobalEntryTables->Get(contextKey, &entries))
    return;

  nsRefPtr<CacheEntry> entry;
  if (!entries->Get(entryKey, getter_AddRefs(entry)))
    return;

  if (!entry->IsFileDoomed())
    return;

  if (entry->IsReferenced())
    return;

  // Need to remove under the lock to avoid possible race leading
  // to duplication of the entry per its key.
  RemoveExactEntry(entries, entryKey, entry, false);
  entry->DoomAlreadyRemoved();
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

  nsRefPtr<CacheEntry> entry;
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
  nsIURI* uri = aEntry->GetURI();
  nsAutoCString uriSpec;
  if (uri) {
    uri->GetAsciiSpec(uriSpec);
  }

  nsCString const enhanceId = aEntry->GetEnhanceID();
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
                         fetchCount, lastModified, expirationTime);
}

// Telementry collection

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

PLDHashOperator PrunePurgeTimeStamps(
  const nsACString& aKey, TimeStamp& aTimeStamp, void* aClosure)
{
  TimeStamp* now = static_cast<TimeStamp*>(aClosure);
  static TimeDuration const fifteenMinutes = TimeDuration::FromSeconds(900);

  if (*now - aTimeStamp > fifteenMinutes) {
    // We are not interested in resurrection of entries after 15 minutes
    // of time.  This is also the limit for the telemetry.
    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

} // namespace

void
CacheStorageService::TelemetryPrune(TimeStamp &now)
{
  static TimeDuration const oneMinute = TimeDuration::FromSeconds(60);
  static TimeStamp dontPruneUntil = now + oneMinute;
  if (now < dontPruneUntil)
    return;

  mPurgeTimeStamps.Enumerate(PrunePurgeTimeStamps, &now);
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
    n += sGlobalEntryTables->SizeOfIncludingThis(nullptr, mallocSizeOf);
  }

  return n;
}

size_t
CacheStorageService::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
}

namespace {

class ReportStorageMemoryData
{
public:
  nsIMemoryReporterCallback *mHandleReport;
  nsISupports *mData;
};

size_t CollectEntryMemory(nsACString const & aKey,
                          nsRefPtr<mozilla::net::CacheEntry> const & aEntry,
                          mozilla::MallocSizeOf mallocSizeOf,
                          void * aClosure)
{
  CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

  CacheEntryTable* aTable = static_cast<CacheEntryTable*>(aClosure);

  size_t n = 0;
  n += aKey.SizeOfExcludingThisIfUnshared(mallocSizeOf);

  // Bypass memory-only entries, those will be reported when iterating
  // the memory only table. Memory-only entries are stored in both ALL_ENTRIES
  // and MEMORY_ONLY hashtables.
  if (aTable->Type() == CacheEntryTable::MEMORY_ONLY || aEntry->IsUsingDisk())
    n += aEntry->SizeOfIncludingThis(mallocSizeOf);

  return n;
}

PLDHashOperator ReportStorageMemory(const nsACString& aKey,
                                    CacheEntryTable* aTable,
                                    void* aClosure)
{
  CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

  size_t size = aTable->SizeOfIncludingThis(&CollectEntryMemory,
                                            CacheStorageService::MallocSizeOf,
                                            aTable);

  ReportStorageMemoryData& data =
    *static_cast<ReportStorageMemoryData*>(aClosure);
  // These key names are not privacy-sensitive.
  data.mHandleReport->Callback(
    EmptyCString(),
    nsPrintfCString("explicit/network/cache2/%s-storage(%s)",
      aTable->Type() == CacheEntryTable::MEMORY_ONLY ? "memory" : "disk",
      aKey.BeginReading()),
    nsIMemoryReporter::KIND_HEAP,
    nsIMemoryReporter::UNITS_BYTES,
    size,
    NS_LITERAL_CSTRING("Memory used by the cache storage."),
    data.mData);

  return PL_DHASH_NEXT;
}

} // namespace

NS_IMETHODIMP
CacheStorageService::CollectReports(nsIMemoryReporterCallback* aHandleReport,
                                    nsISupports* aData, bool aAnonymize)
{
  nsresult rv;

  rv = MOZ_COLLECT_REPORT(
    "explicit/network/cache2/io", KIND_HEAP, UNITS_BYTES,
    CacheFileIOManager::SizeOfIncludingThis(MallocSizeOf),
    "Memory used by the cache IO manager.");
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  rv = MOZ_COLLECT_REPORT(
    "explicit/network/cache2/index", KIND_HEAP, UNITS_BYTES,
    CacheIndex::SizeOfIncludingThis(MallocSizeOf),
    "Memory used by the cache index.");
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  MutexAutoLock lock(mLock);

  // Report the service instance, this doesn't report entries, done lower
  rv = MOZ_COLLECT_REPORT(
    "explicit/network/cache2/service", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(MallocSizeOf),
    "Memory used by the cache storage service.");
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

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
    ReportStorageMemoryData data;
    data.mHandleReport = aHandleReport;
    data.mData = aData;
    sGlobalEntryTables->EnumerateRead(&ReportStorageMemory, &data);
  }

  return NS_OK;
}

} // namespace net
} // namespace mozilla
