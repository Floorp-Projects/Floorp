/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SnappyFrameUtils.h"

#include "crc32c.h"
#include "mozilla/Endian.h"
#include "nsDebug.h"
#include "snappy/snappy.h"

namespace {

using mozilla::detail::SnappyFrameUtils;
using mozilla::NativeEndian;

SnappyFrameUtils::ChunkType ReadChunkType(uint8_t aByte)
{
  if (aByte == 0xff)  {
    return SnappyFrameUtils::StreamIdentifier;
  } else if (aByte == 0x00) {
    return SnappyFrameUtils::CompressedData;
  } else if (aByte == 0x01) {
    return SnappyFrameUtils::UncompressedData;
  } else if (aByte == 0xfe) {
    return SnappyFrameUtils::Padding;
  }

  return SnappyFrameUtils::Reserved;
}

void WriteChunkType(char* aDest, SnappyFrameUtils::ChunkType aType)
{
  unsigned char* dest = reinterpret_cast<unsigned char*>(aDest);
  if (aType == SnappyFrameUtils::StreamIdentifier) {
    *dest = 0xff;
  } else if (aType == SnappyFrameUtils::CompressedData) {
    *dest = 0x00;
  } else if (aType == SnappyFrameUtils::UncompressedData) {
    *dest = 0x01;
  } else if (aType == SnappyFrameUtils::Padding) {
    *dest = 0xfe;
  } else {
    *dest = 0x02;
  }
}

void WriteUInt24(char* aBuf, uint32_t aVal)
{
  MOZ_ASSERT(!(aVal & 0xff000000));
  uint32_t tmp = NativeEndian::swapToLittleEndian(aVal);
  memcpy(aBuf, &tmp, 3);
}

uint32_t ReadUInt24(const char* aBuf)
{
  uint32_t val = 0;
  memcpy(&val, aBuf, 3);
  return NativeEndian::swapFromLittleEndian(val);
}

// This mask is explicitly defined in the snappy framing_format.txt file.
uint32_t MaskChecksum(uint32_t aValue)
{
  return ((aValue >> 15) | (aValue << 17)) + 0xa282ead8;
}

} // namespace

