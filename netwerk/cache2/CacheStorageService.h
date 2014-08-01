/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheStorageService__h__
#define CacheStorageService__h__

#include "nsICacheStorageService.h"
#include "nsIMemoryReporter.h"

#include "nsITimer.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "mozilla/Mutex.h"
#include "mozilla/Atomics.h"
#include "mozilla/TimeStamp.h"
#include "nsTArray.h"

class nsIURI;
class nsICacheEntryOpenCallback;
class nsICacheEntryDoomCallback;
class nsICacheStorageVisitor;
class nsIRunnable;
class nsIThread;
class nsIEventTarget;

namespace mozilla {
namespace net {

class CacheStorageService;
class CacheStorage;
class CacheEntry;
class CacheEntryHandle;
class CacheEntryTable;

class CacheMemoryConsumer
{
private:
  friend class CacheStorageService;
  uint32_t mReportedMemoryConsumption : 30;
  uint32_t mFlags : 2;

private:
  CacheMemoryConsumer() MOZ_DELETE;

protected:
  enum {
    // No special treatment, reports always to the disk-entries pool.
    NORMAL = 0,
    // This consumer is belonging to a memory-only cache entry, used to decide
    // which of the two disk and memory pools count this consumption at.
    MEMORY_ONLY = 1 << 0,
    // Prevent reports of this consumer at all, used for disk data chunks since
    // we throw them away as soon as the entry is not used by any consumer and
    // don't want to make them wipe the whole pool out during their short life.
    DONT_REPORT = 1 << 1
  };

  CacheMemoryConsumer(uint32_t aFlags);
  ~CacheMemoryConsumer() { DoMemoryReport(0); }
  void DoMemoryReport(uint32_t aCurrentSize);
};

class CacheStorageService : public nsICacheStorageService
                          , public nsIMemoryReporter
                          , public nsITimerCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHESTORAGESERVICE
  NS_DECL_NSIMEMORYREPORTER
  NS_DECL_NSITIMERCALLBACK

  CacheStorageService();

  void Shutdown();
  void DropPrivateBrowsingEntries();

  // Takes care of deleting any pending trashes for both cache1 and cache2
  // as well as the cache directory of an inactive cache version when requested.
  static void CleaupCacheDirectories(uint32_t aVersion, uint32_t aActive);

  static CacheStorageService* Self() { return sSelf; }
  static nsISupports* SelfISupports() { return static_cast<nsICacheStorageService*>(Self()); }
  nsresult Dispatch(nsIRunnable* aEvent);
  static bool IsRunning() { return sSelf && !sSelf->mShutdown; }
  static bool IsOnManagementThread();
  already_AddRefed<nsIEventTarget> Thread() const;
  mozilla::Mutex& Lock() { return mLock; }

  // Tracks entries that may be forced valid in a pruned hashtable.
  nsDataHashtable<nsCStringHashKey, TimeStamp> mForcedValidEntries;
  void ForcedValidEntriesPrune(TimeStamp &now);

  // Helper thread-safe interface to pass entry info, only difference from
  // nsICacheStorageVisitor is that instead of nsIURI only the uri spec is
  // passed.
  class EntryInfoCallback {
  public:
    virtual void OnEntryInfo(const nsACString & aURISpec, const nsACString & aIdEnhance,
                             int64_t aDataSize, int32_t aFetchCount,
                             uint32_t aLastModifiedTime, uint32_t aExpirationTime) = 0;
  };

  // Invokes OnEntryInfo for the given aEntry, synchronously.
  static void GetCacheEntryInfo(CacheEntry* aEntry, EntryInfoCallback *aVisitor);

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

private:
  virtual ~CacheStorageService();
  void ShutdownBackground();

private:
  // The following methods may only be called on the management
  // thread.
  friend class CacheEntry;

  /**
   * Registers the entry in management ordered arrays, a mechanism
   * helping with weighted purge of entries.
   * Management arrays keep hard reference to the entry.  Entry is
   * responsible to remove it self or the service is responsible to
   * remove the entry when it's no longer needed.
   */
  void RegisterEntry(CacheEntry* aEntry);

  /**
   * Deregisters the entry from management arrays.  References are
   * then released.
   */
  void UnregisterEntry(CacheEntry* aEntry);

