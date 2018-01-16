/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

// Copyright 2006, 2010 Google Inc. All Rights Reserved.
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

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// This file is derived from the following files in
// toolkit/crashreporter/google-breakpad:
//   src/common/dwarf/types.h
//   src/common/dwarf/dwarf2enums.h
//   src/common/dwarf/bytereader.h
//   src/common/dwarf_cfi_to_module.h
//   src/common/dwarf/dwarf2reader.h

#ifndef LulDwarfExt_h
#define LulDwarfExt_h

#include <stdint.h>

#include "mozilla/Assertions.h"

#include "LulDwarfSummariser.h"

typedef signed char         int8;
typedef short               int16;
typedef int                 int32;
typedef long long           int64;

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;

#ifdef __PTRDIFF_TYPE__
typedef          __PTRDIFF_TYPE__ intptr;
typedef unsigned __PTRDIFF_TYPE__ uintptr;
#else
#error "Can't find pointer-sized integral types."
#endif


namespace lul {

// Exception handling frame description pointer formats, as described
// by the Linux Standard Base Core Specification 4.0, section 11.5,
// DWARF Extensions.
enum DwarfPointerEncoding
  {
    DW_EH_PE_absptr	= 0x00,
    DW_EH_PE_omit	= 0xff,
    DW_EH_PE_uleb128    = 0x01,
    DW_EH_PE_udata2	= 0x02,
    DW_EH_PE_udata4	= 0x03,
    DW_EH_PE_udata8	= 0x04,
    DW_EH_PE_sleb128    = 0x09,
    DW_EH_PE_sdata2	= 0x0A,
    DW_EH_PE_sdata4	= 0x0B,
    DW_EH_PE_sdata8	= 0x0C,
    DW_EH_PE_pcrel	= 0x10,
    DW_EH_PE_textrel	= 0x20,
    DW_EH_PE_datarel	= 0x30,
    DW_EH_PE_funcrel	= 0x40,
    DW_EH_PE_aligned	= 0x50,

    // The GNU toolchain sources define this enum value as well,
    // simply to help classify the lower nybble values into signed and
    // unsigned groups.
    DW_EH_PE_signed	= 0x08,

    // This is not documented in LSB 4.0, but it is used in both the
    // Linux and OS X toolchains. It can be added to any other
    // encoding (except DW_EH_PE_aligned), and indicates that the
    // encoded value represents the address at which the true address
    // is stored, not the true address itself.
    DW_EH_PE_indirect	= 0x80
  };


// We can't use the obvious name of LITTLE_ENDIAN and BIG_ENDIAN
// because it conflicts with a macro
enum Endianness {
  ENDIANNESS_BIG,
  ENDIANNESS_LITTLE
};

// A ByteReader knows how to read single- and multi-byte values of
// various endiannesses, sizes, and encodings, as used in DWARF
// debugging information and Linux C++ exception handling data.
class ByteReader {
 public:
  // Construct a ByteReader capable of reading one-, two-, four-, and
  // eight-byte values according to ENDIANNESS, absolute machine-sized
  // addresses, DWARF-style "initial length" values, signed and
  // unsigned LEB128 numbers, and Linux C++ exception handling data's
  // encoded pointers.
  explicit ByteReader(enum Endianness endianness);
  virtual ~ByteReader();

  // Read a single byte from BUFFER and return it as an unsigned 8 bit
  // number.
  uint8 ReadOneByte(const char* buffer) const;

  // Read two bytes from BUFFER and return them as an unsigned 16 bit
  // number, using this ByteReader's endianness.
  uint16 ReadTwoBytes(const char* buffer) const;

  // Read four bytes from BUFFER and return them as an unsigned 32 bit
  // number, using this ByteReader's endianness. This function returns
  // a uint64 so that it is compatible with ReadAddress and
  // ReadOffset. The number it returns will never be outside the range
  // of an unsigned 32 bit integer.
  uint64 ReadFourBytes(const char* buffer) const;

  // Read eight bytes from BUFFER and return them as an unsigned 64
  // bit number, using this ByteReader's endianness.
  uint64 ReadEightBytes(const char* buffer) const;

  // Read an unsigned LEB128 (Little Endian Base 128) number from
  // BUFFER and return it as an unsigned 64 bit integer. Set LEN to
  // the number of bytes read.
  //
  // The unsigned LEB128 representation of an integer N is a variable
  // number of bytes:
  //
  // - If N is between 0 and 0x7f, then its unsigned LEB128
  //   representation is a single byte whose value is N.
  //
  // - Otherwise, its unsigned LEB128 representation is (N & 0x7f) |
  //   0x80, followed by the unsigned LEB128 representation of N /
  //   128, rounded towards negative infinity.
  //
  // In other words, we break VALUE into groups of seven bits, put
  // them in little-endian order, and then write them as eight-bit
  // bytes with the high bit on all but the last.
  uint64 ReadUnsignedLEB128(const char* buffer, size_t* len) const;

  // Read a signed LEB128 number from BUFFER and return it as an
  // signed 64 bit integer. Set LEN to the number of bytes read.
  //
  // The signed LEB128 representation of an integer N is a variable
  // number of bytes:
  //
  // - If N is between -0x40 and 0x3f, then its signed LEB128
  //   representation is a single byte whose value is N in two's
  //   complement.
  //
  // - Otherwise, its signed LEB128 representation is (N & 0x7f) |
  //   0x80, followed by the signed LEB128 representation of N / 128,
  //   rounded towards negative infinity.
  //
  // In other words, we break VALUE into groups of seven bits, put
  // them in little-endian order, and then write them as eight-bit
  // bytes with the high bit on all but the last.
  int64 ReadSignedLEB128(const char* buffer, size_t* len) const;

  // Indicate that addresses on this architecture are SIZE bytes long. SIZE
  // must be either 4 or 8. (DWARF allows addresses to be any number of
  // bytes in length from 1 to 255, but we only support 32- and 64-bit
  // addresses at the moment.) You must call this before using the
  // ReadAddress member function.
  //
  // For data in a .debug_info section, or something that .debug_info
  // refers to like line number or macro data, the compilation unit
  // header's address_size field indicates the address size to use. Call
  // frame information doesn't indicate its address size (a shortcoming of
  // the spec); you must supply the appropriate size based on the
  // architecture of the target machine.
  void SetAddressSize(uint8 size);

