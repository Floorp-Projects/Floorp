/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileChunk.h"

#include "CacheFile.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

#define kMinBufSize        512

CacheFileChunkBuffer::CacheFileChunkBuffer(CacheFileChunk *aChunk)
  : mChunk(aChunk)
  , mBuf(nullptr)
  , mBufSize(0)
  , mDataSize(0)
  , mReadHandlesCount(0)
  , mWriteHandleExists(false)
{
}

CacheFileChunkBuffer::~CacheFileChunkBuffer()
{
  if (mBuf) {
    CacheFileUtils::FreeBuffer(mBuf);
    mBuf = nullptr;
    mChunk->BuffersAllocationChanged(mBufSize, 0);
    mBufSize = 0;
  }
}

void
CacheFileChunkBuffer::CopyFrom(CacheFileChunkBuffer *aOther)
{
  MOZ_RELEASE_ASSERT(mBufSize >= aOther->mDataSize);
  mDataSize = aOther->mDataSize;
  memcpy(mBuf, aOther->mBuf, mDataSize);
}

nsresult
CacheFileChunkBuffer::FillInvalidRanges(CacheFileChunkBuffer *aOther,
                                        CacheFileUtils::ValidityMap *aMap)
{
  nsresult rv;

  rv = EnsureBufSize(aOther->mDataSize);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t invalidOffset = 0;
  uint32_t invalidLength;

  for (uint32_t i = 0; i < aMap->Length(); ++i) {
    uint32_t validOffset = (*aMap)[i].Offset();
    uint32_t validLength = (*aMap)[i].Len();

    MOZ_RELEASE_ASSERT(invalidOffset <= validOffset);
    invalidLength = validOffset - invalidOffset;
    if (invalidLength > 0) {
      MOZ_RELEASE_ASSERT(invalidOffset + invalidLength <= aOther->mBufSize);
      memcpy(mBuf + invalidOffset, aOther->mBuf + invalidOffset, invalidLength);
    }
    invalidOffset = validOffset + validLength;
  }

  if (invalidOffset < aOther->mBufSize) {
    invalidLength = aOther->mBufSize - invalidOffset;
    memcpy(mBuf + invalidOffset, aOther->mBuf + invalidOffset, invalidLength);
  }

  return NS_OK;
}

