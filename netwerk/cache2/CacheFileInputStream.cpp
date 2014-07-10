/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileInputStream.h"

#include "CacheFile.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include <algorithm>

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(CacheFileInputStream)
NS_IMETHODIMP_(MozExternalRefCountType)
CacheFileInputStream::Release()
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "CacheFileInputStream");

  if (0 == count) {
    mRefCnt = 1;
    delete (this);
    return 0;
  }

  if (count == 1) {
    mFile->RemoveInput(this, mStatus);
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(CacheFileInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileChunkListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END_THREADSAFE

CacheFileInputStream::CacheFileInputStream(CacheFile *aFile)
  : mFile(aFile)
  , mPos(0)
  , mClosed(false)
  , mStatus(NS_OK)
  , mWaitingForUpdate(false)
  , mListeningForChunk(-1)
  , mInReadSegments(false)
  , mCallbackFlags(0)
{
  LOG(("CacheFileInputStream::CacheFileInputStream() [this=%p]", this));
  MOZ_COUNT_CTOR(CacheFileInputStream);
}

CacheFileInputStream::~CacheFileInputStream()
{
  LOG(("CacheFileInputStream::~CacheFileInputStream() [this=%p]", this));
  MOZ_COUNT_DTOR(CacheFileInputStream);
}

// nsIInputStream
NS_IMETHODIMP
CacheFileInputStream::Close()
{
  LOG(("CacheFileInputStream::Close() [this=%p]", this));
  return CloseWithStatus(NS_OK);
}

NS_IMETHODIMP
CacheFileInputStream::Available(uint64_t *_retval)
{
  CacheFileAutoLock lock(mFile);
  MOZ_ASSERT(!mInReadSegments);

  if (mClosed) {
    LOG(("CacheFileInputStream::Available() - Stream is closed. [this=%p, "
         "status=0x%08x]", this, mStatus));
    return NS_FAILED(mStatus) ? mStatus : NS_BASE_STREAM_CLOSED;
  }

  EnsureCorrectChunk(false);
  if (NS_FAILED(mStatus))
    return mStatus;

  *_retval = 0;

  if (mChunk) {
    int64_t canRead = mFile->BytesFromChunk(mChunk->Index());
    canRead -= (mPos % kChunkSize);

    if (canRead > 0)
      *_retval = canRead;
    else if (canRead == 0 && !mFile->mOutput)
      return NS_BASE_STREAM_CLOSED;
  }

  LOG(("CacheFileInputStream::Available() [this=%p, retval=%lld]",
       this, *_retval));

  return NS_OK;
}

NS_IMETHODIMP
CacheFileInputStream::Read(char *aBuf, uint32_t aCount, uint32_t *_retval)
{
  LOG(("CacheFileInputStream::Read() [this=%p, count=%d]", this, aCount));
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, _retval);
}

NS_IMETHODIMP
CacheFileInputStream::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure,
                                   uint32_t aCount, uint32_t *_retval)
{
  CacheFileAutoLock lock(mFile);
  MOZ_ASSERT(!mInReadSegments);

  LOG(("CacheFileInputStream::ReadSegments() [this=%p, count=%d]",
       this, aCount));

  nsresult rv;

  *_retval = 0;

  if (mClosed) {
    LOG(("CacheFileInputStream::ReadSegments() - Stream is closed. [this=%p, "
         "status=0x%08x]", this, mStatus));

    if NS_FAILED(mStatus)
      return mStatus;

    return NS_OK;
  }

  EnsureCorrectChunk(false);

  while (true) {
    if (NS_FAILED(mStatus))
      return mStatus;

    if (!mChunk) {
      if (mListeningForChunk == -1) {
        return NS_OK;
      }
      else {
        return NS_BASE_STREAM_WOULD_BLOCK;
      }
    }

    int64_t canRead;
    const char *buf;
    CanRead(&canRead, &buf);
    if (NS_FAILED(mStatus)) {
      return mStatus;
    }

    if (canRead < 0) {
      // file was truncated ???
      MOZ_ASSERT(false, "SetEOF is currenty not implemented?!");
      rv = NS_OK;
    } else if (canRead > 0) {
      uint32_t toRead = std::min(static_cast<uint32_t>(canRead), aCount);

      // We need to release the lock to avoid lock re-entering unless the
      // caller is Read() method.
#ifdef DEBUG
      int64_t oldPos = mPos;
#endif
      mInReadSegments = true;
      if (aWriter != (nsWriteSegmentFun)NS_CopySegmentToBuffer) {
        lock.Unlock();
      }
      uint32_t read;
      rv = aWriter(this, aClosure, buf, *_retval, toRead, &read);
      if (aWriter != (nsWriteSegmentFun)NS_CopySegmentToBuffer) {
        lock.Lock();
      }
      mInReadSegments = false;
#ifdef DEBUG
      MOZ_ASSERT(oldPos == mPos);
#endif

      if (NS_SUCCEEDED(rv)) {
        MOZ_ASSERT(read <= toRead,
                   "writer should not write more than we asked it to write");

        *_retval += read;
        mPos += read;
        aCount -= read;

        // The last chunk is released after the caller closes this stream.
        EnsureCorrectChunk(false);

        if (mChunk && aCount) {
          // We have the next chunk! Go on.
          continue;
        }
      }

      rv = NS_OK;
    } else {
      if (mFile->mOutput)
        rv = NS_BASE_STREAM_WOULD_BLOCK;
      else {
        rv = NS_OK;
      }
    }

    break;
  }

  LOG(("CacheFileInputStream::ReadSegments() [this=%p, rv=0x%08x, retval=%d]",
       this, rv, *_retval));

  return rv;
}

