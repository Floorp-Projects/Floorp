// Copyright 2006 Google Inc. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef COMMON_MAC_DWARF_BYTEREADER_H__
#define COMMON_MAC_DWARF_BYTEREADER_H__

#include <string>
#include "common/mac/dwarf/types.h"

namespace dwarf2reader {

// We can't use the obvious name of LITTLE_ENDIAN and BIG_ENDIAN
// because it conflicts with a macro
enum Endianness {
  ENDIANNESS_BIG,
  ENDIANNESS_LITTLE
};

// Class that knows how to read both big endian and little endian
// numbers, for use in DWARF2/3 reader.
// Takes an endianness argument.
// To read addresses and offsets, SetAddressSize and SetOffsetSize
// must be called first.
class ByteReader {
 public:
  explicit ByteReader(enum Endianness endian);
  virtual ~ByteReader();

  // Set the address size to SIZE, which sets up the ReadAddress member
  // so that it works.
  void SetAddressSize(uint8 size);

  // Set the offset size to SIZE, which sets up the ReadOffset member
  // so that it works.
  void SetOffsetSize(uint8 size);

  // Return the current offset size
  uint8 OffsetSize() const { return offset_size_; }

  // Return the current address size
  uint8 AddressSize() const { return address_size_; }

  // Read a single byte from BUFFER and return it as an unsigned 8 bit
  // number.
  uint8 ReadOneByte(const char* buffer) const;

  // Read two bytes from BUFFER and return it as an unsigned 16 bit
  // number.
  uint16 ReadTwoBytes(const char* buffer) const;

  // Read four bytes from BUFFER and return it as an unsigned 32 bit
  // number.  This function returns a uint64 so that it is compatible
  // with ReadAddress and ReadOffset.  The number it returns will
  // never be outside the range of an unsigned 32 bit integer.
  uint64 ReadFourBytes(const char* buffer) const;

  // Read eight bytes from BUFFER and return it as an unsigned 64 bit
  // number
  uint64 ReadEightBytes(const char* buffer) const;

  // Read an unsigned LEB128 (Little Endian Base 128) number from
  // BUFFER and return it as an unsigned 64 bit integer.  LEN is set
  // to the length read.  Everybody seems to reinvent LEB128 as a
  // variable size integer encoding, DWARF has had it for a long time.
  uint64 ReadUnsignedLEB128(const char* buffer, size_t* len) const;

  // Read a signed LEB128 number from BUFFER and return it as an
  // signed 64 bit integer.  LEN is set to the length read.
  int64 ReadSignedLEB128(const char* buffer, size_t* len) const;

  // Read an offset from BUFFER and return it as an unsigned 64 bit
  // integer.  DWARF2/3 define offsets as either 4 or 8 bytes,
  // generally depending on the amount of DWARF2/3 info present.
  uint64 ReadOffset(const char* buffer) const;

  // Read an address from BUFFER and return it as an unsigned 64 bit
  // integer.  DWARF2/3 allow addresses to be any size from 0-255
  // bytes currently.  Internally we support 4 and 8 byte addresses,
  // and will CHECK on anything else.
  uint64 ReadAddress(const char* buffer) const;

 private:

  // Function pointer type for our address and offset readers.
  typedef uint64 (ByteReader::*AddressReader)(const char*) const;

  // Read an offset from BUFFER and return it as an unsigned 64 bit
  // integer.  DWARF2/3 define offsets as either 4 or 8 bytes,
  // generally depending on the amount of DWARF2/3 info present.
  // This function pointer gets set by SetOffsetSize.
  AddressReader offset_reader_;

  // Read an address from BUFFER and return it as an unsigned 64 bit
  // integer.  DWARF2/3 allow addresses to be any size from 0-255
  // bytes currently.  Internally we support 4 and 8 byte addresses,
  // and will CHECK on anything else.
  // This function pointer gets set by SetAddressSize.
  AddressReader address_reader_;

  Endianness endian_;
  uint8 address_size_;
  uint8 offset_size_;
};

}  // namespace dwarf2reader

#endif  // COMMON_MAC_DWARF_BYTEREADER_H__
