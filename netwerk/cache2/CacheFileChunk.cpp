/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileChunk.h"

#include "CacheFile.h"
#include "nsThreadUtils.h"
#include "nsAlgorithm.h"
#include <algorithm>

namespace mozilla {
namespace net {

#define kMinBufSize        512

class NotifyUpdateListenerEvent : public nsRunnable {
public:
  NotifyUpdateListenerEvent(CacheFileChunkListener *aCallback,
                            CacheFileChunk *aChunk)
    : mCallback(aCallback)
    , mChunk(aChunk)
  {
    LOG(("NotifyUpdateListenerEvent::NotifyUpdateListenerEvent() [this=%p]",
         this));
    MOZ_COUNT_CTOR(NotifyUpdateListenerEvent);
  }

  ~NotifyUpdateListenerEvent()
  {
    LOG(("NotifyUpdateListenerEvent::~NotifyUpdateListenerEvent() [this=%p]",
         this));
    MOZ_COUNT_DTOR(NotifyUpdateListenerEvent);
  }

  NS_IMETHOD Run()
  {
    LOG(("NotifyUpdateListenerEvent::Run() [this=%p]", this));

    mCallback->OnChunkUpdated(mChunk);
    return NS_OK;
  }

protected:
  nsCOMPtr<CacheFileChunkListener> mCallback;
  nsRefPtr<CacheFileChunk>         mChunk;
};


class ValidityPair {
public:
  ValidityPair(uint32_t aOffset, uint32_t aLen)
    : mOffset(aOffset), mLen(aLen)
  {}

  ValidityPair& operator=(const ValidityPair& aOther) {
    mOffset = aOther.mOffset;
    mLen = aOther.mLen;
    return *this;
  }

  bool Overlaps(const ValidityPair& aOther) const {
    if ((mOffset <= aOther.mOffset && mOffset + mLen >= aOther.mOffset) ||
        (aOther.mOffset <= mOffset && aOther.mOffset + mLen >= mOffset))
      return true;

    return false;
  }

  bool LessThan(const ValidityPair& aOther) const {
    if (mOffset < aOther.mOffset)
      return true;

    if (mOffset == aOther.mOffset && mLen < aOther.mLen)
      return true;

    return false;
  }

  void Merge(const ValidityPair& aOther) {
    MOZ_ASSERT(Overlaps(aOther));

    uint32_t offset = std::min(mOffset, aOther.mOffset);
    uint32_t end = std::max(mOffset + mLen, aOther.mOffset + aOther.mLen);

    mOffset = offset;
    mLen = end - offset;
  }

  uint32_t Offset() { return mOffset; }
  uint32_t Len()    { return mLen; }

private:
  uint32_t mOffset;
  uint32_t mLen;
};


NS_IMPL_ADDREF(CacheFileChunk)
NS_IMETHODIMP_(MozExternalRefCountType)
CacheFileChunk::Release()
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "CacheFileChunk");

  if (0 == count) {
    mRefCnt = 1;
    delete (this);
    return 0;
  }

  if (!mRemovingChunk && count == 1) {
    mFile->RemoveChunk(this);
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(CacheFileChunk)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileIOListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_THREADSAFE

CacheFileChunk::CacheFileChunk(CacheFile *aFile, uint32_t aIndex)
  : CacheMemoryConsumer(aFile->mOpenAsMemoryOnly ? MEMORY_ONLY : DONT_REPORT)
  , mIndex(aIndex)
  , mState(INITIAL)
  , mIsDirty(false)
  , mRemovingChunk(false)
  , mDataSize(0)
  , mBuf(nullptr)
  , mBufSize(0)
  , mRWBuf(nullptr)
  , mRWBufSize(0)
  , mReadHash(0)
  , mFile(aFile)
{
  LOG(("CacheFileChunk::CacheFileChunk() [this=%p]", this));
  MOZ_COUNT_CTOR(CacheFileChunk);
}

CacheFileChunk::~CacheFileChunk()
{
  LOG(("CacheFileChunk::~CacheFileChunk() [this=%p]", this));
  MOZ_COUNT_DTOR(CacheFileChunk);

  if (mBuf) {
    free(mBuf);
    mBuf = nullptr;
    mBufSize = 0;
  }

  if (mRWBuf) {
    free(mRWBuf);
    mRWBuf = nullptr;
    mRWBufSize = 0;
  }
}

void
CacheFileChunk::InitNew(CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::InitNew() [this=%p, listener=%p]", this, aCallback));

  MOZ_ASSERT(mState == INITIAL);
  MOZ_ASSERT(!mBuf);
  MOZ_ASSERT(!mRWBuf);

  mBuf = static_cast<char *>(moz_xmalloc(kMinBufSize));
  mBufSize = kMinBufSize;
  mDataSize = 0;
  mState = READY;
  mIsDirty = true;

  DoMemoryReport(MemorySize());
}

nsresult
CacheFileChunk::Read(CacheFileHandle *aHandle, uint32_t aLen,
                     CacheHash::Hash16_t aHash,
                     CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::Read() [this=%p, handle=%p, len=%d, listener=%p]",
       this, aHandle, aLen, aCallback));

