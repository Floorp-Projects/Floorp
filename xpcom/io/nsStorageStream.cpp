/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The storage stream provides an internal buffer that can be filled by a
 * client using a single output stream.  One or more independent input streams
 * can be created to read the data out non-destructively.  The implementation
 * uses a segmented buffer internally to avoid realloc'ing of large buffers,
 * with the attendant performance loss and heap fragmentation.
 */

#include "nsAlgorithm.h"
#include "nsStorageStream.h"
#include "nsSegmentedBuffer.h"
#include "nsStreamUtils.h"
#include "nsCOMPtr.h"
#include "nsICloneableInputStream.h"
#include "nsIInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsISeekableStream.h"
#include "mozilla/Logging.h"
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/ipc/InputStreamUtils.h"

using mozilla::Maybe;
using mozilla::Some;
using mozilla::ipc::InputStreamParams;
using mozilla::ipc::StringInputStreamParams;

//
// Log module for StorageStream logging...
//
// To enable logging (see prlog.h for full details):
//
//    set MOZ_LOG=StorageStreamLog:5
//    set MOZ_LOG_FILE=storage.log
//
// This enables LogLevel::Debug level information and places all output in
// the file storage.log.
//
static mozilla::LazyLogModule sStorageStreamLog("nsStorageStream");
#ifdef LOG
#  undef LOG
#endif
#define LOG(args) MOZ_LOG(sStorageStreamLog, mozilla::LogLevel::Debug, args)

nsStorageStream::nsStorageStream()
    : mSegmentedBuffer(0),
      mSegmentSize(0),
      mSegmentSizeLog2(0),
      mWriteInProgress(false),
      mLastSegmentNum(-1),
      mWriteCursor(0),
      mSegmentEnd(0),
      mLogicalLength(0) {
  LOG(("Creating nsStorageStream [%p].\n", this));
}

nsStorageStream::~nsStorageStream() { delete mSegmentedBuffer; }

NS_IMPL_ISUPPORTS(nsStorageStream, nsIStorageStream, nsIOutputStream)

NS_IMETHODIMP
nsStorageStream::Init(uint32_t aSegmentSize, uint32_t aMaxSize) {
  mSegmentedBuffer = new nsSegmentedBuffer();
  mSegmentSize = aSegmentSize;
  mSegmentSizeLog2 = mozilla::FloorLog2(aSegmentSize);

  // Segment size must be a power of two
  if (mSegmentSize != ((uint32_t)1 << mSegmentSizeLog2)) {
    return NS_ERROR_INVALID_ARG;
  }

  return mSegmentedBuffer->Init(aSegmentSize, aMaxSize);
}

