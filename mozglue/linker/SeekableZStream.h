/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SeekableZStream_h
#define SeekableZStream_h

#include "Zip.h"

/**
 * Seekable compressed stream are created by splitting the original
 * decompressed data in small chunks and compress these chunks
 * individually.
 *
 * The seekable compressed file format consists in a header defined below,
 * followed by a table of 32-bits words containing the offsets for each
 * individual compressed chunk, then followed by the compressed chunks.
 */

#pragma pack(1)
struct SeekableZStreamHeader: public Zip::SignedEntity<SeekableZStreamHeader>
{
  SeekableZStreamHeader()
  : Zip::SignedEntity<SeekableZStreamHeader>(magic)
  , totalSize(0), chunkSize(0), nChunks(0), lastChunkSize(0) { }

  /* Reuse Zip::SignedEntity to handle the magic number used in the Seekable
   * ZStream file format. The magic number is "SeZz". */
  static const uint32_t magic = 0x7a5a6553;

  /* Total size of the stream, including the 4 magic bytes. */
  le_uint32 totalSize;

  /* Chunk size */
  le_uint32 chunkSize;

  /* Number of chunks */
  le_uint32 nChunks;

  /* Size of last chunk (> 0, <= Chunk size) */
  le_uint32 lastChunkSize;
};
#pragma pack()

MOZ_STATIC_ASSERT(sizeof(SeekableZStreamHeader) == 5 * 4,
                  "SeekableZStreamHeader should be 5 32-bits words");

/**
 * Helper class used to decompress Seekable ZStreams.
 */
class SeekableZStream {
public:
  /* Initialize from the given buffer. Returns whether initialization
   * succeeded (true) or failed (false). */
  bool Init(const void *buf);

  /* Decompresses starting from the given chunk. The decompressed data is
   * stored at the given location. The given length, in bytes, indicates
   * how much data to decompress. If length is 0, then exactly one chunk
   * is decompressed.
   * Returns whether decompression succeeded (true) or failed (false). */
  bool Decompress(void *where, size_t chunk, size_t length = 0);

  /* Decompresses the given chunk at the given address. If a length is given,
   * only decompresses that amount of data instead of the entire chunk.
   * Returns whether decompression succeeded (true) or failed (false). */
  bool DecompressChunk(void *where, size_t chunk, size_t length = 0);
 
  /* Returns the uncompressed size of the complete zstream */
  const size_t GetUncompressedSize() const
  {
    return (offsetTable.numElements() - 1) * chunkSize + lastChunkSize;
  }

  /* Returns the chunk size of the given chunk */
  const size_t GetChunkSize(size_t chunk = 0) const {
    return (chunk == offsetTable.numElements() - 1) ? lastChunkSize : chunkSize;
  }

  /* Returns the number of chunks */
  const size_t GetChunksNum() const {
    return offsetTable.numElements();
  }

private:
  /* RAW Seekable SZtream buffer */
  const unsigned char *buffer;

  /* Total size of the stream, including the 4 magic bytes. */
  uint32_t totalSize;

  /* Chunk size */
  uint32_t chunkSize;

  /* Size of last chunk (> 0, <= Chunk size) */
  uint32_t lastChunkSize;

  /* Offsets table */
  Array<le_uint32> offsetTable;
};

#endif /* SeekableZStream_h */
