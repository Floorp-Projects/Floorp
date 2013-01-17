/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "SeekableZStream.h"
#include "Logging.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifndef PAGE_MASK
#define PAGE_MASK (~ (PAGE_SIZE - 1))
#endif

bool
SeekableZStream::Init(const void *buf)
{
  const SeekableZStreamHeader *header = SeekableZStreamHeader::validate(buf);
  if (!header) {
    log("Not a seekable zstream");
    return false;
  }

  buffer = reinterpret_cast<const unsigned char *>(buf);
  totalSize = header->totalSize;
  chunkSize = header->chunkSize;
  lastChunkSize = header->lastChunkSize;
  offsetTable.Init(&header[1], header->nChunks);

  /* Sanity check */
  if ((chunkSize == 0) ||
      (chunkSize % PAGE_SIZE) ||
      (chunkSize > 8 * PAGE_SIZE) ||
      (offsetTable.numElements() < 1) ||
      (lastChunkSize == 0) ||
      (lastChunkSize > chunkSize)) {
    log("Malformed or broken seekable zstream");
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
    log("DecompressChunk: chunk #%" PRIdSize " out of range [0-%" PRIdSize ")",
        chunk, offsetTable.numElements());
    return false;
  }

  bool isLastChunk = (chunk == offsetTable.numElements() - 1);

  size_t chunkLen = isLastChunk ? lastChunkSize : chunkSize;

  if (length == 0 || length > chunkLen)
    length = chunkLen;

  debug("DecompressChunk #%" PRIdSize " @%p (%" PRIdSize "/% " PRIdSize ")",
        chunk, where, length, chunkLen);
  z_stream zStream;
  memset(&zStream, 0, sizeof(zStream));
  zStream.avail_in = (isLastChunk ? totalSize : uint32_t(offsetTable[chunk + 1]))
                     - uint32_t(offsetTable[chunk]);
  zStream.next_in = const_cast<Bytef *>(buffer + uint32_t(offsetTable[chunk]));
  zStream.avail_out = length;
  zStream.next_out = reinterpret_cast<Bytef *>(where);

  /* Decompress chunk */
  if (inflateInit(&zStream) != Z_OK) {
    log("inflateInit failed: %s", zStream.msg);
    return false;
  }
  if (inflate(&zStream, (length == chunkLen) ? Z_FINISH : Z_SYNC_FLUSH)
      != (length == chunkLen) ? Z_STREAM_END : Z_OK) {
    log("inflate failed: %s", zStream.msg);
    return false;
  }
  if (inflateEnd(&zStream) != Z_OK) {
    log("inflateEnd failed: %s", zStream.msg);
    return false;
  }
  return true;
}
