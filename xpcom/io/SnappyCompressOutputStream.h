/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SnappyCompressOutputStream_h__
#define mozilla_SnappyCompressOutputStream_h__

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "nsISupportsImpl.h"
#include "SnappyFrameUtils.h"

namespace mozilla {

class SnappyCompressOutputStream final : public nsIOutputStream
                                       , protected detail::SnappyFrameUtils
{
public:
  // Maximum compression block size.
  static const size_t kMaxBlockSize;

  // Construct a new blocking output stream to compress data to
  // the given base stream.  The base stream must also be blocking.
  // The compression block size may optionally be set to a value
  // up to kMaxBlockSize.
  explicit SnappyCompressOutputStream(nsIOutputStream* aBaseStream,
                                      size_t aBlockSize = kMaxBlockSize);

  // The compression block size.  To optimize stream performance
  // try to write to the stream in segments at least this size.
  size_t BlockSize() const;

private:
  virtual ~SnappyCompressOutputStream();

  nsresult FlushToBaseStream();
  nsresult MaybeFlushStreamIdentifier();
  nsresult WriteAll(const char* aBuf, uint32_t aCount,
                    uint32_t* aBytesWrittenOut);

  nsCOMPtr<nsIOutputStream> mBaseStream;
  const size_t mBlockSize;

  // Buffer holding copied uncompressed data.  This must be copied here
  // so that the compression can be performed on a single flat buffer.
  mozilla::UniquePtr<char[]> mBuffer;

  // The next byte in the uncompressed data to copy incoming data to.
  size_t mNextByte;

  // Buffer holding the resulting compressed data.
  mozilla::UniquePtr<char[]> mCompressedBuffer;
  size_t mCompressedBufferLength;

  // The first thing written to the stream must be a stream identifier.
  bool mStreamIdentifierWritten;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
};

} // namespace mozilla

#endif // mozilla_SnappyCompressOutputStream_h__