NS_IMETHODIMP
nsStorageStream::GetOutputStream(int32_t aStartingOffset,
                                 nsIOutputStream** aOutputStream) {
  if (NS_WARN_IF(!aOutputStream)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!mSegmentedBuffer)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mWriteInProgress) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = Seek(aStartingOffset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Enlarge the last segment in the buffer so that it is the same size as
  // all the other segments in the buffer.  (It may have been realloc'ed
  // smaller in the Close() method.)
  if (mLastSegmentNum >= 0)
    if (mSegmentedBuffer->ReallocLastSegment(mSegmentSize)) {
      // Need to re-Seek, since realloc changed segment base pointer
      rv = Seek(aStartingOffset);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

  NS_ADDREF(this);
  *aOutputStream = static_cast<nsIOutputStream*>(this);
  mWriteInProgress = true;
  return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::Close() {
  if (NS_WARN_IF(!mSegmentedBuffer)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mWriteInProgress = false;

  int32_t segmentOffset = SegOffset(mLogicalLength);

  // Shrink the final segment in the segmented buffer to the minimum size
  // needed to contain the data, so as to conserve memory.
  if (segmentOffset) {
    mSegmentedBuffer->ReallocLastSegment(segmentOffset);
  }

  mWriteCursor = 0;
  mSegmentEnd = 0;

  LOG(("nsStorageStream [%p] Close mWriteCursor=%p mSegmentEnd=%p\n", this,
       mWriteCursor, mSegmentEnd));

  return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::Flush() { return NS_OK; }

NS_IMETHODIMP
nsStorageStream::Write(const char* aBuffer, uint32_t aCount,
                       uint32_t* aNumWritten) {
  if (NS_WARN_IF(!aNumWritten) || NS_WARN_IF(!aBuffer)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!mSegmentedBuffer)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  const char* readCursor;
  uint32_t count, availableInSegment, remaining;
  nsresult rv = NS_OK;

  LOG(("nsStorageStream [%p] Write mWriteCursor=%p mSegmentEnd=%p aCount=%d\n",
       this, mWriteCursor, mSegmentEnd, aCount));

  remaining = aCount;
  readCursor = aBuffer;
  // If no segments have been created yet, create one even if we don't have
  // to write any data; this enables creating an input stream which reads from
  // the very end of the data for any amount of data in the stream (i.e.
  // this stream contains N bytes of data and newInputStream(N) is called),
  // even for N=0 (with the caveat that we require .write("", 0) be called to
  // initialize internal buffers).
  bool firstTime = mSegmentedBuffer->GetSegmentCount() == 0;
  while (remaining || MOZ_UNLIKELY(firstTime)) {
    firstTime = false;
    availableInSegment = mSegmentEnd - mWriteCursor;
    if (!availableInSegment) {
      mWriteCursor = mSegmentedBuffer->AppendNewSegment();
      if (!mWriteCursor) {
        mSegmentEnd = 0;
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto out;
      }
      mLastSegmentNum++;
      mSegmentEnd = mWriteCursor + mSegmentSize;
      availableInSegment = mSegmentEnd - mWriteCursor;
      LOG(
          ("nsStorageStream [%p] Write (new seg) mWriteCursor=%p "
           "mSegmentEnd=%p\n",
           this, mWriteCursor, mSegmentEnd));
    }

    count = XPCOM_MIN(availableInSegment, remaining);
    memcpy(mWriteCursor, readCursor, count);
    remaining -= count;
    readCursor += count;
    mWriteCursor += count;
    LOG(
        ("nsStorageStream [%p] Writing mWriteCursor=%p mSegmentEnd=%p "
         "count=%d\n",
         this, mWriteCursor, mSegmentEnd, count));
  }

out:
  *aNumWritten = aCount - remaining;
  mLogicalLength += *aNumWritten;

  LOG(
      ("nsStorageStream [%p] Wrote mWriteCursor=%p mSegmentEnd=%p "
       "numWritten=%d\n",
       this, mWriteCursor, mSegmentEnd, *aNumWritten));
  return rv;
}

NS_IMETHODIMP
nsStorageStream::WriteFrom(nsIInputStream* aInStr, uint32_t aCount,
                           uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageStream::WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                               uint32_t aCount, uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageStream::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = false;
  return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::GetLength(uint32_t* aLength) {
  *aLength = mLogicalLength;
  return NS_OK;
}

// Truncate the buffer by deleting the end segments
NS_IMETHODIMP
nsStorageStream::SetLength(uint32_t aLength) {
  if (NS_WARN_IF(!mSegmentedBuffer)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mWriteInProgress) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aLength > mLogicalLength) {
    return NS_ERROR_INVALID_ARG;
  }

  int32_t newLastSegmentNum = SegNum(aLength);
  int32_t segmentOffset = SegOffset(aLength);
  if (segmentOffset == 0) {
    newLastSegmentNum--;
  }

  while (newLastSegmentNum < mLastSegmentNum) {
    mSegmentedBuffer->DeleteLastSegment();
    mLastSegmentNum--;
  }

  mLogicalLength = aLength;
  return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::GetWriteInProgress(bool* aWriteInProgress) {
  *aWriteInProgress = mWriteInProgress;
  return NS_OK;
}

nsresult nsStorageStream::Seek(int32_t aPosition) {
  if (NS_WARN_IF(!mSegmentedBuffer)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // An argument of -1 means "seek to end of stream"
  if (aPosition == -1) {
    aPosition = mLogicalLength;
  }

  // Seeking beyond the buffer end is illegal
  if ((uint32_t)aPosition > mLogicalLength) {
    return NS_ERROR_INVALID_ARG;
  }

  // Seeking backwards in the write stream results in truncation
  SetLength(aPosition);

  // Special handling for seek to start-of-buffer
  if (aPosition == 0) {
    mWriteCursor = 0;
    mSegmentEnd = 0;
    LOG(("nsStorageStream [%p] Seek mWriteCursor=%p mSegmentEnd=%p\n", this,
         mWriteCursor, mSegmentEnd));
    return NS_OK;
  }

  // Segment may have changed, so reset pointers
  mWriteCursor = mSegmentedBuffer->GetSegment(mLastSegmentNum);
  NS_ASSERTION(mWriteCursor, "null mWriteCursor");
  mSegmentEnd = mWriteCursor + mSegmentSize;

  // Adjust write cursor for current segment offset.  This test is necessary
  // because SegNum may reference the next-to-be-allocated segment, in which
  // case we need to be pointing at the end of the last segment.
  int32_t segmentOffset = SegOffset(aPosition);
  if (segmentOffset == 0 && (SegNum(aPosition) > (uint32_t)mLastSegmentNum)) {
    mWriteCursor = mSegmentEnd;
  } else {
    mWriteCursor += segmentOffset;
  }

  LOG(("nsStorageStream [%p] Seek mWriteCursor=%p mSegmentEnd=%p\n", this,
       mWriteCursor, mSegmentEnd));
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

// There can be many nsStorageInputStreams for a single nsStorageStream
class nsStorageInputStream final : public nsIInputStream,
                                   public nsISeekableStream,
                                   public nsIIPCSerializableInputStream,
                                   public nsICloneableInputStream {
 public:
  nsStorageInputStream(nsStorageStream* aStorageStream, uint32_t aSegmentSize)
      : mStorageStream(aStorageStream),
        mReadCursor(0),
        mSegmentEnd(0),
        mSegmentNum(0),
        mSegmentSize(aSegmentSize),
        mLogicalCursor(0),
        mStatus(NS_OK) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSITELLABLESTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM

 private:
  ~nsStorageInputStream() = default;

 protected:
  nsresult Seek(uint32_t aPosition);

  friend class nsStorageStream;

 private:
  RefPtr<nsStorageStream> mStorageStream;
  uint32_t mReadCursor;     // Next memory location to read byte, or 0
  uint32_t mSegmentEnd;     // One byte past end of current buffer segment
  uint32_t mSegmentNum;     // Segment number containing read cursor
  uint32_t mSegmentSize;    // All segments, except the last, are of this size
  uint32_t mLogicalCursor;  // Logical offset into stream
  nsresult mStatus;

  uint32_t SegNum(uint32_t aPosition) {
    return aPosition >> mStorageStream->mSegmentSizeLog2;
  }
  uint32_t SegOffset(uint32_t aPosition) {
    return aPosition & (mSegmentSize - 1);
  }

  template <typename M>
  void SerializeInternal(InputStreamParams& aParams, bool aDelayedStart,
                         uint32_t aMaxSize, uint32_t* aSizeUsed, M* aManager);
};

NS_IMPL_ISUPPORTS(nsStorageInputStream, nsIInputStream, nsISeekableStream,
                  nsITellableStream, nsIIPCSerializableInputStream,
                  nsICloneableInputStream)

NS_IMETHODIMP
nsStorageStream::NewInputStream(int32_t aStartingOffset,
                                nsIInputStream** aInputStream) {
  if (NS_WARN_IF(!mSegmentedBuffer)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsStorageInputStream> inputStream =
      new nsStorageInputStream(this, mSegmentSize);

  nsresult rv = inputStream->Seek(aStartingOffset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  inputStream.forget(aInputStream);
  return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Close() {
  mStatus = NS_BASE_STREAM_CLOSED;
  return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Available(uint64_t* aAvailable) {
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  *aAvailable = mStorageStream->mLogicalLength - mLogicalCursor;
  return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aNumRead) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuffer, aCount, aNumRead);
}

NS_IMETHODIMP
nsStorageInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                   uint32_t aCount, uint32_t* aNumRead) {
  *aNumRead = 0;
  if (mStatus == NS_BASE_STREAM_CLOSED) {
    return NS_OK;
  }
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  uint32_t count, availableInSegment, remainingCapacity, bytesConsumed;
  nsresult rv;

  remainingCapacity = aCount;
  while (remainingCapacity) {
    availableInSegment = mSegmentEnd - mReadCursor;
    if (!availableInSegment) {
      uint32_t available = mStorageStream->mLogicalLength - mLogicalCursor;
      if (!available) {
        goto out;
      }

      // We have data in the stream, but if mSegmentEnd is zero, then we
      // were likely constructed prior to any data being written into
      // the stream.  Therefore, if mSegmentEnd is non-zero, we should
      // move into the next segment; otherwise, we should stay in this
      // segment so our input state can be updated and we can properly
      // perform the initial read.
      if (mSegmentEnd > 0) {
        mSegmentNum++;
      }
      mReadCursor = 0;
      mSegmentEnd = XPCOM_MIN(mSegmentSize, available);
      availableInSegment = mSegmentEnd;
    }
    const char* cur = mStorageStream->mSegmentedBuffer->GetSegment(mSegmentNum);

    count = XPCOM_MIN(availableInSegment, remainingCapacity);
    rv = aWriter(this, aClosure, cur + mReadCursor, aCount - remainingCapacity,
                 count, &bytesConsumed);
    if (NS_FAILED(rv) || (bytesConsumed == 0)) {
      break;
    }
    remainingCapacity -= bytesConsumed;
    mReadCursor += bytesConsumed;
    mLogicalCursor += bytesConsumed;
  }

out:
  *aNumRead = aCount - remainingCapacity;

  bool isWriteInProgress = false;
  if (NS_FAILED(mStorageStream->GetWriteInProgress(&isWriteInProgress))) {
    isWriteInProgress = false;
  }

  if (*aNumRead == 0 && isWriteInProgress) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::IsNonBlocking(bool* aNonBlocking) {
  // TODO: This class should implement nsIAsyncInputStream so that callers
  // have some way of dealing with NS_BASE_STREAM_WOULD_BLOCK errors.

  *aNonBlocking = true;
  return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Seek(int32_t aWhence, int64_t aOffset) {
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  int64_t pos = aOffset;

  switch (aWhence) {
    case NS_SEEK_SET:
      break;
    case NS_SEEK_CUR:
      pos += mLogicalCursor;
      break;
    case NS_SEEK_END:
      pos += mStorageStream->mLogicalLength;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected whence value");
      return NS_ERROR_UNEXPECTED;
  }
  if (pos == int64_t(mLogicalCursor)) {
    return NS_OK;
  }

  return Seek(pos);
}

NS_IMETHODIMP
nsStorageInputStream::Tell(int64_t* aResult) {
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  *aResult = mLogicalCursor;
  return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::SetEOF() {
  MOZ_ASSERT_UNREACHABLE("nsStorageInputStream::SetEOF");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsStorageInputStream::Seek(uint32_t aPosition) {
  uint32_t length = mStorageStream->mLogicalLength;
  if (aPosition > length) {
    return NS_ERROR_INVALID_ARG;
  }

  if (length == 0) {
    return NS_OK;
  }

  mSegmentNum = SegNum(aPosition);
  mReadCursor = SegOffset(aPosition);
  uint32_t available = length - aPosition;
  mSegmentEnd = mReadCursor + XPCOM_MIN(mSegmentSize - mReadCursor, available);
  mLogicalCursor = aPosition;
  return NS_OK;
}

void nsStorageInputStream::Serialize(
    InputStreamParams& aParams, FileDescriptorArray&, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed,
    mozilla::ipc::ParentToChildStreamActorManager* aManager) {
  SerializeInternal(aParams, aDelayedStart, aMaxSize, aSizeUsed, aManager);
}

void nsStorageInputStream::Serialize(
    InputStreamParams& aParams, FileDescriptorArray&, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed,
    mozilla::ipc::ChildToParentStreamActorManager* aManager) {
  SerializeInternal(aParams, aDelayedStart, aMaxSize, aSizeUsed, aManager);
}

template <typename M>
void nsStorageInputStream::SerializeInternal(InputStreamParams& aParams,
                                             bool aDelayedStart,
                                             uint32_t aMaxSize,
                                             uint32_t* aSizeUsed, M* aManager) {
  MOZ_ASSERT(aSizeUsed);
  *aSizeUsed = 0;

  uint64_t remaining = 0;
  nsresult rv = Available(&remaining);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  if (remaining >= aMaxSize) {
    mozilla::ipc::InputStreamHelper::SerializeInputStreamAsPipe(
        this, aParams, aDelayedStart, aManager);
    return;
  }

  *aSizeUsed = remaining;

  nsCString combined;
  int64_t offset;
  rv = Tell(&offset);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  auto handle = combined.BulkWrite(remaining, 0, false, rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  uint32_t numRead = 0;

  rv = Read(handle.Elements(), remaining, &numRead);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  MOZ_ASSERT(numRead == remaining);
  handle.Finish(numRead, false);

  rv = Seek(NS_SEEK_SET, offset);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  StringInputStreamParams params;
  params.data() = combined;
  aParams = params;
}

bool nsStorageInputStream::Deserialize(const InputStreamParams& aParams,
                                       const FileDescriptorArray&) {
  MOZ_ASSERT_UNREACHABLE(
      "We should never attempt to deserialize a storage "
      "input stream.");
  return false;
}

NS_IMETHODIMP
nsStorageInputStream::GetCloneable(bool* aCloneableOut) {
  *aCloneableOut = true;
  return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Clone(nsIInputStream** aCloneOut) {
  return mStorageStream->NewInputStream(mLogicalCursor, aCloneOut);
}

nsresult NS_NewStorageStream(uint32_t aSegmentSize, uint32_t aMaxSize,
                             nsIStorageStream** aResult) {
  RefPtr<nsStorageStream> storageStream = new nsStorageStream();
  nsresult rv = storageStream->Init(aSegmentSize, aMaxSize);
  if (NS_FAILED(rv)) {
    return rv;
  }
  storageStream.forget(aResult);
  return NS_OK;
}

// Undefine LOG, so that other .cpp files (or their includes) won't complain
// about it already being defined, when we build in unified mode.
#undef LOG
