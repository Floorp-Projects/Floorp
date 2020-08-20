/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StartupCache_h_
#define StartupCache_h_

#include <utility>

#include "nsClassHashtable.h"
#include "nsComponentManagerUtils.h"
#include "nsTArray.h"
#include "nsZipArchive.h"
#include "nsITimer.h"
#include "nsIMemoryReporter.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIObjectOutputStream.h"
#include "nsIFile.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoMemMap.h"
#include "mozilla/Compression.h"
#include "mozilla/EnumSet.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/Omnijar.h"
#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"

/**
 * The StartupCache is a persistent cache of simple key-value pairs,
 * where the keys are null-terminated c-strings and the values are
 * arbitrary data, passed as a (char*, size) tuple.
 *
 * Clients should use the GetSingleton() static method to access the cache. It
 * will be available from the end of XPCOM init (NS_InitXPCOM3 in
 * XPCOMInit.cpp), until XPCOM shutdown begins. The GetSingleton() method will
 * return null if the cache is unavailable. The cache is only provided for
 * libxul builds -- it will fail to link in non-libxul builds. The XPCOM
 * interface is provided only to allow compiled-code tests; clients should avoid
 * using it.
 *
 * The API provided is very simple: GetBuffer() returns a buffer that was
 * previously stored in the cache (if any), and PutBuffer() inserts a buffer
 * into the cache. GetBuffer returns a new buffer, and the caller must take
 * ownership of it. PutBuffer will assert if the client attempts to insert a
 * buffer with the same name as an existing entry. The cache makes a copy of the
 * passed-in buffer, so client retains ownership.
 *
 * InvalidateCache() may be called if a client suspects data corruption
 * or wishes to invalidate for any other reason. This will remove all existing
 * cache data. Additionally, the static method IgnoreDiskCache() can be called
 * if it is believed that the on-disk cache file is itself corrupt. This call
 * implicitly calls InvalidateCache (if the singleton has been initialized) to
 * ensure any data already read from disk is discarded. The cache will not load
 * data from the disk file until a successful write occurs.
 *
 * Finally, getDebugObjectOutputStream() allows debug code to wrap an
 * objectstream with a debug objectstream, to check for multiply-referenced
 * objects. These will generally fail to deserialize correctly, unless they are
 * stateless singletons or the client maintains their own object data map for
 * deserialization.
 *
 * Writes before the final-ui-startup notification are placed in an intermediate
 * cache in memory, then written out to disk at a later time, to get writes off
 * the startup path. In any case, clients should not rely on being able to
 * GetBuffer() data that is written to the cache, since it may not have been
 * written to disk or another client may have invalidated the cache. In other
 * words, it should be used as a cache only, and not a reliable persistent
 * store.
 *
 * Some utility functions are provided in StartupCacheUtils. These functions
 * wrap the buffers into object streams, which may be useful for serializing
 * objects. Note the above caution about multiply-referenced objects, though --
 * the streams are just as 'dumb' as the underlying buffers about
 * multiply-referenced objects. They just provide some convenience in writing
 * out data.
 */

namespace mozilla {
namespace dom {
class ContentParent;
}
namespace ipc {
class GeckoChildProcessHost;
}  // namespace ipc

namespace scache {

class StartupCacheChild;

#ifdef XP_UNIX

// Please see bug 1440207 about improving the problem of random fixed FDs,
// which the addition of the below constant exacerbates.
static const int kStartupCacheFd = 11;
#endif

// We use INT_MAX here just to simplify the sorting - we want to push
// unrequested entries to the back, and have requested entries in the order
// they came in.
static const int kStartupCacheEntryNotRequested = INT_MAX;
static const int kStartupcacheEntryNotInSharedData = -1;

// Keys must be of length `kStartupCacheKeyLengthCap - 1` or shorter, which
// will bring them to `kStartupCacheKeyLengthCap` or shorter with a null
// terminator.
static const int kStartupCacheKeyLengthCap = 1024;

// StartupCache entries can be backed by a buffer which they allocate as
// soon as they are requested, into which they decompress the contents out
// of the memory mapped file, *or* they can be backed by a contiguous buffer
// which we allocate up front and decompress into, in order to share it with
// child processes. This class is a helper class to hold a buffer which the
// entry itself may or may not own.
//
// Side note: it may be appropriate for StartupCache entries to never own
// their underlying buffers. We explicitly work to ensure that anything the
// StartupCache returns to a caller survives for the lifetime of the
// application, so it may be preferable to have a set of large contiguous
// buffers which we allocate on demand, and fill up with cache entry contents,
// but at that point we're basically implementing our own hacky pseudo-malloc,
// for relatively uncertain performance gains. For the time being, we just
// keep the existing model unchanged.
class MaybeOwnedCharPtr {
 private:
  char* mPtr;
  bool mOwned;