NS_IMETHODIMP
CacheFileInputStream::IsNonBlocking(bool *_retval)
{
  *_retval = true;
  return NS_OK;
}

// nsIAsyncInputStream
NS_IMETHODIMP
CacheFileInputStream::CloseWithStatus(nsresult aStatus)
{
  CacheFileAutoLock lock(mFile);

  LOG(("CacheFileInputStream::CloseWithStatus() [this=%p, aStatus=0x%08x]",
       this, aStatus));

  return CloseWithStatusLocked(aStatus);
}

nsresult
CacheFileInputStream::CloseWithStatusLocked(nsresult aStatus)
{
  LOG(("CacheFileInputStream::CloseWithStatusLocked() [this=%p, "
       "aStatus=0x%08x]", this, aStatus));

  MOZ_ASSERT(!mInReadSegments);

  if (mClosed) {
    MOZ_ASSERT(!mCallback);
    return NS_OK;
  }

  mClosed = true;
  mStatus = NS_FAILED(aStatus) ? aStatus : NS_BASE_STREAM_CLOSED;

  if (mChunk) {
    ReleaseChunk();
  }

  // TODO propagate error from input stream to other streams ???

  MaybeNotifyListener();

  return NS_OK;
}

NS_IMETHODIMP
CacheFileInputStream::AsyncWait(nsIInputStreamCallback *aCallback,
                                uint32_t aFlags,
                                uint32_t aRequestedCount,
                                nsIEventTarget *aEventTarget)
{
  CacheFileAutoLock lock(mFile);
  MOZ_ASSERT(!mInReadSegments);

  LOG(("CacheFileInputStream::AsyncWait() [this=%p, callback=%p, flags=%d, "
       "requestedCount=%d, eventTarget=%p]", this, aCallback, aFlags,
       aRequestedCount, aEventTarget));

  mCallback = aCallback;
  mCallbackFlags = aFlags;
  mCallbackTarget = aEventTarget;

  if (!mCallback) {
    if (mWaitingForUpdate) {
      mChunk->CancelWait(this);
      mWaitingForUpdate = false;
    }
    return NS_OK;
  }

  if (mClosed) {
    NotifyListener();
    return NS_OK;
  }

  EnsureCorrectChunk(false);

  MaybeNotifyListener();

  return NS_OK;
}

// nsISeekableStream
NS_IMETHODIMP
CacheFileInputStream::Seek(int32_t whence, int64_t offset)
{
  CacheFileAutoLock lock(mFile);
  MOZ_ASSERT(!mInReadSegments);

  LOG(("CacheFileInputStream::Seek() [this=%p, whence=%d, offset=%lld]",
       this, whence, offset));

  if (mClosed) {
    LOG(("CacheFileInputStream::Seek() - Stream is closed. [this=%p]", this));
    return NS_BASE_STREAM_CLOSED;
  }

  int64_t newPos = offset;
  switch (whence) {
    case NS_SEEK_SET:
      break;
    case NS_SEEK_CUR:
      newPos += mPos;
      break;
    case NS_SEEK_END:
      newPos += mFile->mDataSize;
      break;
    default:
      NS_ERROR("invalid whence");
      return NS_ERROR_INVALID_ARG;
  }
  mPos = newPos;
  EnsureCorrectChunk(false);

  LOG(("CacheFileInputStream::Seek() [this=%p, pos=%lld]", this, mPos));
  return NS_OK;
}

