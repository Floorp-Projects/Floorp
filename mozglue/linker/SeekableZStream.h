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

#endif /* SeekableZStream_h */
