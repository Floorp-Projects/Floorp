/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileOutputStream.h"

#include "CacheFile.h"
#include "CacheEntry.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/DebugOnly.h"
#include <algorithm>

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(CacheFileOutputStream)
NS_IMETHODIMP_(MozExternalRefCountType)
CacheFileOutputStream::Release()
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "CacheFileOutputStream");

  if (0 == count) {
    mRefCnt = 1;
    {
      CacheFileAutoLock lock(mFile);
      mFile->RemoveOutput(this, mStatus);
    }
    delete (this);
    return 0;
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(CacheFileOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileChunkListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIOutputStream)
NS_INTERFACE_MAP_END_THREADSAFE

CacheFileOutputStream::CacheFileOutputStream(CacheFile *aFile,
                                             CacheOutputCloseListener *aCloseListener)
  : mFile(aFile)
  , mCloseListener(aCloseListener)
  , mPos(0)
  , mClosed(false)
  , mStatus(NS_OK)
  , mCallbackFlags(0)
{
  LOG(("CacheFileOutputStream::CacheFileOutputStream() [this=%p]", this));
  MOZ_COUNT_CTOR(CacheFileOutputStream);
}

CacheFileOutputStream::~CacheFileOutputStream()
{
  LOG(("CacheFileOutputStream::~CacheFileOutputStream() [this=%p]", this));
  MOZ_COUNT_DTOR(CacheFileOutputStream);
}

// nsIOutputStream
NS_IMETHODIMP
CacheFileOutputStream::Close()
{
  LOG(("CacheFileOutputStream::Close() [this=%p]", this));
  return CloseWithStatus(NS_OK);
}

NS_IMETHODIMP
CacheFileOutputStream::Flush()
{
  // TODO do we need to implement flush ???
  LOG(("CacheFileOutputStream::Flush() [this=%p]", this));
  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::Write(const char * aBuf, uint32_t aCount,
                             uint32_t *_retval)
{
  CacheFileAutoLock lock(mFile);

  LOG(("CacheFileOutputStream::Write() [this=%p, count=%d]", this, aCount));

  if (mClosed) {
    LOG(("CacheFileOutputStream::Write() - Stream is closed. [this=%p, "
         "status=0x%08x]", this, mStatus));

    return NS_FAILED(mStatus) ? mStatus : NS_BASE_STREAM_CLOSED;
  }

  if (CacheObserver::EntryIsTooBig(mPos + aCount, !mFile->mMemoryOnly)) {
    LOG(("CacheFileOutputStream::Write() - Entry is too big, failing and "
         "dooming the entry. [this=%p]", this));

    mFile->DoomLocked(nullptr);
    CloseWithStatusLocked(NS_ERROR_FILE_TOO_BIG);
    return NS_ERROR_FILE_TOO_BIG;
  }

  *_retval = aCount;

  while (aCount) {
    EnsureCorrectChunk(false);
    if (NS_FAILED(mStatus)) {
      return mStatus;
    }

    FillHole();
    if (NS_FAILED(mStatus)) {
      return mStatus;
    }

    uint32_t chunkOffset = mPos - (mPos / kChunkSize) * kChunkSize;
    uint32_t canWrite = kChunkSize - chunkOffset;
    uint32_t thisWrite = std::min(static_cast<uint32_t>(canWrite), aCount);
    nsresult rv = mChunk->EnsureBufSize(chunkOffset + thisWrite);
    if (NS_FAILED(rv)) {
      CloseWithStatusLocked(rv);
      return rv;
    }
    memcpy(mChunk->BufForWriting() + chunkOffset, aBuf, thisWrite);

    mPos += thisWrite;
    aBuf += thisWrite;
    aCount -= thisWrite;

    mChunk->UpdateDataSize(chunkOffset, thisWrite, false);
  }

  EnsureCorrectChunk(true);

  LOG(("CacheFileOutputStream::Write() - Wrote %d bytes [this=%p]",
       *_retval, this));

  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::WriteFrom(nsIInputStream *aFromStream, uint32_t aCount,
                                 uint32_t *_retval)
{
  LOG(("CacheFileOutputStream::WriteFrom() - NOT_IMPLEMENTED [this=%p, from=%p"
       ", count=%d]", this, aFromStream, aCount));

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::WriteSegments(nsReadSegmentFun aReader, void *aClosure,
                                     uint32_t aCount, uint32_t *_retval)
{
  LOG(("CacheFileOutputStream::WriteSegments() - NOT_IMPLEMENTED [this=%p, "
       "count=%d]", this, aCount));

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::IsNonBlocking(bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

// nsIAsyncOutputStream
NS_IMETHODIMP
CacheFileOutputStream::CloseWithStatus(nsresult aStatus)
{
  CacheFileAutoLock lock(mFile);

  LOG(("CacheFileOutputStream::CloseWithStatus() [this=%p, aStatus=0x%08x]",
       this, aStatus));

  return CloseWithStatusLocked(aStatus);
}

nsresult
CacheFileOutputStream::CloseWithStatusLocked(nsresult aStatus)
{
  LOG(("CacheFileOutputStream::CloseWithStatusLocked() [this=%p, "
       "aStatus=0x%08x]", this, aStatus));

  if (mClosed) {
    MOZ_ASSERT(!mCallback);
    return NS_OK;
  }

  mClosed = true;
  mStatus = NS_FAILED(aStatus) ? aStatus : NS_BASE_STREAM_CLOSED;

  if (mChunk) {
    ReleaseChunk();
  }

  if (mCallback) {
    NotifyListener();
  }

  mFile->RemoveOutput(this, mStatus);

  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::AsyncWait(nsIOutputStreamCallback *aCallback,
                                 uint32_t aFlags,
                                 uint32_t aRequestedCount,
                                 nsIEventTarget *aEventTarget)
{
  CacheFileAutoLock lock(mFile);

  LOG(("CacheFileOutputStream::AsyncWait() [this=%p, callback=%p, flags=%d, "
       "requestedCount=%d, eventTarget=%p]", this, aCallback, aFlags,
       aRequestedCount, aEventTarget));

  mCallback = aCallback;
  mCallbackFlags = aFlags;
  mCallbackTarget = aEventTarget;

  if (!mCallback)
    return NS_OK;

  // The stream is blocking so it is writable at any time
  if (mClosed || !(aFlags & WAIT_CLOSURE_ONLY))
    NotifyListener();

  return NS_OK;
}

// nsISeekableStream
NS_IMETHODIMP
CacheFileOutputStream::Seek(int32_t whence, int64_t offset)
{
  CacheFileAutoLock lock(mFile);

  LOG(("CacheFileOutputStream::Seek() [this=%p, whence=%d, offset=%lld]",
       this, whence, offset));

  if (mClosed) {
    LOG(("CacheFileOutputStream::Seek() - Stream is closed. [this=%p]", this));
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
  EnsureCorrectChunk(true);

  LOG(("CacheFileOutputStream::Seek() [this=%p, pos=%lld]", this, mPos));
  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::Tell(int64_t *_retval)
{
  CacheFileAutoLock lock(mFile);

  if (mClosed) {
    LOG(("CacheFileOutputStream::Tell() - Stream is closed. [this=%p]", this));
    return NS_BASE_STREAM_CLOSED;
  }

  *_retval = mPos;

  LOG(("CacheFileOutputStream::Tell() [this=%p, retval=%lld]", this, *_retval));
  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::SetEOF()
{
  MOZ_ASSERT(false, "CacheFileOutputStream::SetEOF() not implemented");
  // Right now we don't use SetEOF(). If we ever need this method, we need
  // to think about what to do with input streams that already points beyond
  // new EOF.
  return NS_ERROR_NOT_IMPLEMENTED;
}

// CacheFileChunkListener
nsresult
CacheFileOutputStream::OnChunkRead(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_CRASH("CacheFileOutputStream::OnChunkRead should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileOutputStream::OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_CRASH(
    "CacheFileOutputStream::OnChunkWritten should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileOutputStream::OnChunkAvailable(nsresult aResult,
                                        uint32_t aChunkIdx,
                                        CacheFileChunk *aChunk)
{
  MOZ_CRASH(
    "CacheFileOutputStream::OnChunkAvailable should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileOutputStream::OnChunkUpdated(CacheFileChunk *aChunk)
{
  MOZ_CRASH(
    "CacheFileOutputStream::OnChunkUpdated should not be called!");
  return NS_ERROR_UNEXPECTED;
}

void CacheFileOutputStream::NotifyCloseListener()
{
  nsRefPtr<CacheOutputCloseListener> listener;
  listener.swap(mCloseListener);
  if (!listener)
    return;

  listener->OnOutputClosed();
}

void
CacheFileOutputStream::ReleaseChunk()
{
  LOG(("CacheFileOutputStream::ReleaseChunk() [this=%p, idx=%d]",
       this, mChunk->Index()));

  mFile->ReleaseOutsideLock(mChunk.forget().take());
}

void
CacheFileOutputStream::EnsureCorrectChunk(bool aReleaseOnly)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileOutputStream::EnsureCorrectChunk() [this=%p, releaseOnly=%d]",
       this, aReleaseOnly));

  uint32_t chunkIdx = mPos / kChunkSize;

  if (mChunk) {
    if (mChunk->Index() == chunkIdx) {
      // we have a correct chunk
      LOG(("CacheFileOutputStream::EnsureCorrectChunk() - Have correct chunk "
           "[this=%p, idx=%d]", this, chunkIdx));

      return;
    }
    else {
      ReleaseChunk();
    }
  }

  if (aReleaseOnly)
    return;

  nsresult rv;
  rv = mFile->GetChunkLocked(chunkIdx, CacheFile::WRITER, nullptr,
                             getter_AddRefs(mChunk));
  if (NS_FAILED(rv)) {
    LOG(("CacheFileOutputStream::EnsureCorrectChunk() - GetChunkLocked failed. "
         "[this=%p, idx=%d, rv=0x%08x]", this, chunkIdx, rv));
    CloseWithStatusLocked(rv);
  }
}

void
CacheFileOutputStream::FillHole()
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mChunk);
  MOZ_ASSERT(mPos / kChunkSize == mChunk->Index());

  uint32_t pos = mPos - (mPos / kChunkSize) * kChunkSize;
  if (mChunk->DataSize() >= pos)
    return;

  LOG(("CacheFileOutputStream::FillHole() - Zeroing hole in chunk %d, range "
       "%d-%d [this=%p]", mChunk->Index(), mChunk->DataSize(), pos - 1, this));

  nsresult rv = mChunk->EnsureBufSize(pos);
  if (NS_FAILED(rv)) {
    CloseWithStatusLocked(rv);
    return;
  }

  memset(mChunk->BufForWriting() + mChunk->DataSize(), 0,
         pos - mChunk->DataSize());

  mChunk->UpdateDataSize(mChunk->DataSize(), pos - mChunk->DataSize(), false);
}

void
CacheFileOutputStream::NotifyListener()
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileOutputStream::NotifyListener() [this=%p]", this));

  MOZ_ASSERT(mCallback);

  if (!mCallbackTarget)
    mCallbackTarget = NS_GetCurrentThread();

  nsCOMPtr<nsIOutputStreamCallback> asyncCallback =
    NS_NewOutputStreamReadyEvent(mCallback, mCallbackTarget);

  mCallback = nullptr;
  mCallbackTarget = nullptr;

  asyncCallback->OnOutputStreamReady(this);
}

// Memory reporting

size_t
CacheFileOutputStream::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  // Everything the stream keeps a reference to is already reported somewhere else.
  // mFile reports itself.
  // mChunk reported as part of CacheFile.
  // mCloseListener is CacheEntry, already reported.
  // mCallback is usually CacheFile or a class that is reported elsewhere.
  return mallocSizeOf(this);
}

} // net
} // mozilla