NS_IMETHODIMP
CacheFileInputStream::Tell(int64_t *_retval)
{
  CacheFileAutoLock lock(mFile);
  MOZ_ASSERT(!mInReadSegments);

  if (mClosed) {
    LOG(("CacheFileInputStream::Tell() - Stream is closed. [this=%p]", this));
    return NS_BASE_STREAM_CLOSED;
  }

  *_retval = mPos;

  LOG(("CacheFileInputStream::Tell() [this=%p, retval=%lld]", this, *_retval));
  return NS_OK;
}

NS_IMETHODIMP
CacheFileInputStream::SetEOF()
{
  MOZ_ASSERT(false, "Don't call SetEOF on cache input stream");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// CacheFileChunkListener
nsresult
CacheFileInputStream::OnChunkRead(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_CRASH("CacheFileInputStream::OnChunkRead should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileInputStream::OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_CRASH("CacheFileInputStream::OnChunkWritten should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileInputStream::OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                                       CacheFileChunk *aChunk)
{
  CacheFileAutoLock lock(mFile);
  MOZ_ASSERT(!mInReadSegments);

  LOG(("CacheFileInputStream::OnChunkAvailable() [this=%p, result=0x%08x, "
       "idx=%d, chunk=%p]", this, aResult, aChunkIdx, aChunk));

  MOZ_ASSERT(mListeningForChunk != -1);

  if (mListeningForChunk != static_cast<int64_t>(aChunkIdx)) {
    // This is not a chunk that we're waiting for
    LOG(("CacheFileInputStream::OnChunkAvailable() - Notification is for a "
         "different chunk. [this=%p, listeningForChunk=%lld]",
         this, mListeningForChunk));

    return NS_OK;
  }

  MOZ_ASSERT(!mChunk);
  MOZ_ASSERT(!mWaitingForUpdate);
  mListeningForChunk = -1;

  if (mClosed) {
    MOZ_ASSERT(!mCallback);

    LOG(("CacheFileInputStream::OnChunkAvailable() - Stream is closed, "
         "ignoring notification. [this=%p]", this));

    return NS_OK;
  }

  if (NS_SUCCEEDED(aResult)) {
    mChunk = aChunk;
  } else if (aResult != NS_ERROR_NOT_AVAILABLE) {
    // Close the stream with error. The consumer will receive this error later
    // in Read(), Available() etc. We need to handle NS_ERROR_NOT_AVAILABLE
    // differently since it is returned when the requested chunk is not
    // available and there is no writer that could create it, i.e. it means that
    // we've reached the end of the file.
    CloseWithStatusLocked(aResult);

    return NS_OK;
  }

  MaybeNotifyListener();

  return NS_OK;
}

nsresult
CacheFileInputStream::OnChunkUpdated(CacheFileChunk *aChunk)
{
  CacheFileAutoLock lock(mFile);
  MOZ_ASSERT(!mInReadSegments);

  LOG(("CacheFileInputStream::OnChunkUpdated() [this=%p, idx=%d]",
       this, aChunk->Index()));

  if (!mWaitingForUpdate) {
    LOG(("CacheFileInputStream::OnChunkUpdated() - Ignoring notification since "
         "mWaitingforUpdate == false. [this=%p]", this));

    return NS_OK;
  }
  else {
    mWaitingForUpdate = false;
  }

  MOZ_ASSERT(mChunk == aChunk);

  MaybeNotifyListener();

  return NS_OK;
}

void
CacheFileInputStream::ReleaseChunk()
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileInputStream::ReleaseChunk() [this=%p, idx=%d]",
       this, mChunk->Index()));

  if (mWaitingForUpdate) {
    LOG(("CacheFileInputStream::ReleaseChunk() - Canceling waiting for update. "
         "[this=%p]", this));

    mChunk->CancelWait(this);
    mWaitingForUpdate = false;
  }

  mFile->ReleaseOutsideLock(mChunk.forget().take());
}