  // Return the current address size, in bytes. This is either 4,
  // indicating 32-bit addresses, or 8, indicating 64-bit addresses.
  uint8 AddressSize() const { return address_size_; }

  // Read an address from BUFFER and return it as an unsigned 64 bit
  // integer, respecting this ByteReader's endianness and address size. You
  // must call SetAddressSize before calling this function.
  uint64 ReadAddress(const char* buffer) const;

  // DWARF actually defines two slightly different formats: 32-bit DWARF
  // and 64-bit DWARF. This is *not* related to the size of registers or
  // addresses on the target machine; it refers only to the size of section
  // offsets and data lengths appearing in the DWARF data. One only needs
  // 64-bit DWARF when the debugging data itself is larger than 4GiB.
  // 32-bit DWARF can handle x86_64 or PPC64 code just fine, unless the
  // debugging data itself is very large.
  //
  // DWARF information identifies itself as 32-bit or 64-bit DWARF: each
  // compilation unit and call frame information entry begins with an
  // "initial length" field, which, in addition to giving the length of the
  // data, also indicates the size of section offsets and lengths appearing
  // in that data. The ReadInitialLength member function, below, reads an
  // initial length and sets the ByteReader's offset size as a side effect.
  // Thus, in the normal process of reading DWARF data, the appropriate
  // offset size is set automatically. So, you should only need to call
  // SetOffsetSize if you are using the same ByteReader to jump from the
  // midst of one block of DWARF data into another.

  // Read a DWARF "initial length" field from START, and return it as
  // an unsigned 64 bit integer, respecting this ByteReader's
  // endianness. Set *LEN to the length of the initial length in
  // bytes, either four or twelve. As a side effect, set this
  // ByteReader's offset size to either 4 (if we see a 32-bit DWARF
  // initial length) or 8 (if we see a 64-bit DWARF initial length).
  //
  // A DWARF initial length is either:
  //
  // - a byte count stored as an unsigned 32-bit value less than
  //   0xffffff00, indicating that the data whose length is being
  //   measured uses the 32-bit DWARF format, or
  //
  // - The 32-bit value 0xffffffff, followed by a 64-bit byte count,
  //   indicating that the data whose length is being measured uses
  //   the 64-bit DWARF format.
  uint64 ReadInitialLength(const char* start, size_t* len);

  // Read an offset from BUFFER and return it as an unsigned 64 bit
  // integer, respecting the ByteReader's endianness. In 32-bit DWARF, the
  // offset is 4 bytes long; in 64-bit DWARF, the offset is eight bytes
  // long. You must call ReadInitialLength or SetOffsetSize before calling
  // this function; see the comments above for details.
  uint64 ReadOffset(const char* buffer) const;

  // Return the current offset size, in bytes.
  // A return value of 4 indicates that we are reading 32-bit DWARF.
  // A return value of 8 indicates that we are reading 64-bit DWARF.
  uint8 OffsetSize() const { return offset_size_; }

  // Indicate that section offsets and lengths are SIZE bytes long. SIZE
  // must be either 4 (meaning 32-bit DWARF) or 8 (meaning 64-bit DWARF).
  // Usually, you should not call this function yourself; instead, let a
  // call to ReadInitialLength establish the data's offset size
  // automatically.
  void SetOffsetSize(uint8 size);

  // The Linux C++ ABI uses a variant of DWARF call frame information
  // for exception handling. This data is included in the program's
  // address space as the ".eh_frame" section, and intepreted at
  // runtime to walk the stack, find exception handlers, and run
  // cleanup code. The format is mostly the same as DWARF CFI, with
  // some adjustments made to provide the additional
  // exception-handling data, and to make the data easier to work with
  // in memory --- for example, to allow it to be placed in read-only
  // memory even when describing position-independent code.
  //
  // In particular, exception handling data can select a number of
  // different encodings for pointers that appear in the data, as
  // described by the DwarfPointerEncoding enum. There are actually
  // four axes(!) to the encoding:
  //
  // - The pointer size: pointers can be 2, 4, or 8 bytes long, or use
  //   the DWARF LEB128 encoding.
  //
  // - The pointer's signedness: pointers can be signed or unsigned.
  //
  // - The pointer's base address: the data stored in the exception
  //   handling data can be the actual address (that is, an absolute
  //   pointer), or relative to one of a number of different base
  //   addreses --- including that of the encoded pointer itself, for
  //   a form of "pc-relative" addressing.
  //
  // - The pointer may be indirect: it may be the address where the
  //   true pointer is stored. (This is used to refer to things via
  //   global offset table entries, program linkage table entries, or
  //   other tricks used in position-independent code.)
  //
  // There are also two options that fall outside that matrix
  // altogether: the pointer may be omitted, or it may have padding to
  // align it on an appropriate address boundary. (That last option
  // may seem like it should be just another axis, but it is not.)

  // Indicate that the exception handling data is loaded starting at
  // SECTION_BASE, and that the start of its buffer in our own memory
  // is BUFFER_BASE. This allows us to find the address that a given
  // byte in our buffer would have when loaded into the program the
  // data describes. We need this to resolve DW_EH_PE_pcrel pointers.
  void SetCFIDataBase(uint64 section_base, const char *buffer_base);

  // Indicate that the base address of the program's ".text" section
  // is TEXT_BASE. We need this to resolve DW_EH_PE_textrel pointers.
  void SetTextBase(uint64 text_base);

  // Indicate that the base address for DW_EH_PE_datarel pointers is
  // DATA_BASE. The proper value depends on the ABI; it is usually the
  // address of the global offset table, held in a designated register in
  // position-independent code. You will need to look at the startup code
  // for the target system to be sure. I tried; my eyes bled.
  void SetDataBase(uint64 data_base);

  // Indicate that the base address for the FDE we are processing is
  // FUNCTION_BASE. This is the start address of DW_EH_PE_funcrel
  // pointers. (This encoding does not seem to be used by the GNU
  // toolchain.)
  void SetFunctionBase(uint64 function_base);

