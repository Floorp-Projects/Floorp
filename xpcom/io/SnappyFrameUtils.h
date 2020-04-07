/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SnappyFrameUtils_h__
#define mozilla_SnappyFrameUtils_h__

#include <cstddef>

#include "mozilla/Attributes.h"
#include "nsError.h"

namespace mozilla {
namespace detail {

//
// Utility class providing primitives necessary to build streams based
// on the snappy compressor.  This essentially abstracts the framing format
// defined in:
//
//  other-licences/snappy/src/framing_format.txt
//
// NOTE: Currently only the StreamIdentifier and CompressedData chunks are
//       supported.
//
class SnappyFrameUtils {
 public:
  enum ChunkType {
    Unknown,
    StreamIdentifier,
    CompressedData,
    UncompressedData,
    Padding,
    Reserved,
    ChunkTypeCount
  };

  static const size_t kChunkTypeLength = 1;
  static const size_t kChunkLengthLength = 3;
  static const size_t kHeaderLength = kChunkTypeLength + kChunkLengthLength;
  static const size_t kStreamIdentifierDataLength = 6;
  static const size_t kCRCLength = 4;

  static nsresult WriteStreamIdentifier(char* aDest, size_t aDestLength,
                                        size_t* aBytesWrittenOut);

  static nsresult WriteCompressedData(char* aDest, size_t aDestLength,
                                      const char* aData, size_t aDataLength,
                                      size_t* aBytesWrittenOut);

  static nsresult ParseHeader(const char* aSource, size_t aSourceLength,
                              ChunkType* aTypeOut, size_t* aDataLengthOut);

  static nsresult ParseData(char* aDest, size_t aDestLength, ChunkType aType,
                            const char* aData, size_t aDataLength,
                            size_t* aBytesWrittenOut, size_t* aBytesReadOut);

  static nsresult ParseStreamIdentifier(char* aDest, size_t aDestLength,
                                        const char* aData, size_t aDataLength,
                                        size_t* aBytesWrittenOut,
                                        size_t* aBytesReadOut);

  static nsresult ParseCompressedData(char* aDest, size_t aDestLength,
                                      const char* aData, size_t aDataLength,
                                      size_t* aBytesWrittenOut,
                                      size_t* aBytesReadOut);

  static size_t MaxCompressedBufferLength(size_t aSourceLength);

 protected:
  SnappyFrameUtils() = default;
  virtual ~SnappyFrameUtils() = default;
};

}  // namespace detail
}  // namespace mozilla

#endif  // mozilla_SnappyFrameUtils_h__
