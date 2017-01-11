// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "mkvmuxerutil.hpp"

#ifdef __ANDROID__
#include <fcntl.h>
#endif

#include <cassert>
#include <cmath>
#include <cstdio>
#ifdef _MSC_VER
#define _CRT_RAND_S
#endif
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <new>

#include "mkvwriter.hpp"
#include "webmids.hpp"

namespace mkvmuxer {

namespace {

// Date elements are always 8 octets in size.
const int kDateElementSize = 8;

}  // namespace

int32 GetCodedUIntSize(uint64 value) {
  if (value < 0x000000000000007FULL)
    return 1;
  else if (value < 0x0000000000003FFFULL)
    return 2;
  else if (value < 0x00000000001FFFFFULL)
    return 3;
  else if (value < 0x000000000FFFFFFFULL)
    return 4;
  else if (value < 0x00000007FFFFFFFFULL)
    return 5;
  else if (value < 0x000003FFFFFFFFFFULL)
    return 6;
  else if (value < 0x0001FFFFFFFFFFFFULL)
    return 7;
  return 8;
}

int32 GetUIntSize(uint64 value) {
  if (value < 0x0000000000000100ULL)
    return 1;
  else if (value < 0x0000000000010000ULL)
    return 2;
  else if (value < 0x0000000001000000ULL)
    return 3;
  else if (value < 0x0000000100000000ULL)
    return 4;
  else if (value < 0x0000010000000000ULL)
    return 5;
  else if (value < 0x0001000000000000ULL)
    return 6;
  else if (value < 0x0100000000000000ULL)
    return 7;
  return 8;
}

uint64 EbmlMasterElementSize(uint64 type, uint64 value) {
  // Size of EBML ID
  int32 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += GetCodedUIntSize(value);

  return ebml_size;
}

uint64 EbmlElementSize(uint64 type, int64 value) {
  return EbmlElementSize(type, static_cast<uint64>(value));
}

uint64 EbmlElementSize(uint64 type, uint64 value) {
  // Size of EBML ID
  int32 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += GetUIntSize(value);

  // Size of Datasize
  ebml_size++;

  return ebml_size;
}

uint64 EbmlElementSize(uint64 type, float /* value */) {
  // Size of EBML ID
  uint64 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += sizeof(float);

  // Size of Datasize
  ebml_size++;

  return ebml_size;
}

uint64 EbmlElementSize(uint64 type, const char* value) {
  if (!value)
    return 0;

  // Size of EBML ID
  uint64 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += strlen(value);

  // Size of Datasize
  ebml_size++;

  return ebml_size;
}

uint64 EbmlElementSize(uint64 type, const uint8* value, uint64 size) {
  if (!value)
    return 0;

  // Size of EBML ID
  uint64 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += size;

  // Size of Datasize
  ebml_size += GetCodedUIntSize(size);

  return ebml_size;
}

uint64 EbmlDateElementSize(uint64 type, int64 value) {
  // Size of EBML ID
  uint64 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += kDateElementSize;

  // Size of Datasize
  ebml_size++;

  return ebml_size;
}

int32 SerializeInt(IMkvWriter* writer, int64 value, int32 size) {
  if (!writer || size < 1 || size > 8)
    return -1;

  for (int32 i = 1; i <= size; ++i) {
    const int32 byte_count = size - i;
    const int32 bit_count = byte_count * 8;

    const int64 bb = value >> bit_count;
    const uint8 b = static_cast<uint8>(bb);

    const int32 status = writer->Write(&b, 1);

    if (status < 0)
      return status;
  }

  return 0;
}

int32 SerializeFloat(IMkvWriter* writer, float f) {
  if (!writer)
    return -1;

  assert(sizeof(uint32) == sizeof(float));
  // This union is merely used to avoid a reinterpret_cast from float& to
  // uint32& which will result in violation of strict aliasing.
  union U32 {
    uint32 u32;
    float f;
  } value;
  value.f = f;

  for (int32 i = 1; i <= 4; ++i) {
    const int32 byte_count = 4 - i;
    const int32 bit_count = byte_count * 8;

    const uint8 byte = static_cast<uint8>(value.u32 >> bit_count);

    const int32 status = writer->Write(&byte, 1);

    if (status < 0)
      return status;
  }

  return 0;
}

int32 WriteUInt(IMkvWriter* writer, uint64 value) {
  if (!writer)
    return -1;

  int32 size = GetCodedUIntSize(value);

  return WriteUIntSize(writer, value, size);
}

int32 WriteUIntSize(IMkvWriter* writer, uint64 value, int32 size) {
  if (!writer || size < 0 || size > 8)
    return -1;

  if (size > 0) {
    const uint64 bit = 1LL << (size * 7);

    if (value > (bit - 2))
      return -1;

    value |= bit;
  } else {
    size = 1;
    int64 bit;

    for (;;) {
      bit = 1LL << (size * 7);
      const uint64 max = bit - 2;

      if (value <= max)
        break;

      ++size;
    }

    if (size > 8)
      return false;

    value |= bit;
  }

  return SerializeInt(writer, value, size);
}

int32 WriteID(IMkvWriter* writer, uint64 type) {
  if (!writer)
    return -1;

  writer->ElementStartNotify(type, writer->Position());

  const int32 size = GetUIntSize(type);

  return SerializeInt(writer, type, size);
}

bool WriteEbmlMasterElement(IMkvWriter* writer, uint64 type, uint64 size) {
  if (!writer)
    return false;

  if (WriteID(writer, type))
    return false;

  if (WriteUInt(writer, size))
    return false;

  return true;
}

bool WriteEbmlElement(IMkvWriter* writer, uint64 type, uint64 value) {
  if (!writer)
    return false;

  if (WriteID(writer, type))
    return false;

  const uint64 size = GetUIntSize(value);
  if (WriteUInt(writer, size))
    return false;

  if (SerializeInt(writer, value, static_cast<int32>(size)))
    return false;

  return true;
}

bool WriteEbmlElement(IMkvWriter* writer, uint64 type, float value) {
  if (!writer)
    return false;

  if (WriteID(writer, type))
    return false;

  if (WriteUInt(writer, 4))
    return false;

  if (SerializeFloat(writer, value))
    return false;

  return true;
}

bool WriteEbmlElement(IMkvWriter* writer, uint64 type, const char* value) {
  if (!writer || !value)
    return false;

  if (WriteID(writer, type))
    return false;

  const uint64 length = strlen(value);
  if (WriteUInt(writer, length))
    return false;

  if (writer->Write(value, static_cast<const uint32>(length)))
    return false;

  return true;
}

bool WriteEbmlElement(IMkvWriter* writer, uint64 type, const uint8* value,
                      uint64 size) {
  if (!writer || !value || size < 1)
    return false;

  if (WriteID(writer, type))
    return false;

  if (WriteUInt(writer, size))
    return false;

  if (writer->Write(value, static_cast<uint32>(size)))
    return false;

  return true;
}

bool WriteEbmlDateElement(IMkvWriter* writer, uint64 type, int64 value) {
  if (!writer)
    return false;

  if (WriteID(writer, type))
    return false;

  if (WriteUInt(writer, kDateElementSize))
    return false;

  if (SerializeInt(writer, value, kDateElementSize))
    return false;

  return true;
}

uint64 WriteSimpleBlock(IMkvWriter* writer, const uint8* data, uint64 length,
                        uint64 track_number, int64 timecode, uint64 is_key) {
  if (!writer)
    return false;

  if (!data || length < 1)
    return false;

  //  Here we only permit track number values to be no greater than
  //  126, which the largest value we can store having a Matroska
  //  integer representation of only 1 byte.

  if (track_number < 1 || track_number > 126)
    return false;

  //  Technically the timestamp for a block can be less than the
  //  timestamp for the cluster itself (remember that block timestamp
  //  is a signed, 16-bit integer).  However, as a simplification we
  //  only permit non-negative cluster-relative timestamps for blocks.

  if (timecode < 0 || timecode > kMaxBlockTimecode)
    return false;

  if (WriteID(writer, kMkvSimpleBlock))
    return 0;

  const int32 size = static_cast<int32>(length) + 4;
  if (WriteUInt(writer, size))
    return 0;

  if (WriteUInt(writer, static_cast<uint64>(track_number)))
    return 0;

  if (SerializeInt(writer, timecode, 2))
    return 0;

  uint64 flags = 0;
  if (is_key)
    flags |= 0x80;

  if (SerializeInt(writer, flags, 1))
    return 0;

  if (writer->Write(data, static_cast<uint32>(length)))
    return 0;

  const uint64 element_size =
      GetUIntSize(kMkvSimpleBlock) + GetCodedUIntSize(size) + 4 + length;

  return element_size;
}

// We must write the metadata (key)frame as a BlockGroup element,
// because we need to specify a duration for the frame.  The
// BlockGroup element comprises the frame itself and its duration,
// and is laid out as follows:
//
//   BlockGroup tag
//   BlockGroup size
//     Block tag
//     Block size
//     (the frame is the block payload)
//     Duration tag
//     Duration size
//     (duration payload)
//
uint64 WriteMetadataBlock(IMkvWriter* writer, const uint8* data, uint64 length,
                          uint64 track_number, int64 timecode,
                          uint64 duration) {
  // We don't backtrack when writing to the stream, so we must
  // pre-compute the BlockGroup size, by summing the sizes of each
  // sub-element (the block and the duration).

  // We use a single byte for the track number of the block, which
  // means the block header is exactly 4 bytes.

  // TODO(matthewjheaney): use EbmlMasterElementSize and WriteEbmlMasterElement

  const uint64 block_payload_size = 4 + length;
  const int32 block_size = GetCodedUIntSize(block_payload_size);
  const uint64 block_elem_size = 1 + block_size + block_payload_size;

  const int32 duration_payload_size = GetUIntSize(duration);
  const int32 duration_size = GetCodedUIntSize(duration_payload_size);
  const uint64 duration_elem_size = 1 + duration_size + duration_payload_size;

  const uint64 blockg_payload_size = block_elem_size + duration_elem_size;
  const int32 blockg_size = GetCodedUIntSize(blockg_payload_size);
  const uint64 blockg_elem_size = 1 + blockg_size + blockg_payload_size;

  if (WriteID(writer, kMkvBlockGroup))  // 1-byte ID size
    return 0;

  if (WriteUInt(writer, blockg_payload_size))
    return 0;

  //  Write Block element

  if (WriteID(writer, kMkvBlock))  // 1-byte ID size
    return 0;

  if (WriteUInt(writer, block_payload_size))
    return 0;

  // Byte 1 of 4

  if (WriteUInt(writer, track_number))
    return 0;

  // Bytes 2 & 3 of 4

  if (SerializeInt(writer, timecode, 2))
    return 0;

  // Byte 4 of 4

  const uint64 flags = 0;

  if (SerializeInt(writer, flags, 1))
    return 0;

  // Now write the actual frame (of metadata)

  if (writer->Write(data, static_cast<uint32>(length)))
    return 0;

  // Write Duration element

  if (WriteID(writer, kMkvBlockDuration))  // 1-byte ID size
    return 0;

  if (WriteUInt(writer, duration_payload_size))
    return 0;

  if (SerializeInt(writer, duration, duration_payload_size))
    return 0;

  // Note that we don't write a reference time as part of the block
  // group; no reference time(s) indicates that this block is a
  // keyframe.  (Unlike the case for a SimpleBlock element, the header
  // bits of the Block sub-element of a BlockGroup element do not
  // indicate keyframe status.  The keyframe status is inferred from
  // the absence of reference time sub-elements.)

  return blockg_elem_size;
}

// Writes a WebM BlockGroup with BlockAdditional data. The structure is as
// follows:
// Indentation shows sub-levels
// BlockGroup
//  Block
//    Data
//  BlockAdditions
//    BlockMore
//      BlockAddID
//        1 (Denotes Alpha)
//      BlockAdditional
//        Data
uint64 WriteBlockWithAdditional(IMkvWriter* writer, const uint8* data,
                                uint64 length, const uint8* additional,
                                uint64 additional_length, uint64 add_id,
                                uint64 track_number, int64 timecode,
                                uint64 is_key) {
  if (!data || !additional || length < 1 || additional_length < 1)
    return 0;

  const uint64 block_payload_size = 4 + length;
  const uint64 block_elem_size =
      EbmlMasterElementSize(kMkvBlock, block_payload_size) + block_payload_size;
  const uint64 block_additional_elem_size =
      EbmlElementSize(kMkvBlockAdditional, additional, additional_length);
  const uint64 block_addid_elem_size = EbmlElementSize(kMkvBlockAddID, add_id);

  const uint64 block_more_payload_size =
      block_addid_elem_size + block_additional_elem_size;
  const uint64 block_more_elem_size =
      EbmlMasterElementSize(kMkvBlockMore, block_more_payload_size) +
      block_more_payload_size;
  const uint64 block_additions_payload_size = block_more_elem_size;
  const uint64 block_additions_elem_size =
      EbmlMasterElementSize(kMkvBlockAdditions, block_additions_payload_size) +
      block_additions_payload_size;
  const uint64 block_group_payload_size =
      block_elem_size + block_additions_elem_size;
  const uint64 block_group_elem_size =
      EbmlMasterElementSize(kMkvBlockGroup, block_group_payload_size) +
      block_group_payload_size;

  if (!WriteEbmlMasterElement(writer, kMkvBlockGroup, block_group_payload_size))
    return 0;

  if (!WriteEbmlMasterElement(writer, kMkvBlock, block_payload_size))
    return 0;

  if (WriteUInt(writer, track_number))
    return 0;

  if (SerializeInt(writer, timecode, 2))
    return 0;

  uint64 flags = 0;
  if (is_key)
    flags |= 0x80;
  if (SerializeInt(writer, flags, 1))
    return 0;

  if (writer->Write(data, static_cast<uint32>(length)))
    return 0;

  if (!WriteEbmlMasterElement(writer, kMkvBlockAdditions,
                              block_additions_payload_size))
    return 0;

  if (!WriteEbmlMasterElement(writer, kMkvBlockMore, block_more_payload_size))
    return 0;

  if (!WriteEbmlElement(writer, kMkvBlockAddID, add_id))
    return 0;

  if (!WriteEbmlElement(writer, kMkvBlockAdditional, additional,
                        additional_length))
    return 0;

  return block_group_elem_size;
}

// Writes a WebM BlockGroup with DiscardPadding. The structure is as follows:
// Indentation shows sub-levels
// BlockGroup
//  Block
//    Data
//  DiscardPadding
uint64 WriteBlockWithDiscardPadding(IMkvWriter* writer, const uint8* data,
                                    uint64 length, int64 discard_padding,
                                    uint64 track_number, int64 timecode,
                                    uint64 is_key) {
  if (!data || length < 1 || discard_padding <= 0)
    return 0;

  const uint64 block_payload_size = 4 + length;
  const uint64 block_elem_size =
      EbmlMasterElementSize(kMkvBlock, block_payload_size) + block_payload_size;
  const uint64 discard_padding_elem_size =
      EbmlElementSize(kMkvDiscardPadding, discard_padding);
  const uint64 block_group_payload_size =
      block_elem_size + discard_padding_elem_size;
  const uint64 block_group_elem_size =
      EbmlMasterElementSize(kMkvBlockGroup, block_group_payload_size) +
      block_group_payload_size;

  if (!WriteEbmlMasterElement(writer, kMkvBlockGroup, block_group_payload_size))
    return 0;

  if (!WriteEbmlMasterElement(writer, kMkvBlock, block_payload_size))
    return 0;

  if (WriteUInt(writer, track_number))
    return 0;

  if (SerializeInt(writer, timecode, 2))
    return 0;

  uint64 flags = 0;
  if (is_key)
    flags |= 0x80;
  if (SerializeInt(writer, flags, 1))
    return 0;

  if (writer->Write(data, static_cast<uint32>(length)))
    return 0;

  if (WriteID(writer, kMkvDiscardPadding))
    return 0;

  const uint64 size = GetUIntSize(discard_padding);
  if (WriteUInt(writer, size))
    return false;

  if (SerializeInt(writer, discard_padding, static_cast<int32>(size)))
    return false;

  return block_group_elem_size;
}

uint64 WriteVoidElement(IMkvWriter* writer, uint64 size) {
  if (!writer)
    return false;

  // Subtract one for the void ID and the coded size.
  uint64 void_entry_size = size - 1 - GetCodedUIntSize(size - 1);
  uint64 void_size =
      EbmlMasterElementSize(kMkvVoid, void_entry_size) + void_entry_size;

  if (void_size != size)
    return 0;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return 0;

  if (WriteID(writer, kMkvVoid))
    return 0;

  if (WriteUInt(writer, void_entry_size))
    return 0;

  const uint8 value = 0;
  for (int32 i = 0; i < static_cast<int32>(void_entry_size); ++i) {
    if (writer->Write(&value, 1))
      return 0;
  }

  const int64 stop_position = writer->Position();
  if (stop_position < 0 ||
      stop_position - payload_position != static_cast<int64>(void_size))
    return 0;

  return void_size;
}

void GetVersion(int32* major, int32* minor, int32* build, int32* revision) {
  *major = 0;
  *minor = 2;
  *build = 1;
  *revision = 0;
}

}  // namespace mkvmuxer

mkvmuxer::uint64 mkvmuxer::MakeUID(unsigned int* seed) {
  uint64 uid = 0;

#ifdef __MINGW32__
  srand(*seed);
#endif

  for (int i = 0; i < 7; ++i) {  // avoid problems with 8-byte values
    uid <<= 8;

// TODO(fgalligan): Move random number generation to platform specific code.
#ifdef _MSC_VER
    (void)seed;
    unsigned int random_value;
    const errno_t e = rand_s(&random_value);
    (void)e;
    const int32 nn = random_value;
#elif __ANDROID__
    int32 temp_num = 1;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd != -1) {
      read(fd, &temp_num, sizeof(int32));
      close(fd);
    }
    const int32 nn = temp_num;
#elif defined __MINGW32__
    const int32 nn = rand();
#else
    const int32 nn = rand_r(seed);
#endif
    const int32 n = 0xFF & (nn >> 4);  // throw away low-order bits

    uid |= n;
  }

  return uid;
}