  // Indicate that we are no longer processing any FDE, so any use of
  // a DW_EH_PE_funcrel encoding is an error.
  void ClearFunctionBase();

  // Return true if ENCODING is a valid pointer encoding.
  bool ValidEncoding(DwarfPointerEncoding encoding) const;

  // Return true if we have all the information we need to read a
  // pointer that uses ENCODING. This checks that the appropriate
  // SetFooBase function for ENCODING has been called.
  bool UsableEncoding(DwarfPointerEncoding encoding) const;

  // Read an encoded pointer from BUFFER using ENCODING; return the
  // absolute address it represents, and set *LEN to the pointer's
  // length in bytes, including any padding for aligned pointers.
  //
  // This function calls 'abort' if ENCODING is invalid or refers to a
  // base address this reader hasn't been given, so you should check
  // with ValidEncoding and UsableEncoding first if you would rather
  // die in a more helpful way.
  uint64 ReadEncodedPointer(const char *buffer, DwarfPointerEncoding encoding,
                            size_t *len) const;

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

  // Base addresses for Linux C++ exception handling data's encoded pointers.
  bool have_section_base_, have_text_base_, have_data_base_;
  bool have_function_base_;
  uint64 section_base_;
  uint64 text_base_, data_base_, function_base_;
  const char *buffer_base_;
};


inline uint8 ByteReader::ReadOneByte(const char* buffer) const {
  return buffer[0];
}

inline uint16 ByteReader::ReadTwoBytes(const char* signed_buffer) const {
  const unsigned char *buffer
    = reinterpret_cast<const unsigned char *>(signed_buffer);
  const uint16 buffer0 = buffer[0];
  const uint16 buffer1 = buffer[1];
  if (endian_ == ENDIANNESS_LITTLE) {
    return buffer0 | buffer1 << 8;
  } else {
    return buffer1 | buffer0 << 8;
  }
}

inline uint64 ByteReader::ReadFourBytes(const char* signed_buffer) const {
  const unsigned char *buffer
    = reinterpret_cast<const unsigned char *>(signed_buffer);
  const uint32 buffer0 = buffer[0];
  const uint32 buffer1 = buffer[1];
  const uint32 buffer2 = buffer[2];
  const uint32 buffer3 = buffer[3];
  if (endian_ == ENDIANNESS_LITTLE) {
    return buffer0 | buffer1 << 8 | buffer2 << 16 | buffer3 << 24;
  } else {
    return buffer3 | buffer2 << 8 | buffer1 << 16 | buffer0 << 24;
  }
}

inline uint64 ByteReader::ReadEightBytes(const char* signed_buffer) const {
  const unsigned char *buffer
    = reinterpret_cast<const unsigned char *>(signed_buffer);
  const uint64 buffer0 = buffer[0];
  const uint64 buffer1 = buffer[1];
  const uint64 buffer2 = buffer[2];
  const uint64 buffer3 = buffer[3];
  const uint64 buffer4 = buffer[4];
  const uint64 buffer5 = buffer[5];
  const uint64 buffer6 = buffer[6];
  const uint64 buffer7 = buffer[7];
  if (endian_ == ENDIANNESS_LITTLE) {
    return buffer0 | buffer1 << 8 | buffer2 << 16 | buffer3 << 24 |
      buffer4 << 32 | buffer5 << 40 | buffer6 << 48 | buffer7 << 56;
  } else {
    return buffer7 | buffer6 << 8 | buffer5 << 16 | buffer4 << 24 |
      buffer3 << 32 | buffer2 << 40 | buffer1 << 48 | buffer0 << 56;
  }
}

// Read an unsigned LEB128 number.  Each byte contains 7 bits of
// information, plus one bit saying whether the number continues or
// not.

inline uint64 ByteReader::ReadUnsignedLEB128(const char* buffer,
                                             size_t* len) const {
  uint64 result = 0;
  size_t num_read = 0;
  unsigned int shift = 0;
  unsigned char byte;

  do {
    byte = *buffer++;
    num_read++;

    result |= (static_cast<uint64>(byte & 0x7f)) << shift;

    shift += 7;

  } while (byte & 0x80);

  *len = num_read;

  return result;
}

// Read a signed LEB128 number.  These are like regular LEB128
// numbers, except the last byte may have a sign bit set.

inline int64 ByteReader::ReadSignedLEB128(const char* buffer,
                                          size_t* len) const {
  int64 result = 0;
  unsigned int shift = 0;
  size_t num_read = 0;
  unsigned char byte;

  do {
      byte = *buffer++;
      num_read++;
      result |= (static_cast<uint64>(byte & 0x7f) << shift);
      shift += 7;
  } while (byte & 0x80);

  if ((shift < 8 * sizeof (result)) && (byte & 0x40))
    result |= -((static_cast<int64>(1)) << shift);
  *len = num_read;
  return result;
}

inline uint64 ByteReader::ReadOffset(const char* buffer) const {
  MOZ_ASSERT(this->offset_reader_);
  return (this->*offset_reader_)(buffer);
}

inline uint64 ByteReader::ReadAddress(const char* buffer) const {
  MOZ_ASSERT(this->address_reader_);
  return (this->*address_reader_)(buffer);
}

inline void ByteReader::SetCFIDataBase(uint64 section_base,
                                       const char *buffer_base) {
  section_base_ = section_base;
  buffer_base_ = buffer_base;
  have_section_base_ = true;
}

inline void ByteReader::SetTextBase(uint64 text_base) {
  text_base_ = text_base;
  have_text_base_ = true;
}

inline void ByteReader::SetDataBase(uint64 data_base) {
  data_base_ = data_base;
  have_data_base_ = true;
}

inline void ByteReader::SetFunctionBase(uint64 function_base) {
  function_base_ = function_base;
  have_function_base_ = true;
}

inline void ByteReader::ClearFunctionBase() {
  have_function_base_ = false;
}


// (derived from)
// dwarf_cfi_to_module.h: Define the DwarfCFIToModule class, which
// accepts parsed DWARF call frame info and adds it to a Summariser object.

// This class is a reader for DWARF's Call Frame Information.  CFI
// describes how to unwind stack frames --- even for functions that do
// not follow fixed conventions for saving registers, whose frame size
// varies as they execute, etc.
//
// CFI describes, at each machine instruction, how to compute the
// stack frame's base address, how to find the return address, and
// where to find the saved values of the caller's registers (if the
// callee has stashed them somewhere to free up the registers for its
// own use).
//
// For example, suppose we have a function whose machine code looks
// like this (imagine an assembly language that looks like C, for a
// machine with 32-bit registers, and a stack that grows towards lower
// addresses):
//
// func:                                ; entry point; return address at sp
// func+0:      sp = sp - 16            ; allocate space for stack frame
// func+1:      sp[12] = r0             ; save r0 at sp+12
// ...                                  ; other code, not frame-related
// func+10:     sp -= 4; *sp = x        ; push some x on the stack
// ...                                  ; other code, not frame-related
// func+20:     r0 = sp[16]             ; restore saved r0
// func+21:     sp += 20                ; pop whole stack frame
// func+22:     pc = *sp; sp += 4       ; pop return address and jump to it
//
// DWARF CFI is (a very compressed representation of) a table with a
// row for each machine instruction address and a column for each
// register showing how to restore it, if possible.
//
// A special column named "CFA", for "Canonical Frame Address", tells how
// to compute the base address of the frame; registers' entries may
// refer to the CFA in describing where the registers are saved.
//
// Another special column, named "RA", represents the return address.
//
// For example, here is a complete (uncompressed) table describing the
// function above:
//
//     insn      cfa    r0      r1 ...  ra
//     =======================================
//     func+0:   sp                     cfa[0]
//     func+1:   sp+16                  cfa[0]
//     func+2:   sp+16  cfa[-4]         cfa[0]
//     func+11:  sp+20  cfa[-4]         cfa[0]
//     func+21:  sp+20                  cfa[0]
//     func+22:  sp                     cfa[0]
//
// Some things to note here:
//
// - Each row describes the state of affairs *before* executing the
//   instruction at the given address.  Thus, the row for func+0
//   describes the state before we allocate the stack frame.  In the
//   next row, the formula for computing the CFA has changed,
//   reflecting that allocation.
//
// - The other entries are written in terms of the CFA; this allows
//   them to remain unchanged as the stack pointer gets bumped around.
//   For example, the rule for recovering the return address (the "ra"
//   column) remains unchanged throughout the function, even as the
//   stack pointer takes on three different offsets from the return
//   address.
//
// - Although we haven't shown it, most calling conventions designate
//   "callee-saves" and "caller-saves" registers. The callee must
//   preserve the values of callee-saves registers; if it uses them,
//   it must save their original values somewhere, and restore them
//   before it returns. In contrast, the callee is free to trash
//   caller-saves registers; if the callee uses these, it will
//   probably not bother to save them anywhere, and the CFI will
//   probably mark their values as "unrecoverable".
//
//   (However, since the caller cannot assume the callee was going to
//   save them, caller-saves registers are probably dead in the caller
//   anyway, so compilers usually don't generate CFA for caller-saves
//   registers.)
//
// - Exactly where the CFA points is a matter of convention that
//   depends on the architecture and ABI in use. In the example, the
//   CFA is the value the stack pointer had upon entry to the
//   function, pointing at the saved return address. But on the x86,
//   the call frame information generated by GCC follows the
//   convention that the CFA is the address *after* the saved return
//   address.
//
//   But by definition, the CFA remains constant throughout the
//   lifetime of the frame. This makes it a useful value for other
//   columns to refer to. It is also gives debuggers a useful handle
//   for identifying a frame.
//
// If you look at the table above, you'll notice that a given entry is
// often the same as the one immediately above it: most instructions
// change only one or two aspects of the stack frame, if they affect
// it at all. The DWARF format takes advantage of this fact, and
// reduces the size of the data by mentioning only the addresses and
// columns at which changes take place. So for the above, DWARF CFI
// data would only actually mention the following:
//
//     insn      cfa    r0      r1 ...  ra
//     =======================================
//     func+0:   sp                     cfa[0]
//     func+1:   sp+16
//     func+2:          cfa[-4]
//     func+11:  sp+20
//     func+21:         r0
//     func+22:  sp
//
// In fact, this is the way the parser reports CFI to the consumer: as
// a series of statements of the form, "At address X, column Y changed
// to Z," and related conventions for describing the initial state.
//
// Naturally, it would be impractical to have to scan the entire
// program's CFI, noting changes as we go, just to recover the
// unwinding rules in effect at one particular instruction. To avoid
// this, CFI data is grouped into "entries", each of which covers a
// specified range of addresses and begins with a complete statement
// of the rules for all recoverable registers at that starting
// address. Each entry typically covers a single function.
//
// Thus, to compute the contents of a given row of the table --- that
// is, rules for recovering the CFA, RA, and registers at a given
// instruction --- the consumer should find the entry that covers that
// instruction's address, start with the initial state supplied at the
// beginning of the entry, and work forward until it has processed all
// the changes up to and including those for the present instruction.
//
// There are seven kinds of rules that can appear in an entry of the
// table:
//
// - "undefined": The given register is not preserved by the callee;
//   its value cannot be recovered.
//
// - "same value": This register has the same value it did in the callee.
//
// - offset(N): The register is saved at offset N from the CFA.
//
// - val_offset(N): The value the register had in the caller is the
//   CFA plus offset N. (This is usually only useful for describing
//   the stack pointer.)
//
// - register(R): The register's value was saved in another register R.
//
// - expression(E): Evaluating the DWARF expression E using the
//   current frame's registers' values yields the address at which the
//   register was saved.
//
// - val_expression(E): Evaluating the DWARF expression E using the
//   current frame's registers' values yields the value the register
//   had in the caller.

class CallFrameInfo {
 public:
  // The different kinds of entries one finds in CFI. Used internally,
  // and for error reporting.
  enum EntryKind { kUnknown, kCIE, kFDE, kTerminator };

