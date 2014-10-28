/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "SeekableZStream.h"
#include "Logging.h"

bool
SeekableZStream::Init(const void *buf, size_t length)
{
  const SeekableZStreamHeader *header = SeekableZStreamHeader::validate(buf);
  if (!header) {
    ERROR("Not a seekable zstream");
    return false;
  }

  buffer = reinterpret_cast<const unsigned char *>(buf);
  totalSize = header->totalSize;
  chunkSize = header->chunkSize;
  lastChunkSize = header->lastChunkSize;
  windowBits = header->windowBits;
  dictionary.Init(buffer + sizeof(SeekableZStreamHeader), header->dictSize);
  offsetTable.Init(buffer + sizeof(SeekableZStreamHeader) + header->dictSize,
                   header->nChunks);
  filter = GetFilter(header->filter);

  /* Sanity check */
  if ((chunkSize == 0) ||
      (!IsPageAlignedSize(chunkSize)) ||
      (chunkSize > 8 * PageSize()) ||
      (offsetTable.numElements() < 1) ||
      (lastChunkSize == 0) ||
      (lastChunkSize > chunkSize) ||
      (length < totalSize)) {
    ERROR("Malformed or broken seekable zstream");
    return false;
  }

  return true;
}

bool
SeekableZStream::Decompress(void *where, size_t chunk, size_t length)
{
  while (length) {
    size_t len = std::min(length, static_cast<size_t>(chunkSize));
    if (!DecompressChunk(where, chunk, len))
      return false;
    where = reinterpret_cast<unsigned char *>(where) + len;
    length -= len;
    chunk++;
  }
  return true;
}

bool
SeekableZStream::DecompressChunk(void *where, size_t chunk, size_t length)
{
  if (chunk >= offsetTable.numElements()) {
    ERROR("DecompressChunk: chunk #%" PRIdSize " out of range [0-%" PRIdSize ")",
        chunk, offsetTable.numElements());
    return false;
  }

  bool isLastChunk = (chunk == offsetTable.numElements() - 1);

  size_t chunkLen = isLastChunk ? lastChunkSize : chunkSize;

  if (length == 0 || length > chunkLen)
    length = chunkLen;

  DEBUG_LOG("DecompressChunk #%" PRIdSize " @%p (%" PRIdSize "/% " PRIdSize ")",
        chunk, where, length, chunkLen);
  zxx_stream zStream;
  zStream.avail_in = (isLastChunk ? totalSize : uint32_t(offsetTable[chunk + 1]))
                     - uint32_t(offsetTable[chunk]);
  zStream.next_in = const_cast<Bytef *>(buffer + uint32_t(offsetTable[chunk]));
  zStream.avail_out = length;
  zStream.next_out = reinterpret_cast<Bytef *>(where);

  /* Decompress chunk */
  if (inflateInit2(&zStream, windowBits) != Z_OK) {
    ERROR("inflateInit failed: %s", zStream.msg);
    return false;
  }
  if (dictionary && inflateSetDictionary(&zStream, dictionary,
                                         dictionary.numElements()) != Z_OK) {
    ERROR("inflateSetDictionary failed: %s", zStream.msg);
    return false;
  }
  if (inflate(&zStream, (length == chunkLen) ? Z_FINISH : Z_SYNC_FLUSH)
      != (length == chunkLen) ? Z_STREAM_END : Z_OK) {
    ERROR("inflate failed: %s", zStream.msg);
    return false;
  }
  if (inflateEnd(&zStream) != Z_OK) {
    ERROR("inflateEnd failed: %s", zStream.msg);
    return false;
  }
  if (filter)
    filter(chunk * chunkSize, UNFILTER, (unsigned char *)where, chunkLen);

  return true;
}

/* Branch/Call/Jump conversion filter for Thumb, derived from xz-utils
 * by Igor Pavlov and Lasse Collin, published in the public domain */
static void
BCJ_Thumb_filter(off_t offset, SeekableZStream::FilterDirection dir,
                 unsigned char *buf, size_t size)
{
  size_t i;
  for (i = 0; i + 4 <= size; i += 2) {
    if ((buf[i + 1] & 0xf8) == 0xf0 && (buf[i + 3] & 0xf8) == 0xf8) {
      uint32_t src = (buf[i] << 11)
                     | ((buf[i + 1] & 0x07) << 19)
                     | buf[i + 2]
                     | ((buf[i + 3] & 0x07) << 8);
      src <<= 1;
      uint32_t dest;
      if (dir == SeekableZStream::FILTER)
        dest = offset + (uint32_t)(i) + 4 + src;
      else
        dest = src - (offset + (uint32_t)(i) + 4);

      dest >>= 1;
      buf[i] = dest >> 11;
      buf[i + 1] = 0xf0 | ((dest >> 19) & 0x07);
      buf[i + 2] = dest;
      buf[i + 3] = 0xf8 | ((dest >> 8) & 0x07);
      i += 2;
    }
  }
}