void
CacheFileInputStream::EnsureCorrectChunk(bool aReleaseOnly)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileInputStream::EnsureCorrectChunk() [this=%p, releaseOnly=%d]",
       this, aReleaseOnly));

  nsresult rv;

  uint32_t chunkIdx = mPos / kChunkSize;

  if (mChunk) {
    if (mChunk->Index() == chunkIdx) {
      // we have a correct chunk
      LOG(("CacheFileInputStream::EnsureCorrectChunk() - Have correct chunk "
           "[this=%p, idx=%d]", this, chunkIdx));

      return;
    }
    else {
      ReleaseChunk();
    }
  }

  MOZ_ASSERT(!mWaitingForUpdate);

  if (aReleaseOnly)
    return;

  if (mListeningForChunk == static_cast<int64_t>(chunkIdx)) {
    // We're already waiting for this chunk
    LOG(("CacheFileInputStream::EnsureCorrectChunk() - Already listening for "
         "chunk %lld [this=%p]", mListeningForChunk, this));

    return;
  }

  rv = mFile->GetChunkLocked(chunkIdx, CacheFile::READER, this,
                             getter_AddRefs(mChunk));
  if (NS_FAILED(rv)) {
    LOG(("CacheFileInputStream::EnsureCorrectChunk() - GetChunkLocked failed. "
         "[this=%p, idx=%d, rv=0x%08x]", this, chunkIdx, rv));
    if (rv != NS_ERROR_NOT_AVAILABLE) {
      // Close the stream with error. The consumer will receive this error later
      // in Read(), Available() etc. We need to handle NS_ERROR_NOT_AVAILABLE
      // differently since it is returned when the requested chunk is not
      // available and there is no writer that could create it, i.e. it means
      // that we've reached the end of the file.
      CloseWithStatusLocked(rv);

      return;
    }
  } else if (!mChunk) {
    mListeningForChunk = static_cast<int64_t>(chunkIdx);
  }

  MaybeNotifyListener();
}

void
CacheFileInputStream::CanRead(int64_t *aCanRead, const char **aBuf)
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mChunk);
  MOZ_ASSERT(mPos / kChunkSize == mChunk->Index());

  uint32_t chunkOffset = mPos - (mPos / kChunkSize) * kChunkSize;
  *aCanRead = mChunk->DataSize() - chunkOffset;
  if (*aCanRead > 0) {
    *aBuf = mChunk->BufForReading() + chunkOffset;
  } else {
    *aBuf = nullptr;
    if (NS_FAILED(mChunk->GetStatus())) {
      CloseWithStatusLocked(mChunk->GetStatus());
    }
  }

  LOG(("CacheFileInputStream::CanRead() [this=%p, canRead=%lld]",
       this, *aCanRead));
}

void
CacheFileInputStream::NotifyListener()
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileInputStream::NotifyListener() [this=%p]", this));

  MOZ_ASSERT(mCallback);

  if (!mCallbackTarget)
    mCallbackTarget = NS_GetCurrentThread();

  nsCOMPtr<nsIInputStreamCallback> asyncCallback =
    NS_NewInputStreamReadyEvent(mCallback, mCallbackTarget);

  mCallback = nullptr;
  mCallbackTarget = nullptr;

  asyncCallback->OnInputStreamReady(this);
}

void
CacheFileInputStream::MaybeNotifyListener()
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileInputStream::MaybeNotifyListener() [this=%p, mCallback=%p, "
       "mClosed=%d, mStatus=0x%08x, mChunk=%p, mListeningForChunk=%lld, "
       "mWaitingForUpdate=%d]", this, mCallback.get(), mClosed, mStatus,
       mChunk.get(), mListeningForChunk, mWaitingForUpdate));

  if (!mCallback)
    return;

  if (mClosed || NS_FAILED(mStatus)) {
    NotifyListener();
    return;
  }

  if (!mChunk) {
    if (mListeningForChunk == -1) {
      // EOF, should we notify even if mCallbackFlags == WAIT_CLOSURE_ONLY ??
      NotifyListener();
    }
    return;
  }

  MOZ_ASSERT(mPos / kChunkSize == mChunk->Index());

  if (mWaitingForUpdate)
    return;

  int64_t canRead;
  const char *buf;
  CanRead(&canRead, &buf);
  if (NS_FAILED(mStatus)) {
    // CanRead() called CloseWithStatusLocked() which called
    // MaybeNotifyListener() so the listener was already notified. Stop here.
    MOZ_ASSERT(!mCallback);
    return;
  }

  if (canRead > 0) {
    if (!(mCallbackFlags & WAIT_CLOSURE_ONLY))
      NotifyListener();
  }
  else if (canRead == 0) {
    if (!mFile->mOutput) {
      // EOF
      NotifyListener();
    }
    else {
      mChunk->WaitForUpdate(this);
      mWaitingForUpdate = true;
    }
  }
  else {
    // Output have set EOF before mPos?
    MOZ_ASSERT(false, "SetEOF is currenty not implemented?!");
    NotifyListener();
  }
}

// Memory reporting

size_t
CacheFileInputStream::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  // Everything the stream keeps a reference to is already reported somewhere else.
  // mFile reports itself.
  // mChunk reported as part of CacheFile.
  // mCallback is usually CacheFile or a class that is reported elsewhere.
  return mallocSizeOf(this);
}

} // net
} // mozilla