  // The handler class to which the parser hands the parsed call frame
  // information.  Defined below.
  class Handler;

  // A reporter class, which CallFrameInfo uses to report errors
  // encountered while parsing call frame information.  Defined below.
  class Reporter;

  // Create a DWARF CFI parser. BUFFER points to the contents of the
  // .debug_frame section to parse; BUFFER_LENGTH is its length in bytes.
  // REPORTER is an error reporter the parser should use to report
  // problems. READER is a ByteReader instance that has the endianness and
  // address size set properly. Report the data we find to HANDLER.
  //
  // This class can also parse Linux C++ exception handling data, as found
  // in '.eh_frame' sections. This data is a variant of DWARF CFI that is
  // placed in loadable segments so that it is present in the program's
  // address space, and is interpreted by the C++ runtime to search the
  // call stack for a handler interested in the exception being thrown,
  // actually pop the frames, and find cleanup code to run.
  //
  // There are two differences between the call frame information described
  // in the DWARF standard and the exception handling data Linux places in
  // the .eh_frame section:
  //
  // - Exception handling data uses uses a different format for call frame
  //   information entry headers. The distinguished CIE id, the way FDEs
  //   refer to their CIEs, and the way the end of the series of entries is
  //   determined are all slightly different.
  //
  //   If the constructor's EH_FRAME argument is true, then the
  //   CallFrameInfo parses the entry headers as Linux C++ exception
  //   handling data. If EH_FRAME is false or omitted, the CallFrameInfo
  //   parses standard DWARF call frame information.
  //
  // - Linux C++ exception handling data uses CIE augmentation strings
  //   beginning with 'z' to specify the presence of additional data after
  //   the CIE and FDE headers and special encodings used for addresses in
  //   frame description entries.
  //
  //   CallFrameInfo can handle 'z' augmentations in either DWARF CFI or
  //   exception handling data if you have supplied READER with the base
  //   addresses needed to interpret the pointer encodings that 'z'
  //   augmentations can specify. See the ByteReader interface for details
  //   about the base addresses. See the CallFrameInfo::Handler interface
  //   for details about the additional information one might find in
  //   'z'-augmented data.
  //
  // Thus:
  //
  // - If you are parsing standard DWARF CFI, as found in a .debug_frame
  //   section, you should pass false for the EH_FRAME argument, or omit
  //   it, and you need not worry about providing READER with the
  //   additional base addresses.
  //
  // - If you want to parse Linux C++ exception handling data from a
  //   .eh_frame section, you should pass EH_FRAME as true, and call
  //   READER's Set*Base member functions before calling our Start method.
  //
  // - If you want to parse DWARF CFI that uses the 'z' augmentations
  //   (although I don't think any toolchain ever emits such data), you
  //   could pass false for EH_FRAME, but call READER's Set*Base members.
  //
  // The extensions the Linux C++ ABI makes to DWARF for exception
  // handling are described here, rather poorly:
  // http://refspecs.linux-foundation.org/LSB_4.0.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html
  // http://refspecs.linux-foundation.org/LSB_4.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
  //
  // The mechanics of C++ exception handling, personality routines,
  // and language-specific data areas are described here, rather nicely:
  // http://www.codesourcery.com/public/cxx-abi/abi-eh.html

