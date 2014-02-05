/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheStorageService__h__
#define CacheStorageService__h__

#include "nsICacheStorageService.h"

#include "nsClassHashtable.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "mozilla/Mutex.h"
#include "mozilla/Atomics.h"
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
  uint32_t mReportedMemoryConsumption;
protected:
  CacheMemoryConsumer();
  void DoMemoryReport(uint32_t aCurrentSize);
};

class CacheStorageService : public nsICacheStorageService
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHESTORAGESERVICE

  CacheStorageService();

  void Shutdown();
  void DropPrivateBrowsingEntries();

  // Wipes out the new or the old cache directory completely.
  static void WipeCacheDirectory(uint32_t aVersion);

  static CacheStorageService* Self() { return sSelf; }
  nsresult Dispatch(nsIRunnable* aEvent);
  static bool IsRunning() { return sSelf && !sSelf->mShutdown; }
  static bool IsOnManagementThread();
  already_AddRefed<nsIEventTarget> Thread() const;
  mozilla::Mutex& Lock() { return mLock; }

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
  friend class CacheMemoryConsumer;

  /**
   * When memory consumption of this entry radically changes, this method
   * is called to reflect the size of allocated memory.  This call may purge
   * unspecified number of entries from memory (but not from disk).
   */
  void OnMemoryConsumptionChange(CacheMemoryConsumer* aConsumer,
                                 uint32_t aCurrentMemoryConsumption);
  void PurgeOverMemoryLimit();

private:
  class PurgeFromMemoryRunnable : public nsRunnable
  {
  public:
    PurgeFromMemoryRunnable(CacheStorageService* aService, uint32_t aWhat)
      : mService(aService), mWhat(aWhat) { }

  private:
    virtual ~PurgeFromMemoryRunnable() { }

    NS_IMETHOD Run() {
      mService->PurgeAll(mWhat);
      return NS_OK;
    }

    nsRefPtr<CacheStorageService> mService;
    uint32_t mWhat;
  };

  /**
   * Purges entries from memory based on the frecency ordered array.
   */
  void PurgeByFrecency(bool &aFrecencyNeedsSort, uint32_t aWhat);
  void PurgeExpired();
  void PurgeAll(uint32_t aWhat);

  nsresult DoomStorageEntries(nsCSubstring const& aContextKey,
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
  nsTArray<nsRefPtr<CacheEntry> > mFrecencyArray;
  nsTArray<nsRefPtr<CacheEntry> > mExpirationArray;
  mozilla::Atomic<uint32_t> mMemorySize;
  bool mPurging;
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

#endif