 public:
  ~MaybeOwnedCharPtr() {
    if (mOwned) {
      delete[] mPtr;
    }
  }

  // MaybeOwnedCharPtr(const MaybeOwnedCharPtr& other);
  // MaybeOwnedCharPtr& operator=(const MaybeOwnedCharPtr& other);

  MaybeOwnedCharPtr(MaybeOwnedCharPtr&& other)
      : mPtr(std::exchange(other.mPtr, nullptr)),
        mOwned(std::exchange(other.mOwned, false)) {}

  MaybeOwnedCharPtr& operator=(MaybeOwnedCharPtr&& other) {
    std::swap(mPtr, other.mPtr);
    std::swap(mOwned, other.mOwned);
    return *this;
  }

  MaybeOwnedCharPtr& operator=(decltype(nullptr)) {
    if (mOwned) {
      delete[] mPtr;
    }
    mPtr = nullptr;
    mOwned = false;
    return *this;
  }

  operator char*() const { return mPtr; }

  explicit operator bool() const { return !!mPtr; }

  char* get() const { return mPtr; }

  bool IsOwned() const { return mOwned; }

  explicit MaybeOwnedCharPtr(char* aBytes) : mPtr(aBytes), mOwned(false) {}

  explicit MaybeOwnedCharPtr(UniquePtr<char[]>&& aBytes)
      : mPtr(aBytes.release()), mOwned(true) {}

  explicit MaybeOwnedCharPtr(size_t size)
      : mPtr(new char[size]), mOwned(true) {}

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    if (!mOwned) {
      return 0;
    }
    return mallocSizeOf(mPtr);
  }
};

struct StartupCacheKeyHasher {
  using Key = MaybeOwnedCharPtr;
  using Lookup = const char*;

  static HashNumber hash(const Lookup& aLookup) { return HashString(aLookup); }

  static bool match(const Key& aKey, const Lookup& aLookup) {
    return strcmp(aKey.get(), aLookup) == 0;
  }
};

enum class StartupCacheEntryFlags {
  Shared,
  RequestedByChild,
  AddedThisSession,

  // We want to track whether code outside the StartupCache has requested
  // and gotten access to a pointer to this item's underlying buffer, and
  // this flag is the mechanism for doing that.
  DoNotFree,
};

struct StartupCacheEntry {
  MaybeOwnedCharPtr mData;
  uint32_t mOffset;
  uint32_t mCompressedSize;
  uint32_t mUncompressedSize;
  int32_t mSharedDataOffset;
  int32_t mHeaderOffsetInFile;
  int32_t mRequestedOrder;
  EnumSet<StartupCacheEntryFlags> mFlags;

  MOZ_IMPLICIT StartupCacheEntry(uint32_t aOffset, uint32_t aCompressedSize,
                                 uint32_t aUncompressedSize,
                                 EnumSet<StartupCacheEntryFlags> aFlags)
      : mData(nullptr),
        mOffset(aOffset),
        mCompressedSize(aCompressedSize),
        mUncompressedSize(aUncompressedSize),
        mSharedDataOffset(kStartupcacheEntryNotInSharedData),
        mHeaderOffsetInFile(0),
        mRequestedOrder(kStartupCacheEntryNotRequested),
        mFlags(aFlags) {}

  StartupCacheEntry(UniquePtr<char[]> aData, size_t aLength,
                    int32_t aRequestedOrder,
                    EnumSet<StartupCacheEntryFlags> aFlags)
      : mData(std::move(aData)),
        mOffset(0),
        mCompressedSize(0),
        mUncompressedSize(aLength),
        mSharedDataOffset(kStartupcacheEntryNotInSharedData),
        mHeaderOffsetInFile(0),
        mRequestedOrder(aRequestedOrder),
        mFlags(aFlags) {}

  struct Comparator {
    using Value = std::pair<const MaybeOwnedCharPtr*, StartupCacheEntry*>;