  CallFrameInfo(const char *buffer, size_t buffer_length,
                ByteReader *reader, Handler *handler, Reporter *reporter,
                bool eh_frame = false)
      : buffer_(buffer), buffer_length_(buffer_length),
        reader_(reader), handler_(handler), reporter_(reporter),
        eh_frame_(eh_frame) { }

  ~CallFrameInfo() { }

  // Parse the entries in BUFFER, reporting what we find to HANDLER.
  // Return true if we reach the end of the section successfully, or
  // false if we encounter an error.
  bool Start();

  // Return the textual name of KIND. For error reporting.
  static const char *KindName(EntryKind kind);

 private:

  struct CIE;

  // A CFI entry, either an FDE or a CIE.
  struct Entry {
    // The starting offset of the entry in the section, for error
    // reporting.
    size_t offset;

    // The start of this entry in the buffer.
    const char *start;

    // Which kind of entry this is.
    //
    // We want to be able to use this for error reporting even while we're
    // in the midst of parsing. Error reporting code may assume that kind,
    // offset, and start fields are valid, although kind may be kUnknown.
    EntryKind kind;

    // The end of this entry's common prologue (initial length and id), and
    // the start of this entry's kind-specific fields.
    const char *fields;

    // The start of this entry's instructions.
    const char *instructions;

    // The address past the entry's last byte in the buffer. (Note that
    // since offset points to the entry's initial length field, and the
    // length field is the number of bytes after that field, this is not
    // simply buffer_ + offset + length.)
    const char *end;

    // For both DWARF CFI and .eh_frame sections, this is the CIE id in a
    // CIE, and the offset of the associated CIE in an FDE.
    uint64 id;

    // The CIE that applies to this entry, if we've parsed it. If this is a
    // CIE, then this field points to this structure.
    CIE *cie;
  };

  // A common information entry (CIE).
  struct CIE: public Entry {
    uint8 version;                      // CFI data version number
    std::string augmentation;           // vendor format extension markers
    uint64 code_alignment_factor;       // scale for code address adjustments
    int data_alignment_factor;          // scale for stack pointer adjustments
    unsigned return_address_register;   // which register holds the return addr

    // True if this CIE includes Linux C++ ABI 'z' augmentation data.
    bool has_z_augmentation;

    // Parsed 'z' augmentation data. These are meaningful only if
    // has_z_augmentation is true.
    bool has_z_lsda;                    // The 'z' augmentation included 'L'.
    bool has_z_personality;             // The 'z' augmentation included 'P'.
    bool has_z_signal_frame;            // The 'z' augmentation included 'S'.

    // If has_z_lsda is true, this is the encoding to be used for language-
    // specific data area pointers in FDEs.
    DwarfPointerEncoding lsda_encoding;

    // If has_z_personality is true, this is the encoding used for the
    // personality routine pointer in the augmentation data.
    DwarfPointerEncoding personality_encoding;

    // If has_z_personality is true, this is the address of the personality
    // routine --- or, if personality_encoding & DW_EH_PE_indirect, the
    // address where the personality routine's address is stored.
    uint64 personality_address;

