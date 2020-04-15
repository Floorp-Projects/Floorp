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
#include "nsTStringHasher.h"  // mozilla::DefaultHasher<nsCString>
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
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"

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

namespace scache {

struct StartupCacheEntry {
  UniquePtr<char[]> mData;
  uint32_t mOffset;
  uint32_t mCompressedSize;
  uint32_t mUncompressedSize;
  int32_t mHeaderOffsetInFile;
  int32_t mRequestedOrder;
  bool mRequested;

  MOZ_IMPLICIT StartupCacheEntry(uint32_t aOffset, uint32_t aCompressedSize,
                                 uint32_t aUncompressedSize)
      : mData(nullptr),
        mOffset(aOffset),
        mCompressedSize(aCompressedSize),
        mUncompressedSize(aUncompressedSize),
        mHeaderOffsetInFile(0),
        mRequestedOrder(0),
        mRequested(false) {}

  StartupCacheEntry(UniquePtr<char[]> aData, size_t aLength,
                    int32_t aRequestedOrder)
      : mData(std::move(aData)),
        mOffset(0),
        mCompressedSize(0),
        mUncompressedSize(aLength),
        mHeaderOffsetInFile(0),
        mRequestedOrder(0),
        mRequested(true) {}

  struct Comparator {
    using Value = std::pair<const nsCString*, StartupCacheEntry*>;

    bool Equals(const Value& a, const Value& b) const {
      return a.second->mRequestedOrder == b.second->mRequestedOrder;
    }

    bool LessThan(const Value& a, const Value& b) const {
      return a.second->mRequestedOrder < b.second->mRequestedOrder;
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

class StartupCache : public nsIMemoryReporter {
  friend class StartupCacheListener;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  // StartupCache methods. See above comments for a more detailed description.

  // true if the archive has an entry for the buffer or not.
  bool HasEntry(const char* id);

  // Returns a buffer that was previously stored, caller does not take ownership
  nsresult GetBuffer(const char* id, const char** outbuf, uint32_t* length);

  // Stores a buffer. Caller yields ownership.
  nsresult PutBuffer(const char* id, UniquePtr<char[]>&& inbuf,
                     uint32_t length);

  // Removes the cache file.
  void InvalidateCache(bool memoryOnly = false);

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

  static StartupCache* GetSingletonNoInit();
  static StartupCache* GetSingleton();
  static void DeleteSingleton();

  // This measures all the heap memory used by the StartupCache, i.e. it
  // excludes the mapping.
  size_t HeapSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  bool ShouldCompactCache();
  nsresult ResetStartupWriteTimerCheckingReadCount();
  nsresult ResetStartupWriteTimer();
  bool StartupWriteComplete();

 private:
  StartupCache();
  virtual ~StartupCache();

  friend class StartupCacheInfo;

  Result<Ok, nsresult> LoadArchive();
  nsresult Init();

  // Returns a file pointer for the cache file with the given name in the
  // current profile.
  Result<nsCOMPtr<nsIFile>, nsresult> GetCacheFile(const nsAString& suffix);

  // Opens the cache file for reading.
  Result<Ok, nsresult> OpenCache();

  // Writes the cache to disk
  Result<Ok, nsresult> WriteToDisk();

  void WaitOnPrefetchThread();
  void StartPrefetchMemoryThread();

  static nsresult InitSingleton();
  static void WriteTimeout(nsITimer* aTimer, void* aClosure);
  void MaybeWriteOffMainThread();
  static void ThreadedPrefetch(void* aClosure);

  HashMap<nsCString, StartupCacheEntry> mTable;
  // owns references to the contents of tables which have been invalidated.
  // In theory grows forever if the cache is continually filled and then
  // invalidated, but this should not happen in practice.
  nsTArray<decltype(mTable)> mOldTables;
  nsCOMPtr<nsIFile> mFile;
  loader::AutoMemMap mCacheData;
  Mutex mTableLock;

  nsCOMPtr<nsIObserverService> mObserverService;
  RefPtr<StartupCacheListener> mListener;
  nsCOMPtr<nsITimer> mTimer;

  Atomic<bool> mDirty;
  Atomic<bool> mWrittenOnce;
  bool mStartupWriteInitiated;
  bool mCurTableReferenced;
  uint32_t mRequestedCount;
  size_t mCacheEntriesBaseOffset;

  static StaticRefPtr<StartupCache> gStartupCache;
  static bool gShutdownInitiated;
  static bool gIgnoreDiskCache;
  static bool gFoundDiskCacheOnInit;
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