MOZ_MUST_USE nsresult
CacheFileChunkBuffer::EnsureBufSize(uint32_t aBufSize)
{
  AssertOwnsLock();

  if (mBufSize >= aBufSize) {
    return NS_OK;
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

  if (!mChunk->CanAllocate(aBufSize - mBufSize)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char *newBuf = static_cast<char *>(realloc(mBuf, aBufSize));
  if (!newBuf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mChunk->BuffersAllocationChanged(mBufSize, aBufSize);
  mBuf = newBuf;
  mBufSize = aBufSize;

  return NS_OK;
}

void
CacheFileChunkBuffer::SetDataSize(uint32_t aDataSize)
{
  MOZ_RELEASE_ASSERT(
    mDataSize <= mBufSize ||
    (mBufSize == 0 && mChunk->mState == CacheFileChunk::READING));
  mDataSize = aDataSize;
}

void
CacheFileChunkBuffer::AssertOwnsLock() const
{
  mChunk->AssertOwnsLock();
}

void
CacheFileChunkBuffer::RemoveReadHandle()
{
  AssertOwnsLock();
  MOZ_RELEASE_ASSERT(mReadHandlesCount);
  MOZ_RELEASE_ASSERT(!mWriteHandleExists);
  mReadHandlesCount--;

  if (mReadHandlesCount == 0 && mChunk->mBuf != this) {
    DebugOnly<bool> removed = mChunk->mOldBufs.RemoveElement(this);
    MOZ_ASSERT(removed);
  }
}

void
CacheFileChunkBuffer::RemoveWriteHandle()
{
  AssertOwnsLock();
  MOZ_RELEASE_ASSERT(mReadHandlesCount == 0);
  MOZ_RELEASE_ASSERT(mWriteHandleExists);
  mWriteHandleExists = false;
}

size_t
CacheFileChunkBuffer::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  size_t n = mallocSizeOf(this);

  if (mBuf) {
    n += mallocSizeOf(mBuf);
  }

  return n;
}

uint32_t
CacheFileChunkHandle::DataSize()
{
  MOZ_ASSERT(mBuf, "Unexpected call on dummy handle");
  mBuf->AssertOwnsLock();
  return mBuf->mDataSize;
}

uint32_t
CacheFileChunkHandle::Offset()
{
  MOZ_ASSERT(mBuf, "Unexpected call on dummy handle");
  mBuf->AssertOwnsLock();
  return mBuf->mChunk->Index() * kChunkSize;
}

CacheFileChunkReadHandle::CacheFileChunkReadHandle(CacheFileChunkBuffer *aBuf)
{
  mBuf = aBuf;
  mBuf->mReadHandlesCount++;
}

CacheFileChunkReadHandle::~CacheFileChunkReadHandle()
{
  mBuf->RemoveReadHandle();
}

const char *
CacheFileChunkReadHandle::Buf()
{
  return mBuf->mBuf;
}

CacheFileChunkWriteHandle::CacheFileChunkWriteHandle(CacheFileChunkBuffer *aBuf)
{
  mBuf = aBuf;
  if (mBuf) {
    MOZ_ASSERT(!mBuf->mWriteHandleExists);
    mBuf->mWriteHandleExists = true;
  }
}

CacheFileChunkWriteHandle::~CacheFileChunkWriteHandle()
{
  if (mBuf) {
    mBuf->RemoveWriteHandle();
  }
}

char *
CacheFileChunkWriteHandle::Buf()
{
  return mBuf ? mBuf->mBuf : nullptr;
}

void
CacheFileChunkWriteHandle::UpdateDataSize(uint32_t aOffset, uint32_t aLen)
{
  MOZ_ASSERT(mBuf, "Write performed on dummy handle?");
  MOZ_ASSERT(aOffset <= mBuf->mDataSize);
  MOZ_ASSERT(aOffset + aLen <= mBuf->mBufSize);

  if (aOffset + aLen > mBuf->mDataSize) {
    mBuf->mDataSize = aOffset + aLen;
  }

  mBuf->mChunk->UpdateDataSize(aOffset, aLen);
}


class NotifyUpdateListenerEvent : public Runnable {
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

protected:
  ~NotifyUpdateListenerEvent()
  {
    LOG(("NotifyUpdateListenerEvent::~NotifyUpdateListenerEvent() [this=%p]",
         this));
    MOZ_COUNT_DTOR(NotifyUpdateListenerEvent);
  }

public:
  NS_IMETHOD Run()
  {
    LOG(("NotifyUpdateListenerEvent::Run() [this=%p]", this));

    mCallback->OnChunkUpdated(mChunk);
    return NS_OK;
  }

protected:
  nsCOMPtr<CacheFileChunkListener> mCallback;
  RefPtr<CacheFileChunk>           mChunk;
};

bool
CacheFileChunk::DispatchRelease()
{
  if (NS_IsMainThread()) {
    return false;
  }

  NS_DispatchToMainThread(NewNonOwningRunnableMethod(this, &CacheFileChunk::Release));

  return true;
}

NS_IMPL_ADDREF(CacheFileChunk)
NS_IMETHODIMP_(MozExternalRefCountType)
CacheFileChunk::Release()
{
  nsrefcnt count = mRefCnt - 1;
  if (DispatchRelease()) {
    // Redispatched to the main thread.
    return count;
  }

  NS_PRECONDITION(0 != mRefCnt, "dup release");
  count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "CacheFileChunk");

  if (0 == count) {
    mRefCnt = 1;
    delete (this);
    return 0;
  }

  // We can safely access this chunk after decreasing mRefCnt since we re-post
  // all calls to Release() happening off the main thread to the main thread.
  // I.e. no other Release() that would delete the object could be run before
  // we call CacheFile::DeactivateChunk().
  //
  // NOTE: we don't grab the CacheFile's lock, so the chunk might be addrefed
  // on another thread before CacheFile::DeactivateChunk() grabs the lock on
  // this thread. To make sure we won't deactivate chunk that was just returned
  // to a new consumer we check mRefCnt once again in
  // CacheFile::DeactivateChunk() after we grab the lock.
  if (mActiveChunk && count == 1) {
    mFile->DeactivateChunk(this);
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(CacheFileChunk)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileIOListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_THREADSAFE

CacheFileChunk::CacheFileChunk(CacheFile *aFile, uint32_t aIndex,
                               bool aInitByWriter)
  : CacheMemoryConsumer(aFile->mOpenAsMemoryOnly ? MEMORY_ONLY : DONT_REPORT)
  , mIndex(aIndex)
  , mState(INITIAL)
  , mStatus(NS_OK)
  , mIsDirty(false)
  , mActiveChunk(false)
  , mBuffersSize(0)
  , mLimitAllocation(!aFile->mOpenAsMemoryOnly && aInitByWriter)
  , mIsPriority(aFile->mPriority)
  , mExpectedHash(0)
  , mFile(aFile)
{
  LOG(("CacheFileChunk::CacheFileChunk() [this=%p, index=%u, initByWriter=%d]",
       this, aIndex, aInitByWriter));
  MOZ_COUNT_CTOR(CacheFileChunk);

  mBuf = new CacheFileChunkBuffer(this);
}

CacheFileChunk::~CacheFileChunk()
{
  LOG(("CacheFileChunk::~CacheFileChunk() [this=%p]", this));
  MOZ_COUNT_DTOR(CacheFileChunk);
}

void
CacheFileChunk::AssertOwnsLock() const
{
  mFile->AssertOwnsLock();
}

void
CacheFileChunk::InitNew()
{
  AssertOwnsLock();

  LOG(("CacheFileChunk::InitNew() [this=%p]", this));

  MOZ_ASSERT(mState == INITIAL);
  MOZ_ASSERT(NS_SUCCEEDED(mStatus));
  MOZ_ASSERT(!mBuf->Buf());
  MOZ_ASSERT(!mWritingStateHandle);
  MOZ_ASSERT(!mReadingStateBuf);
  MOZ_ASSERT(!mIsDirty);

  mBuf = new CacheFileChunkBuffer(this);
  mState = READY;
}

nsresult
CacheFileChunk::Read(CacheFileHandle *aHandle, uint32_t aLen,
                     CacheHash::Hash16_t aHash,
                     CacheFileChunkListener *aCallback)
{
  AssertOwnsLock();

  LOG(("CacheFileChunk::Read() [this=%p, handle=%p, len=%d, listener=%p]",
       this, aHandle, aLen, aCallback));

  MOZ_ASSERT(mState == INITIAL);
  MOZ_ASSERT(NS_SUCCEEDED(mStatus));
  MOZ_ASSERT(!mBuf->Buf());
  MOZ_ASSERT(!mWritingStateHandle);
  MOZ_ASSERT(!mReadingStateBuf);
  MOZ_ASSERT(aLen);

  nsresult rv;

  mState = READING;

  RefPtr<CacheFileChunkBuffer> tmpBuf = new CacheFileChunkBuffer(this);
  rv = tmpBuf->EnsureBufSize(aLen);
  if (NS_FAILED(rv)) {
    SetError(rv);
    return mStatus;
  }
  tmpBuf->SetDataSize(aLen);

  rv = CacheFileIOManager::Read(aHandle, mIndex * kChunkSize,
                                tmpBuf->Buf(), aLen,
                                this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    rv = mIndex ? NS_ERROR_FILE_CORRUPTED : NS_ERROR_FILE_NOT_FOUND;
    SetError(rv);
  } else {
    mReadingStateBuf.swap(tmpBuf);
    mListener = aCallback;
    // mBuf contains no data but we set datasize to size of the data that will
    // be read from the disk. No handle is allowed to access the non-existent
    // data until reading finishes, but data can be appended or overwritten.
    // These pieces are tracked in mValidityMap and will be merged with the data
    // read from disk in OnDataRead().
    mBuf->SetDataSize(aLen);
    mExpectedHash = aHash;
  }

  return rv;
}

nsresult
CacheFileChunk::Write(CacheFileHandle *aHandle,
                      CacheFileChunkListener *aCallback)
{
  AssertOwnsLock();

  LOG(("CacheFileChunk::Write() [this=%p, handle=%p, listener=%p]",
       this, aHandle, aCallback));

  MOZ_ASSERT(mState == READY);
  MOZ_ASSERT(NS_SUCCEEDED(mStatus));
  MOZ_ASSERT(!mWritingStateHandle);
  MOZ_ASSERT(mBuf->DataSize()); // Don't write chunk when it is empty
  MOZ_ASSERT(mBuf->ReadHandlesCount() == 0);
  MOZ_ASSERT(!mBuf->WriteHandleExists());

  nsresult rv;

  mState = WRITING;
  mWritingStateHandle = new CacheFileChunkReadHandle(mBuf);

  rv = CacheFileIOManager::Write(aHandle, mIndex * kChunkSize,
                                 mWritingStateHandle->Buf(),
                                 mWritingStateHandle->DataSize(),
                                 false, false, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mWritingStateHandle = nullptr;
    SetError(rv);
  } else {
    mListener = aCallback;
    mIsDirty = false;
  }

  return rv;
}

void
CacheFileChunk::WaitForUpdate(CacheFileChunkListener *aCallback)
{
  AssertOwnsLock();

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
  item->mTarget = CacheFileIOManager::IOTarget();
  if (!item->mTarget) {
    LOG(("CacheFileChunk::WaitForUpdate() - Cannot get Cache I/O thread! Using "
         "main thread for callback."));
    item->mTarget = do_GetMainThread();
  }
  item->mCallback = aCallback;
  MOZ_ASSERT(item->mTarget);
  item->mCallback = aCallback;

  mUpdateListeners.AppendElement(item);
}

nsresult
CacheFileChunk::CancelWait(CacheFileChunkListener *aCallback)
{
  AssertOwnsLock();

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
  AssertOwnsLock();

  LOG(("CacheFileChunk::NotifyUpdateListeners() [this=%p]", this));

  MOZ_ASSERT(IsReady());

  nsresult rv, rv2;

  rv = NS_OK;
  for (uint32_t i = 0 ; i < mUpdateListeners.Length() ; i++) {
    ChunkListenerItem *item = mUpdateListeners[i];

    LOG(("CacheFileChunk::NotifyUpdateListeners() - Notifying listener %p "
         "[this=%p]", item->mCallback.get(), this));

    RefPtr<NotifyUpdateListenerEvent> ev;
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
CacheFileChunk::Index() const
{
  return mIndex;
}

CacheHash::Hash16_t
CacheFileChunk::Hash() const
{
  AssertOwnsLock();

  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(IsReady());

  return CacheHash::Hash16(mBuf->Buf(), mBuf->DataSize());
}

uint32_t
CacheFileChunk::DataSize() const
{
  return mBuf->DataSize();
}

void
CacheFileChunk::UpdateDataSize(uint32_t aOffset, uint32_t aLen)
{
  AssertOwnsLock();

  // UpdateDataSize() is called only when we've written some data to the chunk
  // and we never write data anymore once some error occurs.
  MOZ_ASSERT(NS_SUCCEEDED(mStatus));

  LOG(("CacheFileChunk::UpdateDataSize() [this=%p, offset=%d, len=%d]",
       this, aOffset, aLen));

  mIsDirty = true;

  int64_t fileSize = static_cast<int64_t>(kChunkSize) * mIndex + aOffset + aLen;
  bool notify = false;

  if (fileSize > mFile->mDataSize) {
    mFile->mDataSize = fileSize;
    notify = true;
  }

  if (mState == READY || mState == WRITING) {
    MOZ_ASSERT(mValidityMap.Length() == 0);

    if (notify) {
      NotifyUpdateListeners();
    }

    return;
  }

  // We're still waiting for data from the disk. This chunk cannot be used by
  // input stream, so there must be no update listener. We also need to keep
  // track of where the data is written so that we can correctly merge the new
  // data with the old one.

  MOZ_ASSERT(mUpdateListeners.Length() == 0);
  MOZ_ASSERT(mState == READING);

  mValidityMap.AddPair(aOffset, aLen);
  mValidityMap.Log();
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

    mWritingStateHandle = nullptr;

    if (NS_WARN_IF(NS_FAILED(aResult))) {
      SetError(aResult);
    }

    mState = READY;
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
    MOZ_ASSERT(mReadingStateBuf);
    MOZ_RELEASE_ASSERT(mBuf->ReadHandlesCount() == 0);
    MOZ_RELEASE_ASSERT(!mBuf->WriteHandleExists());

    RefPtr<CacheFileChunkBuffer> tmpBuf;
    tmpBuf.swap(mReadingStateBuf);

    if (NS_SUCCEEDED(aResult)) {
      CacheHash::Hash16_t hash = CacheHash::Hash16(tmpBuf->Buf(),
                                                   tmpBuf->DataSize());
      if (hash != mExpectedHash) {
        LOG(("CacheFileChunk::OnDataRead() - Hash mismatch! Hash of the data is"
             " %hx, hash in metadata is %hx. [this=%p, idx=%d]",
             hash, mExpectedHash, this, mIndex));
        aResult = NS_ERROR_FILE_CORRUPTED;
      } else {
        if (!mBuf->Buf()) {
          // Just swap the buffers if mBuf is still empty
          mBuf.swap(tmpBuf);
        } else {
          LOG(("CacheFileChunk::OnDataRead() - Merging buffers. [this=%p]",
               this));

          mValidityMap.Log();
          aResult = mBuf->FillInvalidRanges(tmpBuf, &mValidityMap);
          mValidityMap.Clear();
        }
      }
    }

    if (NS_FAILED(aResult)) {
      aResult = mIndex ? NS_ERROR_FILE_CORRUPTED : NS_ERROR_FILE_NOT_FOUND;
      SetError(aResult);
      mBuf->SetDataSize(0);
    }

    mState = READY;
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
CacheFileChunk::IsKilled()
{
  return mFile->IsKilled();
}

bool
CacheFileChunk::IsReady() const
{
  AssertOwnsLock();

  return (NS_SUCCEEDED(mStatus) && (mState == READY || mState == WRITING));
}

bool
CacheFileChunk::IsDirty() const
{
  AssertOwnsLock();

  return mIsDirty;
}

nsresult
CacheFileChunk::GetStatus()
{
  AssertOwnsLock();

  return mStatus;
}

void
CacheFileChunk::SetError(nsresult aStatus)
{
  LOG(("CacheFileChunk::SetError() [this=%p, status=0x%08x]", this, aStatus));

  MOZ_ASSERT(NS_FAILED(aStatus));

  if (NS_FAILED(mStatus)) {
    // Remember only the first error code.
    return;
  }

  mStatus = aStatus;
}

CacheFileChunkReadHandle
CacheFileChunk::GetReadHandle()
{
  LOG(("CacheFileChunk::GetReadHandle() [this=%p]", this));

  AssertOwnsLock();

  MOZ_RELEASE_ASSERT(mState == READY || mState == WRITING);
  // We don't release the lock when writing the data and CacheFileOutputStream
  // doesn't get the read handle, so there cannot be a write handle when read
  // handle is obtained.
  MOZ_RELEASE_ASSERT(!mBuf->WriteHandleExists());

  return CacheFileChunkReadHandle(mBuf);
}

CacheFileChunkWriteHandle
CacheFileChunk::GetWriteHandle(uint32_t aEnsuredBufSize)
{
  LOG(("CacheFileChunk::GetWriteHandle() [this=%p, ensuredBufSize=%u]",
       this, aEnsuredBufSize));

  AssertOwnsLock();

  if (NS_FAILED(mStatus)) {
    return CacheFileChunkWriteHandle(nullptr); // dummy handle
  }

  nsresult rv;

  // We don't support multiple write handles
  MOZ_RELEASE_ASSERT(!mBuf->WriteHandleExists());

  if (mBuf->ReadHandlesCount()) {
    LOG(("CacheFileChunk::GetWriteHandle() - cloning buffer because of existing"
         " read handle"));

    MOZ_RELEASE_ASSERT(mState != READING);
    RefPtr<CacheFileChunkBuffer> newBuf = new CacheFileChunkBuffer(this);
    rv = newBuf->EnsureBufSize(std::max(aEnsuredBufSize, mBuf->DataSize()));
    if (NS_SUCCEEDED(rv)) {
      newBuf->CopyFrom(mBuf);
      mOldBufs.AppendElement(mBuf);
      mBuf = newBuf;
    }
  } else {
    rv = mBuf->EnsureBufSize(aEnsuredBufSize);
  }

  if (NS_FAILED(rv)) {
    SetError(NS_ERROR_OUT_OF_MEMORY);
    return CacheFileChunkWriteHandle(nullptr); // dummy handle
  }

  return CacheFileChunkWriteHandle(mBuf);
}

// Memory reporting

size_t
CacheFileChunk::SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  size_t n = mBuf->SizeOfIncludingThis(mallocSizeOf);

  if (mReadingStateBuf) {
    n += mReadingStateBuf->SizeOfIncludingThis(mallocSizeOf);
  }

  for (uint32_t i = 0; i < mOldBufs.Length(); ++i) {
    n += mOldBufs[i]->SizeOfIncludingThis(mallocSizeOf);
  }

  n += mValidityMap.SizeOfExcludingThis(mallocSizeOf);

  return n;
}

size_t
CacheFileChunk::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
}

bool
CacheFileChunk::CanAllocate(uint32_t aSize) const
{
  if (!mLimitAllocation) {
    return true;
  }

  LOG(("CacheFileChunk::CanAllocate() [this=%p, size=%u]", this, aSize));

  uint32_t limit = CacheObserver::MaxDiskChunksMemoryUsage(mIsPriority);
  if (limit == 0) {
    return true;
  }

  uint32_t usage = ChunksMemoryUsage();
  if (usage + aSize > limit) {
    LOG(("CacheFileChunk::CanAllocate() - Returning false. [this=%p]", this));
    return false;
  }

  return true;
}

void
CacheFileChunk::BuffersAllocationChanged(uint32_t aFreed, uint32_t aAllocated)
{
  uint32_t oldBuffersSize = mBuffersSize;
  mBuffersSize += aAllocated;
  mBuffersSize -= aFreed;

  DoMemoryReport(sizeof(CacheFileChunk) + mBuffersSize);

  if (!mLimitAllocation) {
    return;
  }

  ChunksMemoryUsage() -= oldBuffersSize;
  ChunksMemoryUsage() += mBuffersSize;
  LOG(("CacheFileChunk::BuffersAllocationChanged() - %s chunks usage %u "
       "[this=%p]", mIsPriority ? "Priority" : "Normal",
       static_cast<uint32_t>(ChunksMemoryUsage()), this));
}

mozilla::Atomic<uint32_t, ReleaseAcquire>& CacheFileChunk::ChunksMemoryUsage() const
{
  static mozilla::Atomic<uint32_t, ReleaseAcquire> chunksMemoryUsage(0);
  static mozilla::Atomic<uint32_t, ReleaseAcquire> prioChunksMemoryUsage(0);
  return mIsPriority ? prioChunksMemoryUsage : chunksMemoryUsage;
}

} // namespace net
} // namespace mozilla
