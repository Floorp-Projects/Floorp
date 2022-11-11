/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFile__h__
#define CacheFile__h__

#include "CacheFileChunk.h"
#include "CacheFileIOManager.h"
#include "CacheFileMetadata.h"
#include "nsRefPtrHashtable.h"
#include "nsClassHashtable.h"
#include "mozilla/Mutex.h"

class nsIAsyncOutputStream;
class nsICacheEntry;
class nsICacheEntryMetaDataVisitor;
class nsIInputStream;
class nsIOutputStream;

namespace mozilla {
namespace net {

class CacheFileInputStream;
class CacheFileOutputStream;
class CacheOutputCloseListener;
class MetadataWriteTimer;

namespace CacheFileUtils {
class CacheFileLock;
};

#define CACHEFILELISTENER_IID                        \
  { /* 95e7f284-84ba-48f9-b1fc-3a7336b4c33c */       \
    0x95e7f284, 0x84ba, 0x48f9, {                    \
      0xb1, 0xfc, 0x3a, 0x73, 0x36, 0xb4, 0xc3, 0x3c \
    }                                                \
  }

class CacheFileListener : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(CACHEFILELISTENER_IID)

  NS_IMETHOD OnFileReady(nsresult aResult, bool aIsNew) = 0;
  NS_IMETHOD OnFileDoomed(nsresult aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CacheFileListener, CACHEFILELISTENER_IID)

class CacheFile final : public CacheFileChunkListener,
                        public CacheFileIOListener,
                        public CacheFileMetadataListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CacheFile();

  nsresult Init(const nsACString& aKey, bool aCreateNew, bool aMemoryOnly,
                bool aSkipSizeCheck, bool aPriority, bool aPinned,
                CacheFileListener* aCallback);

  NS_IMETHOD OnChunkRead(nsresult aResult, CacheFileChunk* aChunk) override;
  NS_IMETHOD OnChunkWritten(nsresult aResult, CacheFileChunk* aChunk) override;
  NS_IMETHOD OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                              CacheFileChunk* aChunk) override;
  NS_IMETHOD OnChunkUpdated(CacheFileChunk* aChunk) override;

