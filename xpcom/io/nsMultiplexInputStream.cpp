/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The multiplex stream concatenates a list of input streams into a single
 * stream.
 */

#include "mozilla/Attributes.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Mutex.h"
#include "mozilla/SystemGroup.h"

#include "base/basictypes.h"

#include "nsMultiplexInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsISeekableStream.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIClassInfoImpl.h"
#include "nsIIPCSerializableInputStream.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsIAsyncInputStream.h"

using namespace mozilla;
using namespace mozilla::ipc;

using mozilla::DeprecatedAbs;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

class nsMultiplexInputStream final
  : public nsIMultiplexInputStream
  , public nsISeekableStream
  , public nsIIPCSerializableInputStream
  , public nsICloneableInputStream
  , public nsIAsyncInputStream
  , public nsIInputStreamCallback
{
public:
  nsMultiplexInputStream();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIMULTIPLEXINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMCALLBACK

  void AsyncWaitCompleted();

  struct StreamData
  {
    void Initialize(nsIInputStream* aStream)
    {
      mStream = aStream;
      mAsyncStream = do_QueryInterface(aStream);
      mSeekableStream = do_QueryInterface(aStream);
    }

    nsCOMPtr<nsIInputStream> mStream;

    // This can be null.
    nsCOMPtr<nsIAsyncInputStream> mAsyncStream;
    // This can be null.
    nsCOMPtr<nsISeekableStream> mSeekableStream;
  };

private:
  ~nsMultiplexInputStream()
  {
  }

  // This method updates mSeekableStreams, mIPCSerializableStreams,
  // mCloneableStreams and mAsyncInputStreams values.
  void UpdateQIMap(StreamData& aStream, int32_t aCount);

  struct MOZ_STACK_CLASS ReadSegmentsState
  {
    nsCOMPtr<nsIInputStream> mThisStream;
    uint32_t mOffset;
    nsWriteSegmentFun mWriter;
    void* mClosure;
    bool mDone;
  };

  static nsresult ReadSegCb(nsIInputStream* aIn, void* aClosure,
                            const char* aFromRawSegment, uint32_t aToOffset,
                            uint32_t aCount, uint32_t* aWriteCount);

  bool IsSeekable() const;
  bool IsIPCSerializable() const;
  bool IsCloneable() const;
  bool IsAsyncInputStream() const;

  Mutex mLock; // Protects access to all data members.

  nsTArray<StreamData> mStreams;

  uint32_t mCurrentStream;
  bool mStartedReadingCurrent;
  nsresult mStatus;
  nsCOMPtr<nsIInputStreamCallback> mAsyncWaitCallback;

  uint32_t mSeekableStreams;
  uint32_t mIPCSerializableStreams;
  uint32_t mCloneableStreams;
  uint32_t mAsyncInputStreams;
};

NS_IMPL_ADDREF(nsMultiplexInputStream)
NS_IMPL_RELEASE(nsMultiplexInputStream)

NS_IMPL_CLASSINFO(nsMultiplexInputStream, nullptr, nsIClassInfo::THREADSAFE,
                  NS_MULTIPLEXINPUTSTREAM_CID)

NS_INTERFACE_MAP_BEGIN(nsMultiplexInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIMultiplexInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISeekableStream, IsSeekable())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     IsIPCSerializable())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICloneableInputStream,
                                     IsCloneable())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStream,
                                     IsAsyncInputStream())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamCallback,
                                     IsAsyncInputStream())
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMultiplexInputStream)
  NS_IMPL_QUERY_CLASSINFO(nsMultiplexInputStream)
NS_INTERFACE_MAP_END

NS_IMPL_CI_INTERFACE_GETTER(nsMultiplexInputStream,
                            nsIMultiplexInputStream,
                            nsIInputStream,
                            nsISeekableStream)

static nsresult
AvailableMaybeSeek(nsMultiplexInputStream::StreamData& aStream,
                   uint64_t* aResult)
{
  nsresult rv = aStream.mStream->Available(aResult);
  if (rv == NS_BASE_STREAM_CLOSED) {
    // Blindly seek to the current position if Available() returns
    // NS_BASE_STREAM_CLOSED.
    // If nsIFileInputStream is closed in Read() due to CLOSE_ON_EOF flag,
    // Seek() could reopen the file if REOPEN_ON_REWIND flag is set.
    if (aStream.mSeekableStream) {
      nsresult rvSeek =
        aStream.mSeekableStream->Seek(nsISeekableStream::NS_SEEK_CUR, 0);
      if (NS_SUCCEEDED(rvSeek)) {
        rv = aStream.mStream->Available(aResult);
      }
    }
  }
  return rv;
}

