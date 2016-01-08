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
  , totalSize(0), chunkSize(0), dictSize(0), nChunks(0), lastChunkSize(0)
  , windowBits(0), filter(0) { }

  /* Reuse Zip::SignedEntity to handle the magic number used in the Seekable
   * ZStream file format. The magic number is "SeZz". */
  static const uint32_t magic = 0x7a5a6553;

  /* Total size of the stream, including the 4 magic bytes. */
  le_uint32 totalSize;

  /* Chunk size */
  le_uint16 chunkSize;

  /* Size of the dictionary */
  le_uint16 dictSize;

  /* Number of chunks */
  le_uint32 nChunks;

  /* Size of last chunk (> 0, <= Chunk size) */
  le_uint16 lastChunkSize;

  /* windowBits value used when deflating */
  signed char windowBits;

  /* Filter Id */
  unsigned char filter;
};
#pragma pack()

static_assert(sizeof(SeekableZStreamHeader) == 5 * 4,
              "SeekableZStreamHeader should be 5 32-bits words");

/**
 * Helper class used to decompress Seekable ZStreams.
 */
class SeekableZStream {
public:
  /* Initialize from the given buffer. Returns whether initialization
   * succeeded (true) or failed (false). */
  bool Init(const void *buf, size_t length);

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
  size_t GetUncompressedSize() const
  {
    return (offsetTable.numElements() - 1) * chunkSize + lastChunkSize;
  }

  /* Returns the chunk size of the given chunk */
  size_t GetChunkSize(size_t chunk = 0) const {
    return (chunk == offsetTable.numElements() - 1) ? lastChunkSize : chunkSize;
  }

  /* Returns the number of chunks */
  size_t GetChunksNum() const {
    return offsetTable.numElements();
  }

  /**
   * Filters used to improve compression rate.
   */
  enum FilterDirection {
    FILTER,
    UNFILTER
  };
  typedef void (*ZStreamFilter)(off_t, FilterDirection,
                                  unsigned char *, size_t);

  enum FilterId {
    NONE,
    BCJ_THUMB,
    BCJ_ARM,
    BCJ_X86,
    FILTER_MAX
  };
  static ZStreamFilter GetFilter(FilterId id);

  static ZStreamFilter GetFilter(uint16_t id) {
    return GetFilter(static_cast<FilterId>(id));
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

  /* windowBits value used when deflating */
  int windowBits;

  /* Offsets table */
  Array<le_uint32> offsetTable;

  /* Filter */
  ZStreamFilter filter;

  /* Deflate dictionary */
  Array<unsigned char> dictionary;

  /* Special allocator for inflate to use the same buffers for every chunk */
  zxx_stream::StaticAllocator allocator;
};

inline void
operator++(SeekableZStream::FilterId &other)
{
  const int orig = static_cast<int>(other);
  other = static_cast<SeekableZStream::FilterId>(orig + 1);
}

#endif /* SeekableZStream_h */
