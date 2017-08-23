/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XZSTREAM_h
#define XZSTREAM_h

#include <cstdlib>
#include <stdint.h>

#define XZ_DEC_DYNALLOC
#include "xz.h"

// Used to decode XZ stream buffers.
class XZStream
{
public:
  // Returns whether the provided buffer is likely a XZ stream.
  static bool IsXZ(const void* aBuf, size_t aBufSize);

  // Creates a XZ stream object for the given input buffer.
  XZStream(const void* aInBuf, size_t aInSize);
  ~XZStream();

  // Initializes the decoder and returns whether decoding may commence.
  bool Init();
  // Decodes the next chunk of input into the given output buffer.
  size_t Decode(void* aOutBuf, size_t aOutSize);
  // Returns the number of yet undecoded bytes in the input buffer.
  size_t RemainingInput() const;
  // Returns the total number of bytes in the input buffer (compressed size).
  size_t Size() const;
  // Returns the expected final number of bytes in the output buffer.
  // Note: will return 0 before successful Init().
  size_t UncompressedSize() const;

private:
  // Parses the stream footer and returns the size of the index in bytes.
  size_t ParseIndexSize() const;
  // Parses the stream index and returns the expected uncompressed size in bytes.
  size_t ParseUncompressedSize() const;

  const uint8_t* mInBuf;
  size_t mUncompSize;
  xz_buf mBuffers;
  xz_dec* mDec;
};

#endif // XZSTREAM_h