static nsresult
TellMaybeSeek(nsISeekableStream* aSeekable, int64_t* aResult)
{
  nsresult rv = aSeekable->Tell(aResult);
  if (rv == NS_BASE_STREAM_CLOSED) {
    // Blindly seek to the current position if Tell() returns
    // NS_BASE_STREAM_CLOSED.
    // If nsIFileInputStream is closed in Read() due to CLOSE_ON_EOF flag,
    // Seek() could reopen the file if REOPEN_ON_REWIND flag is set.
    nsresult rvSeek = aSeekable->Seek(nsISeekableStream::NS_SEEK_CUR, 0);
    if (NS_SUCCEEDED(rvSeek)) {
      rv = aSeekable->Tell(aResult);
    }
  }
  return rv;
}

nsMultiplexInputStream::nsMultiplexInputStream()
  : mLock("nsMultiplexInputStream lock")
  , mCurrentStream(0)
  , mStartedReadingCurrent(false)
  , mStatus(NS_OK)
  , mSeekableStreams(0)
  , mIPCSerializableStreams(0)
  , mCloneableStreams(0)
  , mAsyncInputStreams(0)
{}

NS_IMETHODIMP
nsMultiplexInputStream::GetCount(uint32_t* aCount)
{
  MutexAutoLock lock(mLock);
  *aCount = mStreams.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsMultiplexInputStream::AppendStream(nsIInputStream* aStream)
{
  MutexAutoLock lock(mLock);

  StreamData* streamData = mStreams.AppendElement();
  if (NS_WARN_IF(!streamData)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  streamData->Initialize(aStream);

  UpdateQIMap(*streamData, 1);

  if (mStatus == NS_BASE_STREAM_CLOSED) {
    // We were closed, but now we have more data to read.
    mStatus = NS_OK;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMultiplexInputStream::InsertStream(nsIInputStream* aStream, uint32_t aIndex)
{
  MutexAutoLock lock(mLock);

  StreamData* streamData = mStreams.InsertElementAt(aIndex);
  if (NS_WARN_IF(!streamData)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  streamData->Initialize(aStream);

  if (mCurrentStream > aIndex ||
      (mCurrentStream == aIndex && mStartedReadingCurrent)) {
    ++mCurrentStream;
  } else if (mStatus == NS_BASE_STREAM_CLOSED) {
    // We were closed, but now we have more data to read.
    mStatus = NS_OK;
  }

  UpdateQIMap(*streamData, 1);
  return NS_OK;
}

NS_IMETHODIMP
nsMultiplexInputStream::RemoveStream(uint32_t aIndex)
{
  MutexAutoLock lock(mLock);
  if (aIndex >= mStreams.Length()) {
    return NS_ERROR_FAILURE;
  }

  UpdateQIMap(mStreams[aIndex], -1);

  mStreams.RemoveElementAt(aIndex);
  if (mCurrentStream > aIndex) {
    --mCurrentStream;
  } else if (mCurrentStream == aIndex) {
    mStartedReadingCurrent = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMultiplexInputStream::GetStream(uint32_t aIndex, nsIInputStream** aResult)
{
  MutexAutoLock lock(mLock);

  if (aIndex >= mStreams.Length()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  StreamData& streamData = mStreams.ElementAt(aIndex);

  nsCOMPtr<nsIInputStream> stream = streamData.mStream;
  stream.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsMultiplexInputStream::Close()
{
  nsTArray<nsCOMPtr<nsIInputStream>> streams;

  // Let's take a copy of the streams becuase, calling close() it could trigger
  // a nsIInputStreamCallback immediately and we don't want to create a deadlock
  // with mutex.
  {
    MutexAutoLock lock(mLock);
    uint32_t len = mStreams.Length();
    for (uint32_t i = 0; i < len; ++i) {
      streams.AppendElement(mStreams[i].mStream);
    }
    mStatus = NS_BASE_STREAM_CLOSED;
  }

  nsresult rv = NS_OK;

  uint32_t len = streams.Length();
  for (uint32_t i = 0; i < len; ++i) {
    nsresult rv2 = streams[i]->Close();
    // We still want to close all streams, but we should return an error
    if (NS_FAILED(rv2)) {
      rv = rv2;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsMultiplexInputStream::Available(uint64_t* aResult)
{
  *aResult = 0;

  MutexAutoLock lock(mLock);
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  uint64_t avail = 0;
  nsresult rv = NS_BASE_STREAM_CLOSED;

  uint32_t len = mStreams.Length();
  for (uint32_t i = mCurrentStream; i < len; i++) {
    uint64_t streamAvail;
    rv = AvailableMaybeSeek(mStreams[i], &streamAvail);
    if (rv == NS_BASE_STREAM_CLOSED) {
      // If a stream is closed, we continue with the next one.
      // If this is the current stream we move to the following stream.
      if (mCurrentStream == i) {
        ++mCurrentStream;
      }

      // If this is the last stream, we want to return this error code.
      continue;
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      mStatus = rv;
      return mStatus;
    }

    // If the current stream is async, we have to return what we have so far
    // without processing the following streams. This is needed because
    // ::Available should return only what is currently available. In case of an
    // nsIAsyncInputStream, we have to call AsyncWait() in order to read more.
    if (mStreams[i].mAsyncStream) {
      avail += streamAvail;
      break;
    }

    if (streamAvail == 0) {
      // Nothing to read for this stream. Let's move to the next one.
      continue;
    }

    avail += streamAvail;
  }

  // We still have something to read. We don't want to return an error code yet.
  if (avail) {
    *aResult = avail;
    return NS_OK;
  }

  // Let's propagate the last error message.
  mStatus = rv;
  return rv;
}

NS_IMETHODIMP
nsMultiplexInputStream::Read(char* aBuf, uint32_t aCount, uint32_t* aResult)
{
  MutexAutoLock lock(mLock);
  // It is tempting to implement this method in terms of ReadSegments, but
  // that would prevent this class from being used with streams that only
  // implement Read (e.g., file streams).

  *aResult = 0;

  if (mStatus == NS_BASE_STREAM_CLOSED) {
    return NS_OK;
  }
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  nsresult rv = NS_OK;

  uint32_t len = mStreams.Length();
  while (mCurrentStream < len && aCount) {
    uint32_t read;
    rv = mStreams[mCurrentStream].mStream->Read(aBuf, aCount, &read);

    // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
    // (This is a bug in those stream implementations)
    if (rv == NS_BASE_STREAM_CLOSED) {
      NS_NOTREACHED("Input stream's Read method returned NS_BASE_STREAM_CLOSED");
      rv = NS_OK;
      read = 0;
    } else if (NS_FAILED(rv)) {
      break;
    }

    if (read == 0) {
      ++mCurrentStream;
      mStartedReadingCurrent = false;
    } else {
      NS_ASSERTION(aCount >= read, "Read more than requested");
      *aResult += read;
      aCount -= read;
      aBuf += read;
      mStartedReadingCurrent = true;
    }
  }
  return *aResult ? NS_OK : rv;
}

NS_IMETHODIMP
nsMultiplexInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                     uint32_t aCount, uint32_t* aResult)
{
  MutexAutoLock lock(mLock);

  if (mStatus == NS_BASE_STREAM_CLOSED) {
    *aResult = 0;
    return NS_OK;
  }
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  NS_ASSERTION(aWriter, "missing aWriter");

  nsresult rv = NS_OK;
  ReadSegmentsState state;
  state.mThisStream = this;
  state.mOffset = 0;
  state.mWriter = aWriter;
  state.mClosure = aClosure;
  state.mDone = false;

  uint32_t len = mStreams.Length();
  while (mCurrentStream < len && aCount) {
    uint32_t read;
    rv = mStreams[mCurrentStream].mStream->ReadSegments(ReadSegCb, &state,
                                                        aCount, &read);

    // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
    // (This is a bug in those stream implementations)
    if (rv == NS_BASE_STREAM_CLOSED) {
      NS_NOTREACHED("Input stream's Read method returned NS_BASE_STREAM_CLOSED");
      rv = NS_OK;
      read = 0;
    }

    // if |aWriter| decided to stop reading segments...
    if (state.mDone || NS_FAILED(rv)) {
      break;
    }

    // if stream is empty, then advance to the next stream.
    if (read == 0) {
      ++mCurrentStream;
      mStartedReadingCurrent = false;
    } else {
      NS_ASSERTION(aCount >= read, "Read more than requested");
      state.mOffset += read;
      aCount -= read;
      mStartedReadingCurrent = true;
    }
  }

  // if we successfully read some data, then this call succeeded.
  *aResult = state.mOffset;
  return state.mOffset ? NS_OK : rv;
}

nsresult
nsMultiplexInputStream::ReadSegCb(nsIInputStream* aIn, void* aClosure,
                                  const char* aFromRawSegment,
                                  uint32_t aToOffset, uint32_t aCount,
                                  uint32_t* aWriteCount)
{
  nsresult rv;
  ReadSegmentsState* state = (ReadSegmentsState*)aClosure;
  rv = (state->mWriter)(state->mThisStream,
                        state->mClosure,
                        aFromRawSegment,
                        aToOffset + state->mOffset,
                        aCount,
                        aWriteCount);
  if (NS_FAILED(rv)) {
    state->mDone = true;
  }
  return rv;
}

NS_IMETHODIMP
nsMultiplexInputStream::IsNonBlocking(bool* aNonBlocking)
{
  MutexAutoLock lock(mLock);

  uint32_t len = mStreams.Length();
  if (len == 0) {
    // Claim to be non-blocking, since we won't block the caller.
    *aNonBlocking = true;
    return NS_OK;
  }
  for (uint32_t i = 0; i < len; ++i) {
    nsresult rv = mStreams[i].mStream->IsNonBlocking(aNonBlocking);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // If one is blocking the entire stream becomes blocking.
    if (!*aNonBlocking) {
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMultiplexInputStream::Seek(int32_t aWhence, int64_t aOffset)
{
  MutexAutoLock lock(mLock);

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  nsresult rv;

  uint32_t oldCurrentStream = mCurrentStream;
  bool oldStartedReadingCurrent = mStartedReadingCurrent;

  if (aWhence == NS_SEEK_SET) {
    int64_t remaining = aOffset;
    if (aOffset == 0) {
      mCurrentStream = 0;
    }
    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      nsCOMPtr<nsISeekableStream> stream = mStreams[i].mSeekableStream;
      if (!stream) {
        return NS_ERROR_FAILURE;
      }

      // See if all remaining streams should be rewound
      if (remaining == 0) {
        if (i < oldCurrentStream ||
            (i == oldCurrentStream && oldStartedReadingCurrent)) {
          rv = stream->Seek(NS_SEEK_SET, 0);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          continue;
        } else {
          break;
        }
      }

      // Get position in current stream
      int64_t streamPos;
      if (i > oldCurrentStream ||
          (i == oldCurrentStream && !oldStartedReadingCurrent)) {
        streamPos = 0;
      } else {
        rv = TellMaybeSeek(stream, &streamPos);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // See if we need to seek current stream forward or backward
      if (remaining < streamPos) {
        rv = stream->Seek(NS_SEEK_SET, remaining);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        mCurrentStream = i;
        mStartedReadingCurrent = remaining != 0;

        remaining = 0;
      } else if (remaining > streamPos) {
        if (i < oldCurrentStream) {
          // We're already at end so no need to seek this stream
          remaining -= streamPos;
          NS_ASSERTION(remaining >= 0, "Remaining invalid");
        } else {
          uint64_t avail;
          rv = AvailableMaybeSeek(mStreams[i], &avail);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          int64_t newPos = XPCOM_MIN(remaining, streamPos + (int64_t)avail);

          rv = stream->Seek(NS_SEEK_SET, newPos);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          mCurrentStream = i;
          mStartedReadingCurrent = true;

          remaining -= newPos;
          NS_ASSERTION(remaining >= 0, "Remaining invalid");
        }
      } else {
        NS_ASSERTION(remaining == streamPos, "Huh?");
        MOZ_ASSERT(remaining != 0, "Zero remaining should be handled earlier");
        remaining = 0;
        mCurrentStream = i;
        mStartedReadingCurrent = true;
      }
    }

    return NS_OK;
  }

  if (aWhence == NS_SEEK_CUR && aOffset > 0) {
    int64_t remaining = aOffset;
    for (uint32_t i = mCurrentStream; remaining && i < mStreams.Length(); ++i) {
      uint64_t avail;
      rv = AvailableMaybeSeek(mStreams[i], &avail);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      int64_t seek = XPCOM_MIN((int64_t)avail, remaining);

      rv = mStreams[i].mSeekableStream->Seek(NS_SEEK_CUR, seek);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mCurrentStream = i;
      mStartedReadingCurrent = true;

      remaining -= seek;
    }

    return NS_OK;
  }

  if (aWhence == NS_SEEK_CUR && aOffset < 0) {
    int64_t remaining = -aOffset;
    for (uint32_t i = mCurrentStream; remaining && i != (uint32_t)-1; --i) {
      int64_t pos;
      rv = TellMaybeSeek(mStreams[i].mSeekableStream, &pos);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      int64_t seek = XPCOM_MIN(pos, remaining);

      rv = mStreams[i].mSeekableStream->Seek(NS_SEEK_CUR, -seek);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mCurrentStream = i;
      mStartedReadingCurrent = seek != -pos;

      remaining -= seek;
    }

    return NS_OK;
  }

  if (aWhence == NS_SEEK_CUR) {
    NS_ASSERTION(aOffset == 0, "Should have handled all non-zero values");

    return NS_OK;
  }

  if (aWhence == NS_SEEK_END) {
    if (aOffset > 0) {
      return NS_ERROR_INVALID_ARG;
    }
    int64_t remaining = aOffset;
    for (uint32_t i = mStreams.Length() - 1; i != (uint32_t)-1; --i) {
      nsCOMPtr<nsISeekableStream> stream = mStreams[i].mSeekableStream;

      // See if all remaining streams should be seeked to end
      if (remaining == 0) {
        if (i >= oldCurrentStream) {
          rv = stream->Seek(NS_SEEK_END, 0);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        } else {
          break;
        }
      }

      // Get position in current stream
      int64_t streamPos;
      if (i < oldCurrentStream) {
        streamPos = 0;
      } else {
        uint64_t avail;
        rv = AvailableMaybeSeek(mStreams[i], &avail);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        streamPos = avail;
      }

      // See if we have enough data in the current stream.
      if (DeprecatedAbs(remaining) < streamPos) {
        rv = stream->Seek(NS_SEEK_END, remaining);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        mCurrentStream = i;
        mStartedReadingCurrent = true;

        remaining = 0;
      } else if (DeprecatedAbs(remaining) > streamPos) {
        if (i > oldCurrentStream ||
            (i == oldCurrentStream && !oldStartedReadingCurrent)) {
          // We're already at start so no need to seek this stream
          remaining += streamPos;
        } else {
          int64_t avail;
          rv = TellMaybeSeek(stream, &avail);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          int64_t newPos = streamPos + XPCOM_MIN(avail, DeprecatedAbs(remaining));

          rv = stream->Seek(NS_SEEK_END, -newPos);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          mCurrentStream = i;
          mStartedReadingCurrent = true;

          remaining += newPos;
        }
      } else {
        NS_ASSERTION(remaining == streamPos, "Huh?");
        remaining = 0;
      }
    }

    return NS_OK;
  }

  // other Seeks not implemented yet
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMultiplexInputStream::Tell(int64_t* aResult)
{
  MutexAutoLock lock(mLock);

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  nsresult rv;
  int64_t ret64 = 0;
  uint32_t i, last;
  last = mStartedReadingCurrent ? mCurrentStream + 1 : mCurrentStream;
  for (i = 0; i < last; ++i) {
    if (NS_WARN_IF(!mStreams[i].mSeekableStream)) {
      return NS_ERROR_NO_INTERFACE;
    }

    int64_t pos;
    rv = TellMaybeSeek(mStreams[i].mSeekableStream, &pos);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    ret64 += pos;
  }
  *aResult =  ret64;

  return NS_OK;
}

NS_IMETHODIMP
nsMultiplexInputStream::SetEOF()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMultiplexInputStream::CloseWithStatus(nsresult aStatus)
{
  return Close();
}

// This class is used to inform nsMultiplexInputStream that it's time to execute
// the asyncWait callback.
class AsyncWaitRunnable final : public CancelableRunnable
{
  RefPtr<nsMultiplexInputStream> mStream;

public:
  static void
  Create(nsMultiplexInputStream* aStream, nsIEventTarget* aEventTarget)
  {
    RefPtr<AsyncWaitRunnable> runnable = new AsyncWaitRunnable(aStream);
    if (aEventTarget) {
      aEventTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
    } else {
      runnable->Run();
    }
  }

  NS_IMETHOD
  Run() override
  {
    mStream->AsyncWaitCompleted();
    return NS_OK;
  }

private:
  explicit AsyncWaitRunnable(nsMultiplexInputStream* aStream)
    : CancelableRunnable("AsyncWaitRunnable")
    , mStream(aStream)
  {
    MOZ_ASSERT(aStream);
  }

};

NS_IMETHODIMP
nsMultiplexInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                                  uint32_t aFlags,
                                  uint32_t aRequestedCount,
                                  nsIEventTarget* aEventTarget)
{
  nsCOMPtr<nsIAsyncInputStream> stream;

  {
    MutexAutoLock lock(mLock);

    // We must execute the callback also when the stream is closed.
    if (NS_FAILED(mStatus) && mStatus != NS_BASE_STREAM_CLOSED) {
      return mStatus;
    }

    if (mAsyncWaitCallback && aCallback) {
      return NS_ERROR_FAILURE;
    }

    mAsyncWaitCallback = aCallback;

    if (!mAsyncWaitCallback) {
        return NS_OK;
    }

    // Let's take the current async stream if we are not already closed, and if
    // the currentStream value is still valid.
    if (mStatus != NS_BASE_STREAM_CLOSED &&
        mCurrentStream < mStreams.Length()) {
      stream = mStreams[mCurrentStream].mAsyncStream;
    }
  }

  MOZ_ASSERT_IF(stream, NS_SUCCEEDED(mStatus));

  // If we are here it's because we are already closed, or if the current stream
  // is not async. In both case we have to execute the callback.
  if (!stream) {
    AsyncWaitRunnable::Create(this, aEventTarget);
    return NS_OK;
  }

  return stream->AsyncWait(this, aFlags, aRequestedCount, aEventTarget);
}

NS_IMETHODIMP
nsMultiplexInputStream::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  AsyncWaitCompleted();
  return NS_OK;
}

void
nsMultiplexInputStream::AsyncWaitCompleted()
{
  nsCOMPtr<nsIInputStreamCallback> callback;

  {
    MutexAutoLock lock(mLock);

    // The callback has been nullified in the meantime.
    if (!mAsyncWaitCallback) {
      return;
    }

    mAsyncWaitCallback.swap(callback);
  }

  callback->OnInputStreamReady(this);
}

nsresult
nsMultiplexInputStreamConstructor(nsISupports* aOuter,
                                  REFNSIID aIID,
                                  void** aResult)
{
  *aResult = nullptr;

  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  RefPtr<nsMultiplexInputStream> inst = new nsMultiplexInputStream();

  return inst->QueryInterface(aIID, aResult);
}

void
nsMultiplexInputStream::Serialize(InputStreamParams& aParams,
                                  FileDescriptorArray& aFileDescriptors)
{
  MutexAutoLock lock(mLock);

  MultiplexInputStreamParams params;

  uint32_t streamCount = mStreams.Length();

  if (streamCount) {
    InfallibleTArray<InputStreamParams>& streams = params.streams();

    streams.SetCapacity(streamCount);
    for (uint32_t index = 0; index < streamCount; index++) {
      InputStreamParams childStreamParams;
      InputStreamHelper::SerializeInputStream(mStreams[index].mStream,
                                              childStreamParams,
                                              aFileDescriptors);

      streams.AppendElement(childStreamParams);
    }
  }

  params.currentStream() = mCurrentStream;
  params.status() = mStatus;
  params.startedReadingCurrent() = mStartedReadingCurrent;

  aParams = params;
}

bool
nsMultiplexInputStream::Deserialize(const InputStreamParams& aParams,
                                    const FileDescriptorArray& aFileDescriptors)
{
  if (aParams.type() !=
      InputStreamParams::TMultiplexInputStreamParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return false;
  }

  const MultiplexInputStreamParams& params =
    aParams.get_MultiplexInputStreamParams();

  const InfallibleTArray<InputStreamParams>& streams = params.streams();

  uint32_t streamCount = streams.Length();
  for (uint32_t index = 0; index < streamCount; index++) {
    nsCOMPtr<nsIInputStream> stream =
      InputStreamHelper::DeserializeInputStream(streams[index],
                                                aFileDescriptors);
    if (!stream) {
      NS_WARNING("Deserialize failed!");
      return false;
    }

    if (NS_FAILED(AppendStream(stream))) {
      NS_WARNING("AppendStream failed!");
      return false;
    }
  }

  mCurrentStream = params.currentStream();
  mStatus = params.status();
  mStartedReadingCurrent = params.startedReadingCurrent();

  return true;
}

Maybe<uint64_t>
nsMultiplexInputStream::ExpectedSerializedLength()
{
  MutexAutoLock lock(mLock);

  bool lengthValueExists = false;
  uint64_t expectedLength = 0;
  uint32_t streamCount = mStreams.Length();
  for (uint32_t index = 0; index < streamCount; index++) {
    nsCOMPtr<nsIIPCSerializableInputStream> stream =
      do_QueryInterface(mStreams[index].mStream);
    if (!stream) {
      continue;
    }
    Maybe<uint64_t> length = stream->ExpectedSerializedLength();
    if (length.isNothing()) {
      continue;
    }
    lengthValueExists = true;
    expectedLength += length.value();
  }
  return lengthValueExists ? Some(expectedLength) : Nothing();
}

NS_IMETHODIMP
nsMultiplexInputStream::GetCloneable(bool* aCloneable)
{
  MutexAutoLock lock(mLock);
  //XXXnsm Cloning a multiplex stream which has started reading is not permitted
  //right now.
  if (mCurrentStream > 0 || mStartedReadingCurrent) {
    *aCloneable = false;
    return NS_OK;
  }

  uint32_t len = mStreams.Length();
  for (uint32_t i = 0; i < len; ++i) {
    nsCOMPtr<nsICloneableInputStream> cis =
      do_QueryInterface(mStreams[i].mStream);
    if (!cis || !cis->GetCloneable()) {
      *aCloneable = false;
      return NS_OK;
    }
  }

  *aCloneable = true;
  return NS_OK;
}

NS_IMETHODIMP
nsMultiplexInputStream::Clone(nsIInputStream** aClone)
{
  MutexAutoLock lock(mLock);

  //XXXnsm Cloning a multiplex stream which has started reading is not permitted
  //right now.
  if (mCurrentStream > 0 || mStartedReadingCurrent) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsMultiplexInputStream> clone = new nsMultiplexInputStream();

  nsresult rv;
  uint32_t len = mStreams.Length();
  for (uint32_t i = 0; i < len; ++i) {
    nsCOMPtr<nsICloneableInputStream> substream =
      do_QueryInterface(mStreams[i].mStream);
    if (NS_WARN_IF(!substream)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIInputStream> clonedSubstream;
    rv = substream->Clone(getter_AddRefs(clonedSubstream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = clone->AppendStream(clonedSubstream);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  clone.forget(aClone);
  return NS_OK;
}

#define MAYBE_UPDATE_VALUE_REAL(x, y) \
  if (y) {                            \
    if (aCount == 1) {                \
      ++x;                            \
    } else if (x > 0) {               \
      --x;                            \
    } else {                          \
      MOZ_CRASH("A nsIInputStream changed QI map when stored in a nsMultiplexInputStream!"); \
    }                                 \
  }

#define MAYBE_UPDATE_VALUE(x, y)                                \
  {                                                             \
    nsCOMPtr<y> substream = do_QueryInterface(aStream.mStream); \
    MAYBE_UPDATE_VALUE_REAL(x, substream)                       \
  }

void
nsMultiplexInputStream::UpdateQIMap(StreamData& aStream, int32_t aCount)
{
  MOZ_ASSERT(aCount == -1 || aCount == 1);

  MAYBE_UPDATE_VALUE_REAL(mSeekableStreams, aStream.mSeekableStream)
  MAYBE_UPDATE_VALUE(mIPCSerializableStreams, nsIIPCSerializableInputStream)
  MAYBE_UPDATE_VALUE(mCloneableStreams, nsICloneableInputStream)
  MAYBE_UPDATE_VALUE_REAL(mAsyncInputStreams, aStream.mAsyncStream)
}

#undef MAYBE_UPDATE_VALUE

bool
nsMultiplexInputStream::IsSeekable() const
{
  return mStreams.Length() == mSeekableStreams;
}

bool
nsMultiplexInputStream::IsIPCSerializable() const
{
  return mStreams.Length() == mIPCSerializableStreams;
}

bool
nsMultiplexInputStream::IsCloneable() const
{
  return mStreams.Length() == mCloneableStreams;
}

bool
nsMultiplexInputStream::IsAsyncInputStream() const
{
  // nsMultiplexInputStream is nsIAsyncInputStream if at least 1 of the
  // substream implements that interface.
  return !!mAsyncInputStreams;
}