    // This is the encoding used for addresses in the FDE header and
    // in DW_CFA_set_loc instructions. This is always valid, whether
    // or not we saw a 'z' augmentation string; its default value is
    // DW_EH_PE_absptr, which is what normal DWARF CFI uses.
    DwarfPointerEncoding pointer_encoding;
  };

  // A frame description entry (FDE).
  struct FDE: public Entry {
    uint64 address;                     // start address of described code
    uint64 size;                        // size of described code, in bytes

    // If cie->has_z_lsda is true, then this is the language-specific data
    // area's address --- or its address's address, if cie->lsda_encoding
    // has the DW_EH_PE_indirect bit set.
    uint64 lsda_address;
  };

  // Internal use.
  class Rule;
  class UndefinedRule;
  class SameValueRule;
  class OffsetRule;
  class ValOffsetRule;
  class RegisterRule;
  class ExpressionRule;
  class ValExpressionRule;
  class RuleMap;
  class State;

  // Parse the initial length and id of a CFI entry, either a CIE, an FDE,
  // or a .eh_frame end-of-data mark. CURSOR points to the beginning of the
  // data to parse. On success, populate ENTRY as appropriate, and return
  // true. On failure, report the problem, and return false. Even if we
  // return false, set ENTRY->end to the first byte after the entry if we
  // were able to figure that out, or NULL if we weren't.
  bool ReadEntryPrologue(const char *cursor, Entry *entry);

  // Parse the fields of a CIE after the entry prologue, including any 'z'
  // augmentation data. Assume that the 'Entry' fields of CIE are
  // populated; use CIE->fields and CIE->end as the start and limit for
  // parsing. On success, populate the rest of *CIE, and return true; on
  // failure, report the problem and return false.
  bool ReadCIEFields(CIE *cie);

  // Parse the fields of an FDE after the entry prologue, including any 'z'
  // augmentation data. Assume that the 'Entry' fields of *FDE are
  // initialized; use FDE->fields and FDE->end as the start and limit for
  // parsing. Assume that FDE->cie is fully initialized. On success,
  // populate the rest of *FDE, and return true; on failure, report the
  // problem and return false.
  bool ReadFDEFields(FDE *fde);

  // Report that ENTRY is incomplete, and return false. This is just a
  // trivial wrapper for invoking reporter_->Incomplete; it provides a
  // little brevity.
  bool ReportIncomplete(Entry *entry);

  // Return true if ENCODING has the DW_EH_PE_indirect bit set.
  static bool IsIndirectEncoding(DwarfPointerEncoding encoding) {
    return encoding & DW_EH_PE_indirect;
  }

  // The contents of the DWARF .debug_info section we're parsing.
  const char *buffer_;
  size_t buffer_length_;

  // For reading multi-byte values with the appropriate endianness.
  ByteReader *reader_;

  // The handler to which we should report the data we find.
  Handler *handler_;

  // For reporting problems in the info we're parsing.
  Reporter *reporter_;

  // True if we are processing .eh_frame-format data.
  bool eh_frame_;
};


// The handler class for CallFrameInfo.  The a CFI parser calls the
// member functions of a handler object to report the data it finds.
class CallFrameInfo::Handler {
 public:
  // The pseudo-register number for the canonical frame address.
  enum { kCFARegister = DW_REG_CFA };

  Handler() { }
  virtual ~Handler() { }

  // The parser has found CFI for the machine code at ADDRESS,
  // extending for LENGTH bytes. OFFSET is the offset of the frame
  // description entry in the section, for use in error messages.
  // VERSION is the version number of the CFI format. AUGMENTATION is
  // a string describing any producer-specific extensions present in
  // the data. RETURN_ADDRESS is the number of the register that holds
  // the address to which the function should return.
  //
  // Entry should return true to process this CFI, or false to skip to
  // the next entry.
  //
  // The parser invokes Entry for each Frame Description Entry (FDE)
  // it finds.  The parser doesn't report Common Information Entries
  // to the handler explicitly; instead, if the handler elects to
  // process a given FDE, the parser reiterates the appropriate CIE's
  // contents at the beginning of the FDE's rules.
  virtual bool Entry(size_t offset, uint64 address, uint64 length,
                     uint8 version, const std::string &augmentation,
                     unsigned return_address) = 0;

  // When the Entry function returns true, the parser calls these
  // handler functions repeatedly to describe the rules for recovering
  // registers at each instruction in the given range of machine code.
  // Immediately after a call to Entry, the handler should assume that
  // the rule for each callee-saves register is "unchanged" --- that
  // is, that the register still has the value it had in the caller.
  //
  // If a *Rule function returns true, we continue processing this entry's
  // instructions. If a *Rule function returns false, we stop evaluating
  // instructions, and skip to the next entry. Either way, we call End
  // before going on to the next entry.
  //
  // In all of these functions, if the REG parameter is kCFARegister, then
  // the rule describes how to find the canonical frame address.
  // kCFARegister may be passed as a BASE_REGISTER argument, meaning that
  // the canonical frame address should be used as the base address for the
  // computation. All other REG values will be positive.

  // At ADDRESS, register REG's value is not recoverable.
  virtual bool UndefinedRule(uint64 address, int reg) = 0;

  // At ADDRESS, register REG's value is the same as that it had in
  // the caller.
  virtual bool SameValueRule(uint64 address, int reg) = 0;

  // At ADDRESS, register REG has been saved at offset OFFSET from
  // BASE_REGISTER.
  virtual bool OffsetRule(uint64 address, int reg,
                          int base_register, long offset) = 0;

  // At ADDRESS, the caller's value of register REG is the current
  // value of BASE_REGISTER plus OFFSET. (This rule doesn't provide an
  // address at which the register's value is saved.)
  virtual bool ValOffsetRule(uint64 address, int reg,
                             int base_register, long offset) = 0;

  // At ADDRESS, register REG has been saved in BASE_REGISTER. This differs
  // from ValOffsetRule(ADDRESS, REG, BASE_REGISTER, 0), in that
  // BASE_REGISTER is the "home" for REG's saved value: if you want to
  // assign to a variable whose home is REG in the calling frame, you
  // should put the value in BASE_REGISTER.
  virtual bool RegisterRule(uint64 address, int reg, int base_register) = 0;