  MOZ_ASSERT(mState == INITIAL);
  MOZ_ASSERT(!mBuf);
  MOZ_ASSERT(!mRWBuf);
  MOZ_ASSERT(aLen);

  nsresult rv;

  mRWBuf = static_cast<char *>(moz_xmalloc(aLen));
  mRWBufSize = aLen;

  DoMemoryReport(MemorySize());

  rv = CacheFileIOManager::Read(aHandle, mIndex * kChunkSize, mRWBuf, aLen,
                                this);
  if (NS_FAILED(rv)) {
    mState = READING;   // TODO: properly handle error states
//    mState = ERROR;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mState = READING;
  mListener = aCallback;
  mDataSize = aLen;
  mReadHash = aHash;
  return NS_OK;
}

nsresult
CacheFileChunk::Write(CacheFileHandle *aHandle,
                      CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::Write() [this=%p, handle=%p, listener=%p]",
       this, aHandle, aCallback));

  MOZ_ASSERT(mState == READY);
  MOZ_ASSERT(!mRWBuf);
  MOZ_ASSERT(mBuf);
  MOZ_ASSERT(mDataSize); // Don't write chunk when it is empty

  nsresult rv;

  mRWBuf = mBuf;
  mRWBufSize = mBufSize;
  mBuf = nullptr;
  mBufSize = 0;

  rv = CacheFileIOManager::Write(aHandle, mIndex * kChunkSize, mRWBuf,
                                 mDataSize, false, this);
  if (NS_FAILED(rv)) {
    mState = WRITING;   // TODO: properly handle error states
//    mState = ERROR;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mState = WRITING;
  mListener = aCallback;
  mIsDirty = false;
  return NS_OK;
}

void
CacheFileChunk::WaitForUpdate(CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::WaitForUpdate() [this=%p, listener=%p]",
       this, aCallback));

  MOZ_ASSERT(mFile->mOutput);
  MOZ_ASSERT(IsReady());

#ifdef DEBUG
  for (uint32_t i = 0 ; i < mUpdateListeners.Length() ; i++) {
    MOZ_ASSERT(mUpdateListeners[i]->mCallback != aCallback);
  }
#endif

  ChunkListenerItem *item = new ChunkListenerItem();
  item->mTarget = NS_GetCurrentThread();
  item->mCallback = aCallback;

  mUpdateListeners.AppendElement(item);
}

nsresult
CacheFileChunk::CancelWait(CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::CancelWait() [this=%p, listener=%p]", this, aCallback));

  MOZ_ASSERT(IsReady());

  uint32_t i;
  for (i = 0 ; i < mUpdateListeners.Length() ; i++) {
    ChunkListenerItem *item = mUpdateListeners[i];

    if (item->mCallback == aCallback) {
      mUpdateListeners.RemoveElementAt(i);
      delete item;
      break;
    }
  }

#ifdef DEBUG
  for ( ; i < mUpdateListeners.Length() ; i++) {
    MOZ_ASSERT(mUpdateListeners[i]->mCallback != aCallback);
  }
#endif

  return NS_OK;
}

nsresult
CacheFileChunk::NotifyUpdateListeners()
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::NotifyUpdateListeners() [this=%p]", this));

  MOZ_ASSERT(IsReady());

  nsresult rv, rv2;

  rv = NS_OK;
  for (uint32_t i = 0 ; i < mUpdateListeners.Length() ; i++) {
    ChunkListenerItem *item = mUpdateListeners[i];

    LOG(("CacheFileChunk::NotifyUpdateListeners() - Notifying listener %p "
         "[this=%p]", item->mCallback.get(), this));

    nsRefPtr<NotifyUpdateListenerEvent> ev;
    ev = new NotifyUpdateListenerEvent(item->mCallback, this);
    rv2 = item->mTarget->Dispatch(ev, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv2) && NS_SUCCEEDED(rv))
      rv = rv2;
    delete item;
  }

  mUpdateListeners.Clear();

  return rv;
}