  NS_IMETHOD OnFileOpened(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnDataWritten(CacheFileHandle* aHandle, const char* aBuf,
                           nsresult aResult) override;
  NS_IMETHOD OnDataRead(CacheFileHandle* aHandle, char* aBuf,
                        nsresult aResult) override;
  NS_IMETHOD OnFileDoomed(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnEOFSet(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnFileRenamed(CacheFileHandle* aHandle, nsresult aResult) override;
  virtual bool IsKilled() override;

  NS_IMETHOD OnMetadataRead(nsresult aResult) override;
  NS_IMETHOD OnMetadataWritten(nsresult aResult) override;

  NS_IMETHOD OpenInputStream(nsICacheEntry* aCacheEntryHandle,
                             nsIInputStream** _retval);
  NS_IMETHOD OpenAlternativeInputStream(nsICacheEntry* aCacheEntryHandle,
                                        const char* aAltDataType,
                                        nsIInputStream** _retval);
  NS_IMETHOD OpenOutputStream(CacheOutputCloseListener* aCloseListener,
                              nsIOutputStream** _retval);
  NS_IMETHOD OpenAlternativeOutputStream(
      CacheOutputCloseListener* aCloseListener, const char* aAltDataType,
      nsIAsyncOutputStream** _retval);
  NS_IMETHOD SetMemoryOnly();
  NS_IMETHOD Doom(CacheFileListener* aCallback);

  void Kill() { mKill = true; }
  nsresult ThrowMemoryCachedData();

  nsresult GetAltDataSize(int64_t* aSize);
  nsresult GetAltDataType(nsACString& aType);

  // metadata forwarders
  nsresult GetElement(const char* aKey, char** _retval);
  nsresult SetElement(const char* aKey, const char* aValue);
  nsresult VisitMetaData(nsICacheEntryMetaDataVisitor* aVisitor);
  nsresult ElementsSize(uint32_t* _retval);
  nsresult SetExpirationTime(uint32_t aExpirationTime);
  nsresult GetExpirationTime(uint32_t* _retval);
  nsresult SetFrecency(uint32_t aFrecency);
  nsresult GetFrecency(uint32_t* _retval);
  nsresult SetNetworkTimes(uint64_t aOnStartTime, uint64_t aOnStopTime);
  nsresult SetContentType(uint8_t aContentType);
  nsresult GetOnStartTime(uint64_t* _retval);
  nsresult GetOnStopTime(uint64_t* _retval);
  nsresult GetLastModified(uint32_t* _retval);
  nsresult GetLastFetched(uint32_t* _retval);
  nsresult GetFetchCount(uint32_t* _retval);
  nsresult GetDiskStorageSizeInKB(uint32_t* aDiskStorageSize);
  // Called by upper layers to indicated the entry has been fetched,
  // i.e. delivered to the consumer.
  nsresult OnFetched();

  bool DataSize(int64_t* aSize);
  void Key(nsACString& aKey);
  bool IsDoomed();
  bool IsPinned();
  // Returns true when there is a potentially unfinished write operation.
  bool IsWriteInProgress();
  bool EntryWouldExceedLimit(int64_t aOffset, int64_t aSize, bool aIsAltData);

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  friend class CacheFileIOManager;
  friend class CacheFileChunk;
  friend class CacheFileInputStream;
  friend class CacheFileOutputStream;
  friend class CacheFileAutoLock;
  friend class MetadataWriteTimer;

  virtual ~CacheFile();

  MOZ_PUSH_IGNORE_THREAD_SAFETY
  void Lock() { mLock->Lock().Lock(); }
  void Unlock() {
    // move the elements out of mObjsToRelease
    // so that they can be released after we unlock
    nsTArray<RefPtr<nsISupports>> objs = std::move(mObjsToRelease);

    mLock->Lock().Unlock();
  }
  MOZ_POP_THREAD_SAFETY
  void AssertOwnsLock() const { mLock->Lock().AssertCurrentThreadOwns(); }
  void ReleaseOutsideLock(RefPtr<nsISupports> aObject);

  enum ECallerType { READER = 0, WRITER = 1, PRELOADER = 2 };

  nsresult DoomLocked(CacheFileListener* aCallback);

  nsresult GetChunkLocked(uint32_t aIndex, ECallerType aCaller,
                          CacheFileChunkListener* aCallback,
                          CacheFileChunk** _retval);

  void PreloadChunks(uint32_t aIndex);
  bool ShouldCacheChunk(uint32_t aIndex);
  bool MustKeepCachedChunk(uint32_t aIndex);

  nsresult DeactivateChunk(CacheFileChunk* aChunk);
  void RemoveChunkInternal(CacheFileChunk* aChunk, bool aCacheChunk);

  bool OutputStreamExists(bool aAlternativeData);
  // Returns number of bytes that are available and can be read by input stream
  // without waiting for the data. The amount is counted from the start of
  // aIndex chunk and it is guaranteed that this data won't be released by
  // CleanUpCachedChunks().
  int64_t BytesFromChunk(uint32_t aIndex, bool aAlternativeData);
  nsresult Truncate(int64_t aOffset);

  void RemoveInput(CacheFileInputStream* aInput, nsresult aStatus);
  void RemoveOutput(CacheFileOutputStream* aOutput, nsresult aStatus);
  nsresult NotifyChunkListener(CacheFileChunkListener* aCallback,
                               nsIEventTarget* aTarget, nsresult aResult,
                               uint32_t aChunkIdx, CacheFileChunk* aChunk);
  void QueueChunkListener(uint32_t aIndex, CacheFileChunkListener* aCallback);
  nsresult NotifyChunkListeners(uint32_t aIndex, nsresult aResult,
                                CacheFileChunk* aChunk);
  bool HaveChunkListeners(uint32_t aIndex);
  void NotifyListenersAboutOutputRemoval();

  bool IsDirty();
  void WriteMetadataIfNeeded();
  void WriteMetadataIfNeededLocked(bool aFireAndForget = false);
  void PostWriteTimer();

  void CleanUpCachedChunks();

  nsresult PadChunkWithZeroes(uint32_t aChunkIdx);

  void SetError(nsresult aStatus);
  nsresult SetAltMetadata(const char* aAltMetadata);

  nsresult InitIndexEntry();

  bool mOpeningFile{false};
  bool mReady{false};
  bool mMemoryOnly{false};
  bool mSkipSizeCheck{false};
  bool mOpenAsMemoryOnly{false};
  bool mPinned{false};
  bool mPriority{false};
  bool mDataAccessed{false};
  bool mDataIsDirty{false};
  bool mWritingMetadata{false};
  bool mPreloadWithoutInputStreams{true};
  uint32_t mPreloadChunkCount{0};
  nsresult mStatus{NS_OK};
  // Size of the whole data including eventual alternative data represenation.
  int64_t mDataSize{-1};

  // If there is alternative data present, it contains size of the original
  // data, i.e. offset where alternative data starts. Otherwise it is -1.
  int64_t mAltDataOffset{-1};

  nsCString mKey;
  nsCString mAltDataType;  // The type of the saved alt-data. May be empty.

  RefPtr<CacheFileHandle> mHandle;
  RefPtr<CacheFileMetadata> mMetadata;
  nsCOMPtr<CacheFileListener> mListener;
  nsCOMPtr<CacheFileIOListener> mDoomAfterOpenListener;
  Atomic<bool, Relaxed> mKill{false};

  nsRefPtrHashtable<nsUint32HashKey, CacheFileChunk> mChunks;
  nsClassHashtable<nsUint32HashKey, ChunkListeners> mChunkListeners;
  nsRefPtrHashtable<nsUint32HashKey, CacheFileChunk> mCachedChunks;
  // We can truncate data only if there is no input/output stream beyond the
  // truncate position, so only unused chunks can be thrown away. But it can
  // happen that we need to throw away a chunk that is still in mChunks (i.e.
  // an active chunk) because deactivation happens with a small delay. We cannot
  // delete such chunk immediately but we need to ensure that such chunk won't
  // be returned by GetChunkLocked, so we move this chunk into mDiscardedChunks
  // and mark it as discarded.
  nsTArray<RefPtr<CacheFileChunk>> mDiscardedChunks;

  nsTArray<CacheFileInputStream*> mInputs;
  CacheFileOutputStream* mOutput{nullptr};

  nsTArray<RefPtr<nsISupports>> mObjsToRelease;
  RefPtr<CacheFileUtils::CacheFileLock> mLock;
};

class CacheFileAutoLock {
 public:
  explicit CacheFileAutoLock(CacheFile* aFile) : mFile(aFile), mLocked(true) {
    mFile->Lock();
  }
  ~CacheFileAutoLock() {
    if (mLocked) mFile->Unlock();
  }
  void Lock() {
    MOZ_ASSERT(!mLocked);
    mFile->Lock();
    mLocked = true;
  }
  void Unlock() {
    MOZ_ASSERT(mLocked);
    mFile->Unlock();
    mLocked = false;
  }

 private:
  RefPtr<CacheFile> mFile;
  bool mLocked;
};

}  // namespace net
}  // namespace mozilla

#endif