  // At ADDRESS, the DWARF expression EXPRESSION yields the address at
  // which REG was saved.
  virtual bool ExpressionRule(uint64 address, int reg,
                              const std::string &expression) = 0;

  // At ADDRESS, the DWARF expression EXPRESSION yields the caller's
  // value for REG. (This rule doesn't provide an address at which the
  // register's value is saved.)
  virtual bool ValExpressionRule(uint64 address, int reg,
                                 const std::string &expression) = 0;

  // Indicate that the rules for the address range reported by the
  // last call to Entry are complete.  End should return true if
  // everything is okay, or false if an error has occurred and parsing
  // should stop.
  virtual bool End() = 0;

  // Handler functions for Linux C++ exception handling data. These are
  // only called if the data includes 'z' augmentation strings.

  // The Linux C++ ABI uses an extension of the DWARF CFI format to
  // walk the stack to propagate exceptions from the throw to the
  // appropriate catch, and do the appropriate cleanups along the way.
  // CFI entries used for exception handling have two additional data
  // associated with them:
  //
  // - The "language-specific data area" describes which exception
  //   types the function has 'catch' clauses for, and indicates how
  //   to go about re-entering the function at the appropriate catch
  //   clause. If the exception is not caught, it describes the
  //   destructors that must run before the frame is popped.
  //
  // - The "personality routine" is responsible for interpreting the
  //   language-specific data area's contents, and deciding whether
  //   the exception should continue to propagate down the stack,
  //   perhaps after doing some cleanup for this frame, or whether the
  //   exception will be caught here.
  //
  // In principle, the language-specific data area is opaque to
  // everybody but the personality routine. In practice, these values
  // may be useful or interesting to readers with extra context, and
  // we have to at least skip them anyway, so we might as well report
  // them to the handler.

  // This entry's exception handling personality routine's address is
  // ADDRESS. If INDIRECT is true, then ADDRESS is the address at
  // which the routine's address is stored. The default definition for
  // this handler function simply returns true, allowing parsing of
  // the entry to continue.
  virtual bool PersonalityRoutine(uint64 address, bool indirect) {
    return true;
  }

  // This entry's language-specific data area (LSDA) is located at
  // ADDRESS. If INDIRECT is true, then ADDRESS is the address at
  // which the area's address is stored. The default definition for
  // this handler function simply returns true, allowing parsing of
  // the entry to continue.
  virtual bool LanguageSpecificDataArea(uint64 address, bool indirect) {
    return true;
  }

  // This entry describes a signal trampoline --- this frame is the
  // caller of a signal handler. The default definition for this
  // handler function simply returns true, allowing parsing of the
  // entry to continue.
  //
  // The best description of the rationale for and meaning of signal
  // trampoline CFI entries seems to be in the GCC bug database:
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=26208
  virtual bool SignalHandler() { return true; }
};


// The CallFrameInfo class makes calls on an instance of this class to
// report errors or warn about problems in the data it is parsing.
// These messages are sent to the message sink |aLog| provided to the
// constructor.
class CallFrameInfo::Reporter {
 public:
  // Create an error reporter which attributes troubles to the section
  // named SECTION in FILENAME.
  //
  // Normally SECTION would be .debug_frame, but the Mac puts CFI data
  // in a Mach-O section named __debug_frame. If we support
  // Linux-style exception handling data, we could be reading an
  // .eh_frame section.
  Reporter(void (*aLog)(const char*),
           const std::string &filename,
           const std::string &section = ".debug_frame")
      : log_(aLog), filename_(filename), section_(section) { }
  virtual ~Reporter() { }

  // The CFI entry at OFFSET ends too early to be well-formed. KIND
  // indicates what kind of entry it is; KIND can be kUnknown if we
  // haven't parsed enough of the entry to tell yet.
  virtual void Incomplete(uint64 offset, CallFrameInfo::EntryKind kind);

  // The .eh_frame data has a four-byte zero at OFFSET where the next
  // entry's length would be; this is a terminator. However, the buffer
  // length as given to the CallFrameInfo constructor says there should be
  // more data.
  virtual void EarlyEHTerminator(uint64 offset);

  // The FDE at OFFSET refers to the CIE at CIE_OFFSET, but the
  // section is not that large.
  virtual void CIEPointerOutOfRange(uint64 offset, uint64 cie_offset);

  // The FDE at OFFSET refers to the CIE at CIE_OFFSET, but the entry
  // there is not a CIE.
  virtual void BadCIEId(uint64 offset, uint64 cie_offset);

  // The FDE at OFFSET refers to a CIE with version number VERSION,
  // which we don't recognize. We cannot parse DWARF CFI if it uses
  // a version number we don't recognize.
  virtual void UnrecognizedVersion(uint64 offset, int version);

  // The FDE at OFFSET refers to a CIE with augmentation AUGMENTATION,
  // which we don't recognize. We cannot parse DWARF CFI if it uses
  // augmentations we don't recognize.
  virtual void UnrecognizedAugmentation(uint64 offset,
                                        const std::string &augmentation);

  // The FDE at OFFSET contains an invalid or otherwise unusable Dwarf4
  // specific field (currently, only "address_size" or "segment_size").
  // Parsing DWARF CFI with unexpected values here seems dubious at best,
  // so we stop.  WHAT gives a little more information about what is wrong.
  virtual void InvalidDwarf4Artefact(uint64 offset, const char* what);

  // The pointer encoding ENCODING, specified by the CIE at OFFSET, is not
  // a valid encoding.
  virtual void InvalidPointerEncoding(uint64 offset, uint8 encoding);

  // The pointer encoding ENCODING, specified by the CIE at OFFSET, depends
  // on a base address which has not been supplied.
  virtual void UnusablePointerEncoding(uint64 offset, uint8 encoding);

  // The CIE at OFFSET contains a DW_CFA_restore instruction at
  // INSN_OFFSET, which may not appear in a CIE.
  virtual void RestoreInCIE(uint64 offset, uint64 insn_offset);

