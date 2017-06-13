/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SnappyUncompressInputStream_h__
#define mozilla_SnappyUncompressInputStream_h__

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsISupportsImpl.h"
#include "SnappyFrameUtils.h"

namespace mozilla {

class SnappyUncompressInputStream final : public nsIInputStream
                                        , protected detail::SnappyFrameUtils
{
public:
  // Construct a new blocking stream to uncompress the given base stream.  The
  // base stream must be readable in its entirety without having to wait for it
  // to have more data (e.g. must be blocking, or must have all its data
  // already).  Specifically, the base stream must never return
  // NS_BASE_STREAM_WOULD_BLOCK when it's read.  The base stream does not have
  // to be buffered.
  explicit SnappyUncompressInputStream(nsIInputStream* aBaseStream);

private:
  virtual ~SnappyUncompressInputStream();

  // Parse the next chunk of data.  This may populate mBuffer and set
  // mBufferFillSize.  This should not be called when mBuffer already
  // contains data.
  nsresult ParseNextChunk(uint32_t* aBytesReadOut);

  // Convenience routine to Read() from the base stream until we get
  // the given number of bytes or reach EOF.
  //
  // aBuf           - The buffer to write the bytes into.
  // aCount         - Max number of bytes to read. If the stream closes
  //                  fewer bytes my be read.
  // aMinValidCount - A minimum expected number of bytes.  If we find
  //                  fewer than this many bytes, then return
  //                  NS_ERROR_CORRUPTED_CONTENT.  If nothing was read due
  //                  due to EOF (aBytesReadOut == 0), then NS_OK is returned.
  // aBytesReadOut  - An out parameter indicating how many bytes were read.
  nsresult ReadAll(char* aBuf, uint32_t aCount, uint32_t aMinValidCount,
                   uint32_t* aBytesReadOut);

  // Convenience routine to determine how many bytes of uncompressed data
  // we currently have in our buffer.
  size_t UncompressedLength() const;

  nsCOMPtr<nsIInputStream> mBaseStream;

  // Buffer to hold compressed data.  Must copy here since we need a large
  // flat buffer to run the uncompress process on. Always the same length
  // of SnappyFrameUtils::MaxCompressedBufferLength(snappy::kBlockSize)
  // bytes long.
  mozilla::UniquePtr<char[]> mCompressedBuffer;

  // Buffer storing the resulting uncompressed data.  Exactly snappy::kBlockSize
  // bytes long.
  mozilla::UniquePtr<char[]> mUncompressedBuffer;

  // Number of bytes of uncompressed data in mBuffer.
  size_t mUncompressedBytes;

  // Next byte of mBuffer to return in ReadSegments().  Must be less than
  // mBufferFillSize
  size_t mNextByte;

  // Next chunk in the stream that has been parsed during read-ahead.
  ChunkType mNextChunkType;

  // Length of next chunk's length that has been determined during read-ahead.
  size_t mNextChunkDataLength;

  // The stream must begin with a StreamIdentifier chunk.  Are we still
  // expecting it?
  bool mNeedFirstStreamIdentifier;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
};

} // namespace mozilla

#endif // mozilla_SnappyUncompressInputStream_h__