  /**
   * Removes the entry from the related entry hash table, if still present.
   */
  bool RemoveEntry(CacheEntry* aEntry, bool aOnlyUnreferenced = false);

  /**
   * Tells the storage service whether this entry is only to be stored in
   * memory.
   */
  void RecordMemoryOnlyEntry(CacheEntry* aEntry,
                             bool aOnlyInMemory,
                             bool aOverwrite);

  /**
   * Sets a cache entry valid (overrides the default loading behavior by loading
   * directly from cache) for the given number of seconds
   * See nsICacheEntry.idl for more details
   */
  void ForceEntryValidFor(nsACString &aCacheEntryKey,
                          uint32_t aSecondsToTheFuture);

private:
  friend class CacheIndex;

  /**
   * Gets the mutex lock for CacheStorageService then calls through to
   * IsForcedValidEntryInternal. See below for details.
   */
  bool IsForcedValidEntry(nsACString &aCacheEntryKey);

  /**
   * Retrieves the status of the cache entry to see if it has been forced valid
   * (so it will loaded directly from cache without further validation)
   * CacheIndex uses this to prevent a cache entry from being prememptively
   * thrown away when forced valid
   * See nsICacheEntry.idl for more details
   */
  bool IsForcedValidEntryInternal(nsACString &aCacheEntryKey);

private:
  // These are helpers for telemetry monitorying of the memory pools.
  void TelemetryPrune(TimeStamp &now);
  void TelemetryRecordEntryCreation(CacheEntry const* entry);
  void TelemetryRecordEntryRemoval(CacheEntry const* entry);

private:
  // Following methods are thread safe to call.
  friend class CacheStorage;

  /**
   * Get, or create when not existing and demanded, an entry for the storage
   * and uri+id extension.
   */
  nsresult AddStorageEntry(CacheStorage const* aStorage,
                           nsIURI* aURI,
                           const nsACString & aIdExtension,
                           bool aCreateIfNotExist,
                           bool aReplace,
                           CacheEntryHandle** aResult);

  /**
   * Check existance of an entry.  This may throw NS_ERROR_NOT_AVAILABLE
   * when the information cannot be obtained synchronously w/o blocking.
   */
  nsresult CheckStorageEntry(CacheStorage const* aStorage,
                             nsIURI* aURI,
                             const nsACString & aIdExtension,
                             bool* aResult);

  /**
   * Removes the entry from the related entry hash table, if still present
   * and returns it.
   */
  nsresult DoomStorageEntry(CacheStorage const* aStorage,
                            nsIURI* aURI,
                            const nsACString & aIdExtension,
                            nsICacheEntryDoomCallback* aCallback);

  /**
   * Removes and returns entry table for the storage.
   */
  nsresult DoomStorageEntries(CacheStorage const* aStorage,
                              nsICacheEntryDoomCallback* aCallback);

  /**
   * Walk all entiries beloging to the storage.
   */
  nsresult WalkStorageEntries(CacheStorage const* aStorage,
                              bool aVisitEntries,
                              nsICacheStorageVisitor* aVisitor);

private:
  friend class CacheFileIOManager;

  /**
   * CacheFileIOManager uses this method to notify CacheStorageService that
   * an active entry was removed. This method is called even if the entry
   * removal was originated by CacheStorageService.
   */
  void CacheFileDoomed(nsILoadContextInfo* aLoadContextInfo,
                       const nsACString & aIdExtension,
                       const nsACString & aURISpec);

  /**
   * Tries to find an existing entry in the hashtables and synchronously call
   * OnCacheEntryInfo of the aVisitor callback when found.
   * @retuns
   *   true, when the entry has been found that also implies the callbacks has
   *        beem invoked
   *   false, when an entry has not been found
   */
  bool GetCacheEntryInfo(nsILoadContextInfo* aLoadContextInfo,
                         const nsACString & aIdExtension,
                         const nsACString & aURISpec,
                         EntryInfoCallback *aCallback);

private:
  friend class CacheMemoryConsumer;

  /**
   * When memory consumption of this entry radically changes, this method
   * is called to reflect the size of allocated memory.  This call may purge
   * unspecified number of entries from memory (but not from disk).
   */
  void OnMemoryConsumptionChange(CacheMemoryConsumer* aConsumer,
                                 uint32_t aCurrentMemoryConsumption);

