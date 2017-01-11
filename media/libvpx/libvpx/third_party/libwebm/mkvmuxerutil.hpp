// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef MKVMUXERUTIL_HPP
#define MKVMUXERUTIL_HPP

#include "mkvmuxertypes.hpp"

namespace mkvmuxer {

class IMkvWriter;

const uint64 kEbmlUnknownValue = 0x01FFFFFFFFFFFFFFULL;
const int64 kMaxBlockTimecode = 0x07FFFLL;

// Writes out |value| in Big Endian order. Returns 0 on success.
int32 SerializeInt(IMkvWriter* writer, int64 value, int32 size);

// Returns the size in bytes of the element.
int32 GetUIntSize(uint64 value);
int32 GetCodedUIntSize(uint64 value);
uint64 EbmlMasterElementSize(uint64 type, uint64 value);
uint64 EbmlElementSize(uint64 type, int64 value);
uint64 EbmlElementSize(uint64 type, uint64 value);
uint64 EbmlElementSize(uint64 type, float value);
uint64 EbmlElementSize(uint64 type, const char* value);
uint64 EbmlElementSize(uint64 type, const uint8* value, uint64 size);
uint64 EbmlDateElementSize(uint64 type, int64 value);

// Creates an EBML coded number from |value| and writes it out. The size of
// the coded number is determined by the value of |value|. |value| must not
// be in a coded form. Returns 0 on success.
int32 WriteUInt(IMkvWriter* writer, uint64 value);

// Creates an EBML coded number from |value| and writes it out. The size of
// the coded number is determined by the value of |size|. |value| must not
// be in a coded form. Returns 0 on success.
int32 WriteUIntSize(IMkvWriter* writer, uint64 value, int32 size);

// Output an Mkv master element. Returns true if the element was written.
bool WriteEbmlMasterElement(IMkvWriter* writer, uint64 value, uint64 size);

// Outputs an Mkv ID, calls |IMkvWriter::ElementStartNotify|, and passes the
// ID to |SerializeInt|. Returns 0 on success.
int32 WriteID(IMkvWriter* writer, uint64 type);

// Output an Mkv non-master element. Returns true if the element was written.
bool WriteEbmlElement(IMkvWriter* writer, uint64 type, uint64 value);
bool WriteEbmlElement(IMkvWriter* writer, uint64 type, float value);
bool WriteEbmlElement(IMkvWriter* writer, uint64 type, const char* value);
bool WriteEbmlElement(IMkvWriter* writer, uint64 type, const uint8* value,
                      uint64 size);
bool WriteEbmlDateElement(IMkvWriter* writer, uint64 type, int64 value);

// Output an Mkv Simple Block.
// Inputs:
//   data:         Pointer to the data.
//   length:       Length of the data.
//   track_number: Track to add the data to. Value returned by Add track
//                  functions.  Only values in the range [1, 126] are
//                  permitted.
//   timecode:     Relative timecode of the Block.  Only values in the
//                  range [0, 2^15) are permitted.
//   is_key:       Non-zero value specifies that frame is a key frame.
uint64 WriteSimpleBlock(IMkvWriter* writer, const uint8* data, uint64 length,
                        uint64 track_number, int64 timecode, uint64 is_key);

// Output a metadata keyframe, using a Block Group element.
// Inputs:
//   data:         Pointer to the (meta)data.
//   length:       Length of the (meta)data.
//   track_number: Track to add the data to. Value returned by Add track
//                  functions.  Only values in the range [1, 126] are
//                  permitted.
//   timecode      Timecode of frame, relative to cluster timecode.  Only
//                  values in the range [0, 2^15) are permitted.
//   duration_timecode  Duration of frame, using timecode units.
uint64 WriteMetadataBlock(IMkvWriter* writer, const uint8* data, uint64 length,
                          uint64 track_number, int64 timecode,
                          uint64 duration_timecode);

// Output an Mkv Block with BlockAdditional data.
// Inputs:
//   data:         Pointer to the data.
//   length:       Length of the data.
//   additional:   Pointer to the additional data
//   additional_length: Length of the additional data.
//   add_id: Value of BlockAddID element.
//   track_number: Track to add the data to. Value returned by Add track
//                  functions.  Only values in the range [1, 126] are
//                  permitted.
//   timecode:     Relative timecode of the Block.  Only values in the
//                  range [0, 2^15) are permitted.
//   is_key:       Non-zero value specifies that frame is a key frame.
uint64 WriteBlockWithAdditional(IMkvWriter* writer, const uint8* data,
                                uint64 length, const uint8* additional,
                                uint64 additional_length, uint64 add_id,
                                uint64 track_number, int64 timecode,
                                uint64 is_key);

// Output an Mkv Block with a DiscardPadding element.
// Inputs:
//   data:            Pointer to the data.
//   length:          Length of the data.
//   discard_padding: DiscardPadding value.
//   track_number:    Track to add the data to. Value returned by Add track
//                    functions. Only values in the range [1, 126] are
//                    permitted.
//   timecode:        Relative timecode of the Block.  Only values in the
//                    range [0, 2^15) are permitted.
//   is_key:          Non-zero value specifies that frame is a key frame.
uint64 WriteBlockWithDiscardPadding(IMkvWriter* writer, const uint8* data,
                                    uint64 length, int64 discard_padding,
                                    uint64 track_number, int64 timecode,
                                    uint64 is_key);

// Output a void element. |size| must be the entire size in bytes that will be
// void. The function will calculate the size of the void header and subtract
// it from |size|.
uint64 WriteVoidElement(IMkvWriter* writer, uint64 size);

// Returns the version number of the muxer in |major|, |minor|, |build|,
// and |revision|.
void GetVersion(int32* major, int32* minor, int32* build, int32* revision);

// Returns a random number to be used for UID, using |seed| to seed
// the random-number generator (see POSIX rand_r() for semantics).
uint64 MakeUID(unsigned int* seed);

}  // end namespace mkvmuxer

#endif  // MKVMUXERUTIL_HPP
