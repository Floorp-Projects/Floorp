/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileChunk__h__
#define CacheFileChunk__h__

#include "CacheFileIOManager.h"
#include "CacheStorageService.h"
#include "CacheHashUtils.h"
#include "CacheFileUtils.h"
#include "nsAutoPtr.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace net {

#define kChunkSize (256 * 1024)
#define kEmptyChunkHash 0x1826

class CacheFileChunk;
class CacheFile;

class CacheFileChunkBuffer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheFileChunkBuffer)

  explicit CacheFileChunkBuffer(CacheFileChunk* aChunk);

  nsresult EnsureBufSize(uint32_t aSize);
  void CopyFrom(CacheFileChunkBuffer* aOther);
  nsresult FillInvalidRanges(CacheFileChunkBuffer* aOther,
                             CacheFileUtils::ValidityMap* aMap);
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  char* Buf() const { return mBuf; }
  void SetDataSize(uint32_t aDataSize);
  uint32_t DataSize() const { return mDataSize; }
  uint32_t ReadHandlesCount() const { return mReadHandlesCount; }
  bool WriteHandleExists() const { return mWriteHandleExists; }

 private:
  friend class CacheFileChunkHandle;
  friend class CacheFileChunkReadHandle;
  friend class CacheFileChunkWriteHandle;

  ~CacheFileChunkBuffer();

  void AssertOwnsLock() const;

  void RemoveReadHandle();
  void RemoveWriteHandle();

  // We keep a weak reference to the chunk to not create a reference cycle. The
  // buffer is referenced only by chunk and handles. Handles are always
  // destroyed before the chunk so it is guaranteed that mChunk is a valid
  // pointer for the whole buffer's lifetime.
  CacheFileChunk* mChunk;
  char* mBuf;
  uint32_t mBufSize;
  uint32_t mDataSize;
  uint32_t mReadHandlesCount;
  bool mWriteHandleExists;
};

class CacheFileChunkHandle {
 public:
  uint32_t DataSize();
  uint32_t Offset();

 protected:
  RefPtr<CacheFileChunkBuffer> mBuf;
};

class CacheFileChunkReadHandle : public CacheFileChunkHandle {
 public:
  explicit CacheFileChunkReadHandle(CacheFileChunkBuffer* aBuf);
  ~CacheFileChunkReadHandle();

  const char* Buf();
};

class CacheFileChunkWriteHandle : public CacheFileChunkHandle {
 public:
  explicit CacheFileChunkWriteHandle(CacheFileChunkBuffer* aBuf);
  ~CacheFileChunkWriteHandle();

  char* Buf();
  void UpdateDataSize(uint32_t aOffset, uint32_t aLen);
};

#define CACHEFILECHUNKLISTENER_IID                   \
  { /* baf16149-2ab5-499c-a9c2-5904eb95c288 */       \
    0xbaf16149, 0x2ab5, 0x499c, {                    \
      0xa9, 0xc2, 0x59, 0x04, 0xeb, 0x95, 0xc2, 0x88 \
    }                                                \
  }

class CacheFileChunkListener : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(CACHEFILECHUNKLISTENER_IID)

  NS_IMETHOD OnChunkRead(nsresult aResult, CacheFileChunk* aChunk) = 0;
  NS_IMETHOD OnChunkWritten(nsresult aResult, CacheFileChunk* aChunk) = 0;
  NS_IMETHOD OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                              CacheFileChunk* aChunk) = 0;
  NS_IMETHOD OnChunkUpdated(CacheFileChunk* aChunk) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CacheFileChunkListener,
                              CACHEFILECHUNKLISTENER_IID)

class ChunkListenerItem {
 public:
  ChunkListenerItem() { MOZ_COUNT_CTOR(ChunkListenerItem); }
  ~ChunkListenerItem() { MOZ_COUNT_DTOR(ChunkListenerItem); }

  nsCOMPtr<nsIEventTarget> mTarget;
  nsCOMPtr<CacheFileChunkListener> mCallback;
};

class ChunkListeners {
 public:
  ChunkListeners() { MOZ_COUNT_CTOR(ChunkListeners); }
  ~ChunkListeners() { MOZ_COUNT_DTOR(ChunkListeners); }

  nsTArray<ChunkListenerItem*> mItems;
};

