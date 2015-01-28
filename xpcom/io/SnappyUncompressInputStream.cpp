/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SnappyUncompressInputStream.h"

#include <algorithm>
#include "nsIAsyncInputStream.h"
#include "nsStreamUtils.h"
#include "snappy/snappy.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(SnappyUncompressInputStream,
                  nsIInputStream);

static size_t kCompressedBufferLength =
  detail::SnappyFrameUtils::MaxCompressedBufferLength(snappy::kBlockSize);

SnappyUncompressInputStream::SnappyUncompressInputStream(nsIInputStream* aBaseStream)
  : mBaseStream(aBaseStream)
  , mUncompressedBytes(0)
  , mNextByte(0)
  , mNextChunkType(Unknown)
  , mNextChunkDataLength(0)
  , mNeedFirstStreamIdentifier(true)
{
  // This implementation only supports sync base streams.  Verify this in debug
  // builds.  Note, this is a bit complicated because the streams we support
  // advertise different capabilities:
  //  - nsFileInputStream - blocking and sync
  //  - nsStringInputStream - non-blocking and sync
  //  - nsPipeInputStream - can be blocking, but provides async interface
#ifdef DEBUG
  bool baseNonBlocking;
  nsresult rv = mBaseStream->IsNonBlocking(&baseNonBlocking);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (baseNonBlocking) {
    nsCOMPtr<nsIAsyncInputStream> async = do_QueryInterface(mBaseStream);
    MOZ_ASSERT(!async);
  }
#endif
}

NS_IMETHODIMP
SnappyUncompressInputStream::Close()
{
  if (!mBaseStream) {
    return NS_OK;
  }

  mBaseStream->Close();
  mBaseStream = nullptr;

  mUncompressedBuffer = nullptr;
  mCompressedBuffer = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
SnappyUncompressInputStream::Available(uint64_t* aLengthOut)
{
  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  // If we have uncompressed bytes, then we are done.
  *aLengthOut = UncompressedLength();
  if (*aLengthOut > 0) {
    return NS_OK;
  }

  // Otherwise, attempt to uncompress bytes until we get something or the
  // underlying stream is drained.  We loop here because some chunks can
  // be StreamIdentifiers, padding, etc with no data.
  uint32_t bytesRead;
  do {
    nsresult rv = ParseNextChunk(&bytesRead);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    *aLengthOut = UncompressedLength();
  } while(*aLengthOut == 0 && bytesRead);

  return NS_OK;
}

NS_IMETHODIMP
SnappyUncompressInputStream::Read(char* aBuf, uint32_t aCount,
                                  uint32_t* aBytesReadOut)
{
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aBytesReadOut);
}