    bool Equals(const Value& a, const Value& b) const {
      // This is a bit ugly. Here and below, just note that we want entries
      // with the RequestedByChild flag to be sorted before any other entries,
      // because we're going to want to decompress them and send them down to
      // child processes pretty early during startup.
      return a.second->mFlags.contains(
                 StartupCacheEntryFlags::RequestedByChild) ==
                 b.second->mFlags.contains(
                     StartupCacheEntryFlags::RequestedByChild) &&
             a.second->mRequestedOrder == b.second->mRequestedOrder;
    }

    bool LessThan(const Value& a, const Value& b) const {
      bool requestedByChildA =
          a.second->mFlags.contains(StartupCacheEntryFlags::RequestedByChild);
      bool requestedByChildB =
          b.second->mFlags.contains(StartupCacheEntryFlags::RequestedByChild);
      if (requestedByChildA == requestedByChildB) {
        return a.second->mRequestedOrder < b.second->mRequestedOrder;
      } else {
        return requestedByChildA;
      }
    }
  };
};

// We don't want to refcount StartupCache, and ObserverService wants to
// refcount its listeners, so we'll let it refcount this instead.
class StartupCacheListener final : public nsIObserver {
  ~StartupCacheListener() = default;
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

// This mirrors a bit of logic in the script preloader. Basically, there's
// certainly some overhead in child processes sending us lists of requested
// startup cache items, so we want to limit that. Accordingly, we only
// request to be notified of requested cache items for the first occurrence
// of each process type, enumerated below.
enum class ProcessType : uint8_t {
  Uninitialized,
  Parent,
  Web,
  Extension,
  PrivilegedAbout,
};

class StartupCache : public nsIMemoryReporter {
  friend class StartupCacheListener;
  friend class StartupCacheChild;

 public:
  using Table =
      HashMap<MaybeOwnedCharPtr, StartupCacheEntry, StartupCacheKeyHasher>;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  // StartupCache methods. See above comments for a more detailed description.

  // true if the archive has an entry for the buffer or not.
  bool HasEntry(const char* id);

  // Returns a buffer that was previously stored, caller does not take ownership
  nsresult GetBuffer(const char* id, const char** outbuf, uint32_t* length);

  // Stores a buffer. Caller yields ownership.
  nsresult PutBuffer(const char* id, UniquePtr<char[]>&& inbuf, uint32_t length,
                     bool isFromChildProcess = false);

  void InvalidateCache();

  // For use during shutdown - this will write the startupcache's data
  // to disk if the timer hasn't already gone off.
  void MaybeInitShutdownWrite();

  // For use during shutdown - ensure we complete the shutdown write
  // before shutdown, even in the FastShutdown case.
  void EnsureShutdownWriteComplete();

  // Signal that data should not be loaded from the cache file
  static void IgnoreDiskCache();

  // In DEBUG builds, returns a stream that will attempt to check for
  // and disallow multiple writes of the same object.
  nsresult GetDebugObjectOutputStream(nsIObjectOutputStream* aStream,
                                      nsIObjectOutputStream** outStream);

  static ProcessType GetChildProcessType(const nsACString& remoteType);
  static StartupCache* GetSingleton();

  // This will get the StartupCache up and running to get cached entries, but
  // it won't init some of the deferred things which require later services
  // to be up and running.
  static nsresult PartialInitSingleton(nsIFile* aProfileLocalDir);

  // If the startup cache singleton exists (initialized via
  // PartialInitSingleton), this will ensure that all of the ancillary
  // requirements of the startup cache are met.
  static nsresult FullyInitSingleton();

  static nsresult InitChildSingleton(char* aScacheHandleStr,
                                     char* aScacheSizeStr);

  static void DeleteSingleton();
  static void InitContentChild(dom::ContentParent& parent);

  void AddStartupCacheCmdLineArgs(ipc::GeckoChildProcessHost& procHost,
                                  const nsACString& aRemoteType,
                                  std::vector<std::string>& aExtraOpts);
  nsresult ParseStartupCacheCmdLineArgs(char* aScacheHandleStr,
                                        char* aScacheSizeStr);

  // This measures all the heap memory used by the StartupCache, i.e. it
  // excludes the mapping.
  size_t HeapSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  bool ShouldCompactCache();
  nsresult ResetStartupWriteTimer();
  bool StartupWriteComplete();

 private:
  StartupCache();
  virtual ~StartupCache();

  friend class StartupCacheInfo;