  /**
   * If not already pending, it schedules mPurgeTimer that fires after 1 second
   * and dispatches PurgeOverMemoryLimit().
   */
  void SchedulePurgeOverMemoryLimit();

  /**
   * Called on the management thread, removes all expired and then least used
   * entries from the memory, first from the disk pool and then from the memory
   * pool.
   */
  void PurgeOverMemoryLimit();

private:
  nsresult DoomStorageEntries(nsCSubstring const& aContextKey,
                              nsILoadContextInfo* aContext,
                              bool aDiskStorage,
                              nsICacheEntryDoomCallback* aCallback);
  nsresult AddStorageEntry(nsCSubstring const& aContextKey,
                           nsIURI* aURI,
                           const nsACString & aIdExtension,
                           bool aWriteToDisk,
                           bool aCreateIfNotExist,
                           bool aReplace,
                           CacheEntryHandle** aResult);

  static CacheStorageService* sSelf;

  mozilla::Mutex mLock;

  bool mShutdown;

  // Accessible only on the service thread
  class MemoryPool
  {
  public:
    enum EType
    {
      DISK,
      MEMORY,
    } mType;

    MemoryPool(EType aType);
    ~MemoryPool();

    nsTArray<nsRefPtr<CacheEntry> > mFrecencyArray;
    nsTArray<nsRefPtr<CacheEntry> > mExpirationArray;
    mozilla::Atomic<uint32_t> mMemorySize;

    bool OnMemoryConsumptionChange(uint32_t aSavedMemorySize,
                                   uint32_t aCurrentMemoryConsumption);
    /**
     * Purges entries from memory based on the frecency ordered array.
     */
    void PurgeOverMemoryLimit();
    void PurgeExpired();
    void PurgeByFrecency(bool &aFrecencyNeedsSort, uint32_t aWhat);
    void PurgeAll(uint32_t aWhat);

  private:
    uint32_t const Limit() const;
    MemoryPool() MOZ_DELETE;
  };

  MemoryPool mDiskPool;
  MemoryPool mMemoryPool;
  MemoryPool& Pool(bool aUsingDisk)
  {
    return aUsingDisk ? mDiskPool : mMemoryPool;
  }
  MemoryPool const& Pool(bool aUsingDisk) const
  {
    return aUsingDisk ? mDiskPool : mMemoryPool;
  }

  nsCOMPtr<nsITimer> mPurgeTimer;

  class PurgeFromMemoryRunnable : public nsRunnable
  {
  public:
    PurgeFromMemoryRunnable(CacheStorageService* aService, uint32_t aWhat)
      : mService(aService), mWhat(aWhat) { }

  private:
    virtual ~PurgeFromMemoryRunnable() { }

    NS_IMETHOD Run()
    {
      // TODO not all flags apply to both pools
      mService->Pool(true).PurgeAll(mWhat);
      mService->Pool(false).PurgeAll(mWhat);
      return NS_OK;
    }

    nsRefPtr<CacheStorageService> mService;
    uint32_t mWhat;
  };

  // Used just for telemetry purposes, accessed only on the management thread.
  // Note: not included in the memory reporter, this is not expected to be huge
  // and also would be complicated to report since reporting happens on the main
  // thread but this table is manipulated on the management thread.
  nsDataHashtable<nsCStringHashKey, mozilla::TimeStamp> mPurgeTimeStamps;
};

template<class T>
void ProxyRelease(nsCOMPtr<T> &object, nsIThread* thread)
{
  T* release;
  object.forget(&release);

  NS_ProxyRelease(thread, release);
}

template<class T>
void ProxyReleaseMainThread(nsCOMPtr<T> &object)
{
  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  ProxyRelease(object, mainThread);
}

} // net
} // mozilla

#define NS_CACHE_STORAGE_SERVICE_CID \
  { 0xea70b098, 0x5014, 0x4e21, \
  { 0xae, 0xe1, 0x75, 0xe6, 0xb2, 0xc4, 0xb8, 0xe0 } } \

#define NS_CACHE_STORAGE_SERVICE_CONTRACTID \
  "@mozilla.org/netwerk/cache-storage-service;1"

#define NS_CACHE_STORAGE_SERVICE_CONTRACTID2 \
  "@mozilla.org/network/cache-storage-service;1"

#endif