  // The entry at OFFSET, of kind KIND, has an unrecognized
  // instruction at INSN_OFFSET.
  virtual void BadInstruction(uint64 offset, CallFrameInfo::EntryKind kind,
                              uint64 insn_offset);

  // The instruction at INSN_OFFSET in the entry at OFFSET, of kind
  // KIND, establishes a rule that cites the CFA, but we have not
  // established a CFA rule yet.
  virtual void NoCFARule(uint64 offset, CallFrameInfo::EntryKind kind,
                         uint64 insn_offset);

  // The instruction at INSN_OFFSET in the entry at OFFSET, of kind
  // KIND, is a DW_CFA_restore_state instruction, but the stack of
  // saved states is empty.
  virtual void EmptyStateStack(uint64 offset, CallFrameInfo::EntryKind kind,
                               uint64 insn_offset);

  // The DW_CFA_remember_state instruction at INSN_OFFSET in the entry
  // at OFFSET, of kind KIND, would restore a state that has no CFA
  // rule, whereas the current state does have a CFA rule. This is
  // bogus input, which the CallFrameInfo::Handler interface doesn't
  // (and shouldn't) have any way to report.
  virtual void ClearingCFARule(uint64 offset, CallFrameInfo::EntryKind kind,
                               uint64 insn_offset);

 private:
  // A logging sink function, as supplied by LUL's user.
  void (*log_)(const char*);

 protected:
  // The name of the file whose CFI we're reading.
  std::string filename_;

  // The name of the CFI section in that file.
  std::string section_;
};


using lul::CallFrameInfo;
using lul::Summariser;

// A class that accepts parsed call frame information from the DWARF
// CFI parser and populates a google_breakpad::Module object with the
// contents.
class DwarfCFIToModule: public CallFrameInfo::Handler {
 public:

  // DwarfCFIToModule uses an instance of this class to report errors
  // detected while converting DWARF CFI to Breakpad STACK CFI records.
  class Reporter {
   public:
    // Create a reporter that writes messages to the message sink
    // |aLog|. FILE is the name of the file we're processing, and
    // SECTION is the name of the section within that file that we're
    // looking at (.debug_frame, .eh_frame, etc.).
    Reporter(void (*aLog)(const char*),
             const std::string &file, const std::string &section)
      : log_(aLog), file_(file), section_(section) { }
    virtual ~Reporter() { }

    // The DWARF CFI entry at OFFSET says that REG is undefined, but the
    // Breakpad symbol file format cannot express this.
    virtual void UndefinedNotSupported(size_t offset,
                                       const UniqueString* reg);

    // The DWARF CFI entry at OFFSET says that REG uses a DWARF
    // expression to find its value, but parseDwarfExpr could not
    // convert it to a sequence of PfxInstrs.
    virtual void ExpressionCouldNotBeSummarised(size_t offset,
                                                const UniqueString* reg);

  private:
    // A logging sink function, as supplied by LUL's user.
    void (*log_)(const char*);
  protected:
    std::string file_, section_;
  };

  // Register name tables. If TABLE is a vector returned by one of these
  // functions, then TABLE[R] is the name of the register numbered R in
  // DWARF call frame information.
  class RegisterNames {
   public:
    // Intel's "x86" or IA-32.
    static unsigned int I386();

    // AMD x86_64, AMD64, Intel EM64T, or Intel 64
    static unsigned int X86_64();

    // ARM.
    static unsigned int ARM();

    // MIPS.
    static unsigned int MIPS();
  };

  // Create a handler for the dwarf2reader::CallFrameInfo parser that
  // records the stack unwinding information it receives in SUMM.
  //
  // Use REGISTER_NAMES[I] as the name of register number I; *this
  // keeps a reference to the vector, so the vector should remain
  // alive for as long as the DwarfCFIToModule does.
  //
  // Use REPORTER for reporting problems encountered in the conversion
  // process.
  DwarfCFIToModule(const unsigned int num_dw_regs,
                   Reporter *reporter,
                   ByteReader* reader,
                   /*MOD*/UniqueStringUniverse* usu,
                   /*OUT*/Summariser* summ)
      : summ_(summ), usu_(usu), num_dw_regs_(num_dw_regs),
        reporter_(reporter), reader_(reader), return_address_(-1) {
  }
  virtual ~DwarfCFIToModule() {}

  virtual bool Entry(size_t offset, uint64 address, uint64 length,
                     uint8 version, const std::string &augmentation,
                     unsigned return_address) override;
  virtual bool UndefinedRule(uint64 address, int reg) override;
  virtual bool SameValueRule(uint64 address, int reg) override;
  virtual bool OffsetRule(uint64 address, int reg,
                          int base_register, long offset) override;
  virtual bool ValOffsetRule(uint64 address, int reg,
                             int base_register, long offset) override;
  virtual bool RegisterRule(uint64 address, int reg, int base_register) override;
  virtual bool ExpressionRule(uint64 address, int reg,
                              const std::string &expression) override;
  virtual bool ValExpressionRule(uint64 address, int reg,
                                 const std::string &expression) override;
  virtual bool End() override;

 private:
  // Return the name to use for register I.
  const UniqueString* RegisterName(int i);

  // The Summariser to which we should give entries
  Summariser* summ_;

  // Universe for creating UniqueStrings in, should that be necessary.
  UniqueStringUniverse* usu_;

  // The number of Dwarf-defined register names for this architecture.
  const unsigned int num_dw_regs_;

  // The reporter to use to report problems.
  Reporter *reporter_;

  // The ByteReader to use for parsing Dwarf expressions.
  ByteReader* reader_;

  // The section offset of the current frame description entry, for
  // use in error messages.
  size_t entry_offset_;

  // The return address column for that entry.
  unsigned return_address_;
};


// Convert the Dwarf expression in |expr| into PfxInstrs stored in the
// SecMap referred to by |summ|, and return the index of the starting
// PfxInstr added, which must be >= 0.  In case of failure return -1.
int32_t parseDwarfExpr(Summariser* summ, const ByteReader* reader,
                       string expr, bool debug,
                       bool pushCfaAtStart, bool derefAtEnd);

} // namespace lul

#endif // LulDwarfExt_h