uint32_t
CacheFileChunk::Index()
{
  return mIndex;
}

CacheHash::Hash16_t
CacheFileChunk::Hash()
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mBuf);
  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(IsReady());

  return CacheHash::Hash16(BufForReading(), mDataSize);
}

uint32_t
CacheFileChunk::DataSize()
{
  mFile->AssertOwnsLock();
  return mDataSize;
}

void
CacheFileChunk::UpdateDataSize(uint32_t aOffset, uint32_t aLen, bool aEOF)
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(!aEOF, "Implement me! What to do with opened streams?");
  MOZ_ASSERT(aOffset <= mDataSize);

  LOG(("CacheFileChunk::UpdateDataSize() [this=%p, offset=%d, len=%d, EOF=%d]",
       this, aOffset, aLen, aEOF));

  mIsDirty = true;

  int64_t fileSize = kChunkSize * mIndex + aOffset + aLen;
  bool notify = false;

  if (fileSize > mFile->mDataSize)
    mFile->mDataSize = fileSize;

  if (aOffset + aLen > mDataSize) {
    mDataSize = aOffset + aLen;
    notify = true;
  }

  if (mState == READY || mState == WRITING) {
    MOZ_ASSERT(mValidityMap.Length() == 0);

    if (notify)
      NotifyUpdateListeners();

    return;
  }

  // We're still waiting for data from the disk. This chunk cannot be used by
  // input stream, so there must be no update listener. We also need to keep
  // track of where the data is written so that we can correctly merge the new
  // data with the old one.

  MOZ_ASSERT(mUpdateListeners.Length() == 0);
  MOZ_ASSERT(mState == READING);

  ValidityPair pair(aOffset, aLen);

  if (mValidityMap.Length() == 0) {
    mValidityMap.AppendElement(pair);
    return;
  }


  // Find out where to place this pair into the map, it can overlap with
  // one preceding pair and all subsequent pairs.
  uint32_t pos = 0;
  for (pos = mValidityMap.Length() ; pos > 0 ; pos--) {
    if (mValidityMap[pos-1].LessThan(pair)) {
      if (mValidityMap[pos-1].Overlaps(pair)) {
        // Merge with the preceding pair
        mValidityMap[pos-1].Merge(pair);
        pos--; // Point to the updated pair
      }
      else {
        if (pos == mValidityMap.Length())
          mValidityMap.AppendElement(pair);
        else
          mValidityMap.InsertElementAt(pos, pair);
      }

      break;
    }
  }

  if (!pos)
    mValidityMap.InsertElementAt(0, pair);

  // Now pos points to merged or inserted pair, check whether it overlaps with
  // subsequent pairs.
  while (pos + 1 < mValidityMap.Length()) {
    if (mValidityMap[pos].Overlaps(mValidityMap[pos + 1])) {
      mValidityMap[pos].Merge(mValidityMap[pos + 1]);
      mValidityMap.RemoveElementAt(pos + 1);
    }
    else {
      break;
    }
  }
}

nsresult
CacheFileChunk::OnFileOpened(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheFileChunk::OnFileOpened should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileChunk::OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                              nsresult aResult)
{
  LOG(("CacheFileChunk::OnDataWritten() [this=%p, handle=%p, result=0x%08x]",
       this, aHandle, aResult));

  nsCOMPtr<CacheFileChunkListener> listener;

  {
    CacheFileAutoLock lock(mFile);

    MOZ_ASSERT(mState == WRITING);
    MOZ_ASSERT(mListener);

#if 0
    // TODO: properly handle error states
    if (NS_FAILED(aResult)) {
      mState = ERROR;
    }
    else {
#endif
      mState = READY;
      if (!mBuf) {
        mBuf = mRWBuf;
        mBufSize = mRWBufSize;
      }
      else {
        free(mRWBuf);
      }

      mRWBuf = nullptr;
      mRWBufSize = 0;

      DoMemoryReport(MemorySize());
#if 0
    }
#endif

    mListener.swap(listener);
  }

  listener->OnChunkWritten(aResult, this);

  return NS_OK;
}