namespace mozilla {
namespace detail {

using mozilla::LittleEndian;

// static
nsresult
SnappyFrameUtils::WriteStreamIdentifier(char* aDest, size_t aDestLength,
                                        size_t* aBytesWrittenOut)
{
  if (NS_WARN_IF(aDestLength < (kHeaderLength + kStreamIdentifierDataLength))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  WriteChunkType(aDest, StreamIdentifier);
  aDest[1] = 0x06;  // Data length
  aDest[2] = 0x00;
  aDest[3] = 0x00;
  aDest[4] = 0x73;  // "sNaPpY"
  aDest[5] = 0x4e;
  aDest[6] = 0x61;
  aDest[7] = 0x50;
  aDest[8] = 0x70;
  aDest[9] = 0x59;

  static_assert(kHeaderLength + kStreamIdentifierDataLength == 10,
                "StreamIdentifier chunk should be exactly 10 bytes long");
  *aBytesWrittenOut = kHeaderLength + kStreamIdentifierDataLength;

  return NS_OK;
}

// static
nsresult
SnappyFrameUtils::WriteCompressedData(char* aDest, size_t aDestLength,
                                      const char* aData, size_t aDataLength,
                                      size_t* aBytesWrittenOut)
{
  *aBytesWrittenOut = 0;

  size_t neededLength = MaxCompressedBufferLength(aDataLength);
  if (NS_WARN_IF(aDestLength < neededLength)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  size_t offset = 0;

  WriteChunkType(aDest, CompressedData);
  offset += kChunkTypeLength;

  // Skip length for now and write it out after we know the compressed length.
  size_t lengthOffset = offset;
  offset += kChunkLengthLength;

  uint32_t crc = ComputeCrc32c(~0, reinterpret_cast<const unsigned char*>(aData),
                               aDataLength);
  uint32_t maskedCrc = MaskChecksum(crc);
  LittleEndian::writeUint32(aDest + offset, maskedCrc);
  offset += kCRCLength;

  size_t compressedLength;
  snappy::RawCompress(aData, aDataLength, aDest + offset, &compressedLength);

  // Go back and write the data length.
  size_t dataLength = compressedLength + kCRCLength;
  WriteUInt24(aDest + lengthOffset, dataLength);

  *aBytesWrittenOut = kHeaderLength + dataLength;

  return NS_OK;
}

// static
nsresult
SnappyFrameUtils::ParseHeader(const char* aSource, size_t aSourceLength,
                              ChunkType* aTypeOut, size_t* aDataLengthOut)
{
  if (NS_WARN_IF(aSourceLength < kHeaderLength)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aTypeOut = ReadChunkType(aSource[0]);
  *aDataLengthOut = ReadUInt24(aSource + kChunkTypeLength);

  return NS_OK;
}

// static
nsresult
SnappyFrameUtils::ParseData(char* aDest, size_t aDestLength,
                            ChunkType aType, const char* aData,
                            size_t aDataLength,
                            size_t* aBytesWrittenOut, size_t* aBytesReadOut)
{
  switch(aType) {
    case StreamIdentifier:
      return ParseStreamIdentifier(aDest, aDestLength, aData, aDataLength,
                                   aBytesWrittenOut, aBytesReadOut);

    case CompressedData:
      return ParseCompressedData(aDest, aDestLength, aData, aDataLength,
                                 aBytesWrittenOut, aBytesReadOut);

    // TODO: support other snappy chunk types
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported snappy framing chunk type.");
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

// static
nsresult
SnappyFrameUtils::ParseStreamIdentifier(char*, size_t,
                                        const char* aData, size_t aDataLength,
                                        size_t* aBytesWrittenOut,
                                        size_t* aBytesReadOut)
{
  *aBytesWrittenOut = 0;
  *aBytesReadOut = 0;
  if (NS_WARN_IF(aDataLength != kStreamIdentifierDataLength ||
                 aData[0] != 0x73 ||
                 aData[1] != 0x4e ||
                 aData[2] != 0x61 ||
                 aData[3] != 0x50 ||
                 aData[4] != 0x70 ||
                 aData[5] != 0x59)) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }
  *aBytesReadOut = aDataLength;
  return NS_OK;
}

// static
nsresult
SnappyFrameUtils::ParseCompressedData(char* aDest, size_t aDestLength,
                                      const char* aData, size_t aDataLength,
                                      size_t* aBytesWrittenOut,
                                      size_t* aBytesReadOut)
{
  *aBytesWrittenOut = 0;
  *aBytesReadOut = 0;
  size_t offset = 0;

  uint32_t readCrc = LittleEndian::readUint32(aData + offset);
  offset += kCRCLength;

  size_t uncompressedLength;
  if (NS_WARN_IF(!snappy::GetUncompressedLength(aData + offset,
                                                aDataLength - offset,
                                                &uncompressedLength))) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  if (NS_WARN_IF(aDestLength < uncompressedLength)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_WARN_IF(!snappy::RawUncompress(aData + offset, aDataLength - offset,
                                        aDest))) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  uint32_t crc = ComputeCrc32c(~0, reinterpret_cast<const unsigned char*>(aDest),
                               uncompressedLength);
  uint32_t maskedCrc = MaskChecksum(crc);
  if (NS_WARN_IF(readCrc != maskedCrc)) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  *aBytesWrittenOut = uncompressedLength;
  *aBytesReadOut = aDataLength;

  return NS_OK;
}

// static
size_t
SnappyFrameUtils::MaxCompressedBufferLength(size_t aSourceLength)
{
  size_t neededLength = kHeaderLength;
  neededLength += kCRCLength;
  neededLength += snappy::MaxCompressedLength(aSourceLength);
  return neededLength;
}

} // namespace detail
} // namespace mozilla
