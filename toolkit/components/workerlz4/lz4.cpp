/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Compression.h"

/**
 * LZ4 is a very fast byte-wise compression algorithm.
 *
 * Compared to Google's Snappy it is faster to compress and decompress and
 * generally produces output of about the same size.
 *
 * Compared to zlib it compresses at about 10x the speed, decompresses at about
 * 4x the speed and produces output of about 1.5x the size.
 *
 */

using namespace mozilla::Compression;

/**
 * Compresses 'inputSize' bytes from 'source' into 'dest'.
 * Destination buffer must be already allocated,
 * and must be sized to handle worst cases situations (input data not compressible)
 * Worst case size evaluation is provided by function LZ4_compressBound()
 *
 * @param inputSize is the input size. Max supported value is ~1.9GB
 * @param return the number of bytes written in buffer dest
 */
extern "C" MOZ_EXPORT size_t
workerlz4_compress(const char* source, size_t inputSize, char* dest) {
  return LZ4::compress(source, inputSize, dest);
}

/**
 * If the source stream is malformed, the function will stop decoding
 * and return a negative result, indicating the byte position of the
 * faulty instruction
 *
 * This function never writes outside of provided buffers, and never
 * modifies input buffer.
 *
 * note : destination buffer must be already allocated.
 *        its size must be a minimum of 'outputSize' bytes.
 * @param outputSize is the output size, therefore the original size
 * @return true/false
 */
extern "C" MOZ_EXPORT int
workerlz4_decompress(const char* source, size_t inputSize,
                     char* dest, size_t maxOutputSize,
                     size_t *bytesOutput) {
  return LZ4::decompress(source, inputSize,
                         dest, maxOutputSize,
                         bytesOutput);
}


/*
  Provides the maximum size that LZ4 may output in a "worst case"
  scenario (input data not compressible) primarily useful for memory
  allocation of output buffer.
  note : this function is limited by "int" range (2^31-1)

  @param inputSize is the input size. Max supported value is ~1.9GB
  @return maximum output size in a "worst case" scenario
*/
extern "C" MOZ_EXPORT size_t
workerlz4_maxCompressedSize(size_t inputSize)
{
  return LZ4::maxCompressedSize(inputSize);
}