/* Branch/Call/Jump conversion filter for ARM, derived from xz-utils
 * by Igor Pavlov and Lasse Collin, published in the public domain */
static void
BCJ_ARM_filter(off_t offset, SeekableZStream::FilterDirection dir,
               unsigned char *buf, size_t size)
{
  size_t i;
  for (i = 0; i + 4 <= size; i += 4) {
    if (buf[i + 3] == 0xeb) {
      uint32_t src = buf[i]
                     | (buf[i + 1] << 8)
                     | (buf[i + 2] << 16);
      src <<= 2;
      uint32_t dest;
      if (dir == SeekableZStream::FILTER)
        dest = offset + (uint32_t)(i) + 8 + src;
      else
        dest = src - (offset + (uint32_t)(i) + 8);

      dest >>= 2;
      buf[i] = dest;
      buf[i + 1] = dest >> 8;
      buf[i + 2] = dest >> 16;
    }
  }
}

/* Branch/Call/Jump conversion filter for x86, derived from xz-utils
 * by Igor Pavlov and Lasse Collin, published in the public domain */

#define Test86MSByte(b) ((b) == 0 || (b) == 0xff)

static void
BCJ_X86_filter(off_t offset, SeekableZStream::FilterDirection dir,
               unsigned char *buf, size_t size)
{
  static const bool MASK_TO_ALLOWED_STATUS[8] =
    { true, true, true, false, true, false, false, false };

  static const uint32_t MASK_TO_BIT_NUMBER[8] =
    { 0, 1, 2, 2, 3, 3, 3, 3 };

  uint32_t prev_mask = 0;
  uint32_t prev_pos = 0;

  for (size_t i = 0; i + 5 <= size;) {
    uint8_t b = buf[i];
    if (b != 0xe8 && b != 0xe9) {
      ++i;
      continue;
    }

    const uint32_t off = offset + (uint32_t)(i) - prev_pos;
    prev_pos = offset + (uint32_t)(i);

    if (off > 5) {
      prev_mask = 0;
    } else {
      for (uint32_t i = 0; i < off; ++i) {
        prev_mask &= 0x77;
        prev_mask <<= 1;
      }
    }

    b = buf[i + 4];

    if (Test86MSByte(b) && MASK_TO_ALLOWED_STATUS[(prev_mask >> 1) & 0x7]
        && (prev_mask >> 1) < 0x10) {

      uint32_t src = ((uint32_t)(b) << 24)
                     | ((uint32_t)(buf[i + 3]) << 16)
                     | ((uint32_t)(buf[i + 2]) << 8)
                     | (buf[i + 1]);

      uint32_t dest;
      while (true) {
        if (dir == SeekableZStream::FILTER)
          dest = src + (offset + (uint32_t)(i) + 5);
        else
          dest = src - (offset + (uint32_t)(i) + 5);

        if (prev_mask == 0)
          break;

        const uint32_t i = MASK_TO_BIT_NUMBER[prev_mask >> 1];

        b = (uint8_t)(dest >> (24 - i * 8));

        if (!Test86MSByte(b))
          break;

        src = dest ^ ((1 << (32 - i * 8)) - 1);
      }

      buf[i + 4] = (uint8_t)(~(((dest >> 24) & 1) - 1));
      buf[i + 3] = (uint8_t)(dest >> 16);
      buf[i + 2] = (uint8_t)(dest >> 8);
      buf[i + 1] = (uint8_t)(dest);
      i += 5;
      prev_mask = 0;

    } else {
      ++i;
      prev_mask |= 1;
      if (Test86MSByte(b))
        prev_mask |= 0x10;
    }
  }
}

SeekableZStream::ZStreamFilter
SeekableZStream::GetFilter(SeekableZStream::FilterId id)
{
  switch (id) {
  case BCJ_THUMB:
    return BCJ_Thumb_filter;
  case BCJ_ARM:
    return BCJ_ARM_filter;
  case BCJ_X86:
    return BCJ_X86_filter;
  default:
    return nullptr;
  }
  return nullptr;
}
