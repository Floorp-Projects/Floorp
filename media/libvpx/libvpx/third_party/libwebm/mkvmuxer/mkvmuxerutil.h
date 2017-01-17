// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef MKVMUXER_MKVMUXERUTIL_H_
#define MKVMUXER_MKVMUXERUTIL_H_

#include <stdint.h>

namespace mkvmuxer {
class Cluster;
class Frame;
class IMkvWriter;

const uint64_t kEbmlUnknownValue = 0x01FFFFFFFFFFFFFFULL;
const int64_t kMaxBlockTimecode = 0x07FFFLL;

// Writes out |value| in Big Endian order. Returns 0 on success.
int32_t SerializeInt(IMkvWriter* writer, int64_t value, int32_t size);

// Returns the size in bytes of the element.
int32_t GetUIntSize(uint64_t value);
int32_t GetIntSize(int64_t value);
int32_t GetCodedUIntSize(uint64_t value);
uint64_t EbmlMasterElementSize(uint64_t type, uint64_t value);
uint64_t EbmlElementSize(uint64_t type, int64_t value);
uint64_t EbmlElementSize(uint64_t type, uint64_t value);
uint64_t EbmlElementSize(uint64_t type, float value);
uint64_t EbmlElementSize(uint64_t type, const char* value);
uint64_t EbmlElementSize(uint64_t type, const uint8_t* value, uint64_t size);
uint64_t EbmlDateElementSize(uint64_t type);

// Returns the size in bytes of the element assuming that the element was
// written using |fixed_size| bytes. If |fixed_size| is set to zero, then it
// computes the necessary number of bytes based on |value|.
uint64_t EbmlElementSize(uint64_t type, uint64_t value, uint64_t fixed_size);

// Creates an EBML coded number from |value| and writes it out. The size of
// the coded number is determined by the value of |value|. |value| must not
// be in a coded form. Returns 0 on success.
int32_t WriteUInt(IMkvWriter* writer, uint64_t value);

// Creates an EBML coded number from |value| and writes it out. The size of
// the coded number is determined by the value of |size|. |value| must not
// be in a coded form. Returns 0 on success.
int32_t WriteUIntSize(IMkvWriter* writer, uint64_t value, int32_t size);

// Output an Mkv master element. Returns true if the element was written.
bool WriteEbmlMasterElement(IMkvWriter* writer, uint64_t value, uint64_t size);

// Outputs an Mkv ID, calls |IMkvWriter::ElementStartNotify|, and passes the
// ID to |SerializeInt|. Returns 0 on success.
int32_t WriteID(IMkvWriter* writer, uint64_t type);

// Output an Mkv non-master element. Returns true if the element was written.
bool WriteEbmlElement(IMkvWriter* writer, uint64_t type, uint64_t value);
bool WriteEbmlElement(IMkvWriter* writer, uint64_t type, int64_t value);
bool WriteEbmlElement(IMkvWriter* writer, uint64_t type, float value);
bool WriteEbmlElement(IMkvWriter* writer, uint64_t type, const char* value);
bool WriteEbmlElement(IMkvWriter* writer, uint64_t type, const uint8_t* value,
                      uint64_t size);
bool WriteEbmlDateElement(IMkvWriter* writer, uint64_t type, int64_t value);

// Output an Mkv non-master element using fixed size. The element will be
// written out using exactly |fixed_size| bytes. If |fixed_size| is set to zero
// then it computes the necessary number of bytes based on |value|. Returns true
// if the element was written.
bool WriteEbmlElement(IMkvWriter* writer, uint64_t type, uint64_t value,
                      uint64_t fixed_size);

// Output a Mkv Frame. It decides the correct element to write (Block vs
// SimpleBlock) based on the parameters of the Frame.
uint64_t WriteFrame(IMkvWriter* writer, const Frame* const frame,
                    Cluster* cluster);

// Output a void element. |size| must be the entire size in bytes that will be
// void. The function will calculate the size of the void header and subtract
// it from |size|.
uint64_t WriteVoidElement(IMkvWriter* writer, uint64_t size);

// Returns the version number of the muxer in |major|, |minor|, |build|,
// and |revision|.
void GetVersion(int32_t* major, int32_t* minor, int32_t* build,
                int32_t* revision);

// Returns a random number to be used for UID, using |seed| to seed
// the random-number generator (see POSIX rand_r() for semantics).
uint64_t MakeUID(unsigned int* seed);

}  // namespace mkvmuxer

#endif  // MKVMUXER_MKVMUXERUTIL_H_