NS_IMETHODIMP
SnappyUncompressInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                          void* aClosure, uint32_t aCount,
                                          uint32_t* aBytesReadOut)
{
  *aBytesReadOut = 0;

  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  nsresult rv;

  // Do not try to use the base stream's ReadSegements here.  Its very
  // unlikely we will get a single buffer that contains all of the compressed
  // data and therefore would have to copy into our own buffer anyways.
  // Instead, focus on making efficient use of the Read() interface.

  while (aCount > 0) {
    // We have some decompressed data in our buffer.  Provide it to the
    // callers writer function.
    if (mUncompressedBytes > 0) {
      MOZ_ASSERT(mUncompressedBuffer);
      uint32_t remaining = UncompressedLength();
      uint32_t numToWrite = std::min(aCount, remaining);
      uint32_t numWritten;
      rv = aWriter(this, aClosure, &mUncompressedBuffer[mNextByte], *aBytesReadOut,
                   numToWrite, &numWritten);

      // As defined in nsIInputputStream.idl, do not pass writer func errors.
      if (NS_FAILED(rv)) {
        return NS_OK;
      }

      // End-of-file
      if (numWritten == 0) {
        return NS_OK;
      }

      *aBytesReadOut += numWritten;
      mNextByte += numWritten;
      MOZ_ASSERT(mNextByte <= mUncompressedBytes);

      if (mNextByte == mUncompressedBytes) {
        mNextByte = 0;
        mUncompressedBytes = 0;
      }

      aCount -= numWritten;

      continue;
    }

    // Otherwise uncompress the next chunk and loop.  Any resulting data
    // will set mUncompressedBytes which we check at the top of the loop.
    uint32_t bytesRead;
    rv = ParseNextChunk(&bytesRead);
    if (NS_FAILED(rv)) { return rv; }

    // If we couldn't read anything and there is no more data to provide
    // to the caller, then this is eof.
    if (bytesRead == 0 && mUncompressedBytes == 0) {
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
SnappyUncompressInputStream::IsNonBlocking(bool* aNonBlockingOut)
{
  *aNonBlockingOut = false;
  return NS_OK;
}

SnappyUncompressInputStream::~SnappyUncompressInputStream()
{
  Close();
}

nsresult
SnappyUncompressInputStream::ParseNextChunk(uint32_t* aBytesReadOut)
{
  // There must not be any uncompressed data already in mUncompressedBuffer.
  MOZ_ASSERT(mUncompressedBytes == 0);
  MOZ_ASSERT(mNextByte == 0);

  nsresult rv;
  *aBytesReadOut = 0;

  // Lazily create our two buffers so we can report OOM during stream
  // operation.  These allocations only happens once.  The buffers are reused
  // until the stream is closed.
  if (!mUncompressedBuffer) {
    mUncompressedBuffer.reset(new (fallible) char[snappy::kBlockSize]);
    if (NS_WARN_IF(!mUncompressedBuffer)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (!mCompressedBuffer) {
    mCompressedBuffer.reset(new (fallible) char[kCompressedBufferLength]);
    if (NS_WARN_IF(!mCompressedBuffer)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // We have no decompressed data and we also have not seen the start of stream
  // yet. Read and validate the StreamIdentifier chunk.  Also read the next
  // header to determine the size of the first real data chunk.
  if (mNeedFirstStreamIdentifier) {
    const uint32_t firstReadLength = kHeaderLength +
                                     kStreamIdentifierDataLength +
                                     kHeaderLength;
    MOZ_ASSERT(firstReadLength <= kCompressedBufferLength);

    rv = ReadAll(mCompressedBuffer.get(), firstReadLength, firstReadLength,
                 aBytesReadOut);
    if (NS_WARN_IF(NS_FAILED(rv)) || *aBytesReadOut == 0) { return rv; }

    rv = ParseHeader(mCompressedBuffer.get(), kHeaderLength,
                     &mNextChunkType, &mNextChunkDataLength);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    if (NS_WARN_IF(mNextChunkType != StreamIdentifier ||
                   mNextChunkDataLength != kStreamIdentifierDataLength)) {
      return NS_ERROR_CORRUPTED_CONTENT;
    }
    size_t offset = kHeaderLength;

    mNeedFirstStreamIdentifier = false;

    size_t numRead;
    size_t numWritten;
    rv = ParseData(mUncompressedBuffer.get(), snappy::kBlockSize, mNextChunkType,
                   &mCompressedBuffer[offset],
                   mNextChunkDataLength, &numWritten, &numRead);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    MOZ_ASSERT(numWritten == 0);
    MOZ_ASSERT(numRead == mNextChunkDataLength);
    offset += numRead;

    rv = ParseHeader(&mCompressedBuffer[offset], *aBytesReadOut - offset,
                     &mNextChunkType, &mNextChunkDataLength);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return NS_OK;
  }

  // We have no compressed data and we don't know how big the next chunk is.
  // This happens when we get an EOF pause in the middle of a stream and also
  // at the end of the stream.  Simply read the next header and return.  The
  // chunk body will be read on the next entry into this method.
  if (mNextChunkType == Unknown) {
    rv = ReadAll(mCompressedBuffer.get(), kHeaderLength, kHeaderLength,
                 aBytesReadOut);
    if (NS_WARN_IF(NS_FAILED(rv)) || *aBytesReadOut == 0) { return rv; }

    rv = ParseHeader(mCompressedBuffer.get(), kHeaderLength,
                     &mNextChunkType, &mNextChunkDataLength);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return NS_OK;
  }

  // We have no decompressed data, but we do know the size of the next chunk.
  // Read at least that much from the base stream.
  uint32_t readLength = mNextChunkDataLength;
  MOZ_ASSERT(readLength <= kCompressedBufferLength);

  // However, if there is enough data in the base stream, also read the next
  // chunk header.  This helps optimize the stream by avoiding many small reads.
  uint64_t avail;
  rv = mBaseStream->Available(&avail);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  if (avail >= (readLength + kHeaderLength)) {
    readLength += kHeaderLength;
    MOZ_ASSERT(readLength <= kCompressedBufferLength);
  }

  rv = ReadAll(mCompressedBuffer.get(), readLength, mNextChunkDataLength,
               aBytesReadOut);
  if (NS_WARN_IF(NS_FAILED(rv)) || *aBytesReadOut == 0) { return rv; }

  size_t numRead;
  size_t numWritten;
  rv = ParseData(mUncompressedBuffer.get(), snappy::kBlockSize, mNextChunkType,
                 mCompressedBuffer.get(), mNextChunkDataLength,
                 &numWritten, &numRead);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  MOZ_ASSERT(numRead == mNextChunkDataLength);

  mUncompressedBytes = numWritten;

  // If we were unable to directly read the next chunk header, then clear
  // our internal state.  We will have to perform a small read to get the
  // header the next time we enter this method.
  if (*aBytesReadOut <= mNextChunkDataLength) {
    mNextChunkType = Unknown;
    mNextChunkDataLength = 0;
    return NS_OK;
  }

  // We got the next chunk header.  Parse it so that we are ready to for the
  // next call into this method.
  rv = ParseHeader(&mCompressedBuffer[numRead], *aBytesReadOut - numRead,
                   &mNextChunkType, &mNextChunkDataLength);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return NS_OK;
}

nsresult
SnappyUncompressInputStream::ReadAll(char* aBuf, uint32_t aCount,
                                     uint32_t aMinValidCount,
                                     uint32_t* aBytesReadOut)
{
  MOZ_ASSERT(aCount >= aMinValidCount);

  *aBytesReadOut = 0;

  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  uint32_t offset = 0;
  while (aCount > 0) {
    uint32_t bytesRead = 0;
    nsresult rv = mBaseStream->Read(aBuf + offset, aCount, &bytesRead);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    // EOF, but don't immediately return.  We need to validate min read bytes
    // below.
    if (bytesRead == 0) {
      break;
    }

    *aBytesReadOut += bytesRead;
    offset += bytesRead;
    aCount -= bytesRead;
  }

  // Reading zero bytes is not an error.  Its the expected EOF condition.
  // Only compare to the minimum valid count if we read at least one byte.
  if (*aBytesReadOut != 0 && *aBytesReadOut < aMinValidCount) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  return NS_OK;
}

size_t
SnappyUncompressInputStream::UncompressedLength() const
{
  MOZ_ASSERT(mNextByte <= mUncompressedBytes);
  return mUncompressedBytes - mNextByte;
}

} // namespace mozilla