class CacheFileChunk final : public CacheFileIOListener,
                             public CacheMemoryConsumer {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  bool DispatchRelease();

  CacheFileChunk(CacheFile* aFile, uint32_t aIndex, bool aInitByWriter);

  void InitNew();
  nsresult Read(CacheFileHandle* aHandle, uint32_t aLen,
                CacheHash::Hash16_t aHash, CacheFileChunkListener* aCallback);
  nsresult Write(CacheFileHandle* aHandle, CacheFileChunkListener* aCallback);
  void WaitForUpdate(CacheFileChunkListener* aCallback);
  nsresult CancelWait(CacheFileChunkListener* aCallback);
  nsresult NotifyUpdateListeners();

  uint32_t Index() const;
  CacheHash::Hash16_t Hash() const;
  uint32_t DataSize() const;

  NS_IMETHOD OnFileOpened(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnDataWritten(CacheFileHandle* aHandle, const char* aBuf,
                           nsresult aResult) override;
  NS_IMETHOD OnDataRead(CacheFileHandle* aHandle, char* aBuf,
                        nsresult aResult) override;
  NS_IMETHOD OnFileDoomed(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnEOFSet(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnFileRenamed(CacheFileHandle* aHandle, nsresult aResult) override;
  virtual bool IsKilled() override;

  bool IsReady() const;
  bool IsDirty() const;

  nsresult GetStatus();
  void SetError(nsresult aStatus);

  CacheFileChunkReadHandle GetReadHandle();
  CacheFileChunkWriteHandle GetWriteHandle(uint32_t aEnsuredBufSize);

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  friend class CacheFileChunkBuffer;
  friend class CacheFileChunkWriteHandle;
  friend class CacheFileInputStream;
  friend class CacheFileOutputStream;
  friend class CacheFile;

  virtual ~CacheFileChunk();

  void AssertOwnsLock() const;

  void UpdateDataSize(uint32_t aOffset, uint32_t aLen);
  nsresult Truncate(uint32_t aOffset);

  bool CanAllocate(uint32_t aSize) const;
  void BuffersAllocationChanged(uint32_t aFreed, uint32_t aAllocated);

  mozilla::Atomic<uint32_t, ReleaseAcquire>& ChunksMemoryUsage() const;

  enum EState { INITIAL = 0, READING = 1, WRITING = 2, READY = 3 };

  uint32_t mIndex;
  EState mState;
  nsresult mStatus;

  Atomic<bool> mActiveChunk;  // Is true iff the chunk is in CacheFile::mChunks.
                              // Adding/removing chunk to/from mChunks as well
                              // as changing this member happens under the
                              // CacheFile's lock.
  bool mIsDirty : 1;
  bool mDiscardedChunk : 1;

  uint32_t mBuffersSize;
  bool const mLimitAllocation : 1;  // Whether this chunk respects limit for
                                    // disk chunks memory usage.
  bool const mIsPriority : 1;

  // Buffer containing the chunk data. Multiple read handles can access the same
  // buffer. When write handle is created and some read handle exists a new copy
  // of the buffer is created. This prevents invalidating the buffer when
  // CacheFileInputStream::ReadSegments calls the handler outside the lock.
  RefPtr<CacheFileChunkBuffer> mBuf;

  // We need to keep pointers of the old buffers for memory reporting.
  nsTArray<RefPtr<CacheFileChunkBuffer>> mOldBufs;

  // Read handle that is used during writing the chunk to the disk.
  nsAutoPtr<CacheFileChunkReadHandle> mWritingStateHandle;

  // Buffer that is used to read the chunk from the disk. It is allowed to write
  // a new data to chunk while we wait for the data from the disk. In this case
  // this buffer is merged with mBuf in OnDataRead().
  RefPtr<CacheFileChunkBuffer> mReadingStateBuf;
  CacheHash::Hash16_t mExpectedHash;

  RefPtr<CacheFile> mFile;  // is null if chunk is cached to
                            // prevent reference cycles
  nsCOMPtr<CacheFileChunkListener> mListener;
  nsTArray<ChunkListenerItem*> mUpdateListeners;
  CacheFileUtils::ValidityMap mValidityMap;
};

}  // namespace net
}  // namespace mozilla

#endif