nsresult
CacheFileChunk::OnDataRead(CacheFileHandle *aHandle, char *aBuf,
                           nsresult aResult)
{
  LOG(("CacheFileChunk::OnDataRead() [this=%p, handle=%p, result=0x%08x]",
       this, aHandle, aResult));

  nsCOMPtr<CacheFileChunkListener> listener;

  {
    CacheFileAutoLock lock(mFile);

    MOZ_ASSERT(mState == READING);
    MOZ_ASSERT(mListener);

    if (NS_SUCCEEDED(aResult)) {
      CacheHash::Hash16_t hash = CacheHash::Hash16(mRWBuf, mRWBufSize);
      if (hash != mReadHash) {
        LOG(("CacheFileChunk::OnDataRead() - Hash mismatch! Hash of the data is"
             " %hx, hash in metadata is %hx. [this=%p, idx=%d]",
             hash, mReadHash, this, mIndex));
        aResult = NS_ERROR_FILE_CORRUPTED;
      }
      else {
        if (!mBuf) {
          // Just swap the buffers if we don't have mBuf yet
          MOZ_ASSERT(mDataSize == mRWBufSize);
          mBuf = mRWBuf;
          mBufSize = mRWBufSize;
          mRWBuf = nullptr;
          mRWBufSize = 0;
        } else {
          // Merge data with write buffer
          if (mRWBufSize < mBufSize) {
            mRWBuf = static_cast<char *>(moz_xrealloc(mRWBuf, mBufSize));
            mRWBufSize = mBufSize;
          }

          for (uint32_t i = 0 ; i < mValidityMap.Length() ; i++) {
            memcpy(mRWBuf + mValidityMap[i].Offset(),
                   mBuf + mValidityMap[i].Offset(), mValidityMap[i].Len());
          }

          free(mBuf);
          mBuf = mRWBuf;
          mBufSize = mRWBufSize;
          mRWBuf = nullptr;
          mRWBufSize = 0;

          DoMemoryReport(MemorySize());
        }
      }
    }

    if (NS_FAILED(aResult)) {
#if 0
      // TODO: properly handle error states
      mState = ERROR;
#endif
      mState = READY;
      mDataSize = 0;
    }
    else {
      mState = READY;
    }

    mListener.swap(listener);
  }

  listener->OnChunkRead(aResult, this);

  return NS_OK;
}

nsresult
CacheFileChunk::OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheFileChunk::OnFileDoomed should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileChunk::OnEOFSet(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheFileChunk::OnEOFSet should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileChunk::OnFileRenamed(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheFileChunk::OnFileRenamed should not be called!");
  return NS_ERROR_UNEXPECTED;
}

bool
CacheFileChunk::IsReady() const
{
  mFile->AssertOwnsLock();

  return (mState == READY || mState == WRITING);
}

bool
CacheFileChunk::IsDirty() const
{
  mFile->AssertOwnsLock();

  return mIsDirty;
}

char *
CacheFileChunk::BufForWriting() const
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mBuf); // Writer should always first call EnsureBufSize()

  MOZ_ASSERT((mState == READY && !mRWBuf) ||
             (mState == WRITING && mRWBuf) ||
             (mState == READING && mRWBuf));

  return mBuf;
}

const char *
CacheFileChunk::BufForReading() const
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT((mState == READY && mBuf && !mRWBuf) ||
             (mState == WRITING && mRWBuf));

  return mBuf ? mBuf : mRWBuf;
}

void
CacheFileChunk::EnsureBufSize(uint32_t aBufSize)
{
  mFile->AssertOwnsLock();

  if (mBufSize >= aBufSize)
    return;

  bool copy = false;
  if (!mBuf && mState == WRITING) {
    // We need to duplicate the data that is being written on the background
    // thread, so make sure that all the data fits into the new buffer.
    copy = true;

    if (mRWBufSize > aBufSize)
      aBufSize = mRWBufSize;
  }

  // find smallest power of 2 greater than or equal to aBufSize
  aBufSize--;
  aBufSize |= aBufSize >> 1;
  aBufSize |= aBufSize >> 2;
  aBufSize |= aBufSize >> 4;
  aBufSize |= aBufSize >> 8;
  aBufSize |= aBufSize >> 16;
  aBufSize++;

  const uint32_t minBufSize = kMinBufSize;
  const uint32_t maxBufSize = kChunkSize;
  aBufSize = clamped(aBufSize, minBufSize, maxBufSize);

  mBuf = static_cast<char *>(moz_xrealloc(mBuf, aBufSize));
  mBufSize = aBufSize;

  if (copy)
    memcpy(mBuf, mRWBuf, mRWBufSize);

  DoMemoryReport(MemorySize());
}

// Memory reporting

size_t
CacheFileChunk::SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  size_t n = 0;
  n += mallocSizeOf(mBuf);
  n += mallocSizeOf(mRWBuf);
  n += mValidityMap.SizeOfExcludingThis(mallocSizeOf);

  return n;
}

size_t
CacheFileChunk::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
}

} // net
} // mozilla
