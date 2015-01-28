/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SnappyCompressOutputStream.h"

#include <algorithm>
#include "nsStreamUtils.h"
#include "snappy/snappy.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(SnappyCompressOutputStream, nsIOutputStream);

// static
const size_t
SnappyCompressOutputStream::kMaxBlockSize = snappy::kBlockSize;

SnappyCompressOutputStream::SnappyCompressOutputStream(nsIOutputStream* aBaseStream,
                                                       size_t aBlockSize)
 : mBaseStream(aBaseStream)
 , mBlockSize(std::min(aBlockSize, kMaxBlockSize))
 , mNextByte(0)
 , mCompressedBufferLength(0)
 , mStreamIdentifierWritten(false)
{
  MOZ_ASSERT(mBlockSize > 0);

  // This implementation only supports sync base streams.  Verify this in debug
  // builds.  Note, this can be simpler than the check in
  // SnappyUncompressInputStream because we don't have to deal with the
  // nsStringInputStream oddness of being non-blocking and sync.
#ifdef DEBUG
  bool baseNonBlocking;
  nsresult rv = mBaseStream->IsNonBlocking(&baseNonBlocking);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(!baseNonBlocking);
#endif
}

size_t
SnappyCompressOutputStream::BlockSize() const
{
  return mBlockSize;
}

NS_IMETHODIMP
SnappyCompressOutputStream::Close()
{
  if (!mBaseStream) {
    return NS_OK;
  }

  nsresult rv = Flush();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  mBaseStream->Close();
  mBaseStream = nullptr;

  mBuffer = nullptr;
  mCompressedBuffer = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
SnappyCompressOutputStream::Flush()
{
  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  nsresult rv = FlushToBaseStream();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  mBaseStream->Flush();

  return NS_OK;
}

NS_IMETHODIMP
SnappyCompressOutputStream::Write(const char* aBuf, uint32_t aCount,
                                  uint32_t* aResultOut)
{
  return WriteSegments(NS_CopySegmentToBuffer, const_cast<char*>(aBuf), aCount,
                       aResultOut);
}

NS_IMETHODIMP
SnappyCompressOutputStream::WriteFrom(nsIInputStream*, uint32_t, uint32_t*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SnappyCompressOutputStream::WriteSegments(nsReadSegmentFun aReader,
                                          void* aClosure,
                                          uint32_t aCount,
                                          uint32_t* aBytesWrittenOut)
{
  *aBytesWrittenOut = 0;

  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (!mBuffer) {
    mBuffer.reset(new (fallible) char[mBlockSize]);
    if (NS_WARN_IF(!mBuffer)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  while (aCount > 0) {
    // Determine how much space is left in our flat, uncompressed buffer.
    MOZ_ASSERT(mNextByte <= mBlockSize);
    uint32_t remaining = mBlockSize - mNextByte;

    // If it is full, then compress and flush the data to the base stream.
    if (remaining == 0) {
      nsresult rv = FlushToBaseStream();
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      // Now the entire buffer should be available for copying.
      MOZ_ASSERT(!mNextByte);
      remaining = mBlockSize;
    }

    uint32_t numToRead = std::min(remaining, aCount);
    uint32_t numRead = 0;

    nsresult rv = aReader(this, aClosure, &mBuffer[mNextByte],
                          *aBytesWrittenOut, numToRead, &numRead);

    // As defined in nsIOutputStream.idl, do not pass reader func errors.
    if (NS_FAILED(rv)) {
      return NS_OK;
    }

    // End-of-file
    if (numRead == 0) {
      return NS_OK;
    }

    mNextByte += numRead;
    *aBytesWrittenOut += numRead;
    aCount -= numRead;
  }

  return NS_OK;
}

NS_IMETHODIMP
SnappyCompressOutputStream::IsNonBlocking(bool* aNonBlockingOut)
{
  *aNonBlockingOut = false;
  return NS_OK;
}

SnappyCompressOutputStream::~SnappyCompressOutputStream()
{
  Close();
}

nsresult
SnappyCompressOutputStream::FlushToBaseStream()
{
  MOZ_ASSERT(mBaseStream);

  // Lazily create the compressed buffer on our first flush.  This
  // allows us to report OOM during stream operation.  This buffer
  // will then get re-used until the stream is closed.
  if (!mCompressedBuffer) {
    mCompressedBufferLength = MaxCompressedBufferLength(mBlockSize);
    mCompressedBuffer.reset(new (fallible) char[mCompressedBufferLength]);
    if (NS_WARN_IF(!mCompressedBuffer)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // The first chunk must be a StreamIdentifier chunk.  Write it out
  // if we have not done so already.
  nsresult rv = MaybeFlushStreamIdentifier();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Compress the data to our internal compressed buffer.
  size_t compressedLength;
  rv = WriteCompressedData(mCompressedBuffer.get(), mCompressedBufferLength,
                           mBuffer.get(), mNextByte, &compressedLength);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  MOZ_ASSERT(compressedLength > 0);

  mNextByte = 0;

  // Write the compressed buffer out to the base stream.
  uint32_t numWritten = 0;
  rv = WriteAll(mCompressedBuffer.get(), compressedLength, &numWritten);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  MOZ_ASSERT(compressedLength == numWritten);

  return NS_OK;
}

nsresult
SnappyCompressOutputStream::MaybeFlushStreamIdentifier()
{
  MOZ_ASSERT(mCompressedBuffer);

  if (mStreamIdentifierWritten) {
    return NS_OK;
  }

  // Build the StreamIdentifier in our compressed buffer.
  size_t compressedLength;
  nsresult rv = WriteStreamIdentifier(mCompressedBuffer.get(),
                                      mCompressedBufferLength,
                                      &compressedLength);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Write the compressed buffer out to the base stream.
  uint32_t numWritten = 0;
  rv = WriteAll(mCompressedBuffer.get(), compressedLength, &numWritten);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  MOZ_ASSERT(compressedLength == numWritten);

  mStreamIdentifierWritten = true;

  return NS_OK;
}

nsresult
SnappyCompressOutputStream::WriteAll(const char* aBuf, uint32_t aCount,
                                     uint32_t* aBytesWrittenOut)
{
  *aBytesWrittenOut = 0;

  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  uint32_t offset = 0;
  while (aCount > 0) {
    uint32_t numWritten = 0;
    nsresult rv = mBaseStream->Write(aBuf + offset, aCount, &numWritten);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    offset += numWritten;
    aCount -= numWritten;
    *aBytesWrittenOut += numWritten;
  }

  return NS_OK;
}

} // namespace mozilla