  Result<Ok, nsresult> LoadArchive();
  nsresult PartialInit(nsIFile* aProfileLocalDir);
  nsresult FullyInit();
  nsresult InitChild(StartupCacheChild* cacheChild);

  // Removes the cache file.
  void InvalidateCacheImpl(bool memoryOnly = false);

  nsresult ResetStartupWriteTimerCheckingReadCount();
  nsresult ResetStartupWriteTimerImpl();

  // Returns a file pointer for the cache file with the given name in the
  // current profile.
  Result<nsCOMPtr<nsIFile>, nsresult> GetCacheFile(const nsAString& suffix);

  // Opens the cache file for reading.
  Result<Ok, nsresult> OpenCache();

  // Writes the cache to disk
  Result<Ok, nsresult> WriteToDisk();

  Result<Ok, nsresult> DecompressEntry(StartupCacheEntry& aEntry);

  Result<Ok, nsresult> LoadEntriesOffDisk();

  Result<Ok, nsresult> LoadEntriesFromSharedMemory();

  void WaitOnPrefetchThread();
  void StartPrefetchMemoryThread();

  static void WriteTimeout(nsITimer* aTimer, void* aClosure);
  static void SendEntriesTimeout(nsITimer* aTimer, void* aClosure);
  void MaybeWriteOffMainThread();
  static void ThreadedPrefetch(void* aClosure);

  EnumSet<ProcessType> mInitializedProcesses{};
  nsCString mContentStartupFinishedTopic;

  Table mTable;
  // owns references to the contents of tables which have been invalidated.
  // In theory grows forever if the cache is continually filled and then
  // invalidated, but this should not happen in practice.
  nsTArray<decltype(mTable)> mOldTables;
  nsCOMPtr<nsIFile> mFile;
  loader::AutoMemMap mCacheData;
  loader::AutoMemMap mSharedData;
  UniqueFileHandle mSharedDataHandle;

  // This lock must protect a few members of the StartupCache. Essentially,
  // we want to protect everything accessed by GetBuffer and PutBuffer. This
  // includes:
  // - mTable
  // - mCacheData
  // - mDecompressionContext
  // - mCurTableReferenced
  // - mOldTables
  // - mWrittenOnce
  // - gIgnoreDiskCache
  // - mFile
  // - mWriteTimer
  // - mStartupWriteInitiated
  mutable Mutex mLock;

  nsCOMPtr<nsIObserverService> mObserverService;
  RefPtr<StartupCacheListener> mListener;
  nsCOMPtr<nsITimer> mWriteTimer;
  nsCOMPtr<nsITimer> mSendEntriesTimer;

  Atomic<bool> mDirty;
  Atomic<bool> mWrittenOnce;
  bool mCurTableReferenced;
  bool mLoaded;
  bool mFullyInitialized;
  uint32_t mRequestedCount;
  uint32_t mPrefetchSize;
  uint32_t mSharedDataSize;
  size_t mCacheEntriesBaseOffset;

  static StaticRefPtr<StartupCache> gStartupCache;
  static bool gIgnoreDiskCache;
  static bool gFoundDiskCacheOnInit;

  Atomic<StartupCacheChild*> mChildActor;
  PRThread* mPrefetchThread;
  UniquePtr<Compression::LZ4FrameDecompressionContext> mDecompressionContext;
#ifdef DEBUG
  nsTHashtable<nsISupportsHashKey> mWriteObjectMap;
#endif
};

// This debug outputstream attempts to detect if clients are writing multiple
// references to the same object. We only support that if that object
// is a singleton.
#ifdef DEBUG
class StartupCacheDebugOutputStream final : public nsIObjectOutputStream {
  ~StartupCacheDebugOutputStream() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBJECTOUTPUTSTREAM

  StartupCacheDebugOutputStream(nsIObjectOutputStream* binaryStream,
                                nsTHashtable<nsISupportsHashKey>* objectMap)
      : mBinaryStream(binaryStream), mObjectMap(objectMap) {}

  NS_FORWARD_SAFE_NSIBINARYOUTPUTSTREAM(mBinaryStream)
  NS_FORWARD_SAFE_NSIOUTPUTSTREAM(mBinaryStream)

  bool CheckReferences(nsISupports* aObject);

  nsCOMPtr<nsIObjectOutputStream> mBinaryStream;
  nsTHashtable<nsISupportsHashKey>* mObjectMap;
};
#endif  // DEBUG

}  // namespace scache
}  // namespace mozilla

#endif  // StartupCache_h_
