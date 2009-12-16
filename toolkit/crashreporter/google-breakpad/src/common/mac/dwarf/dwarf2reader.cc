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

#include <assert.h>

#include <stack>
#include <utility>

#include "common/mac/dwarf/bytereader-inl.h"
#include "common/mac/dwarf/dwarf2reader.h"
#include "common/mac/dwarf/bytereader.h"
#include "common/mac/dwarf/line_state_machine.h"

namespace dwarf2reader {

// Read a DWARF2/3 initial length field from START, using READER, and
// report the length in LEN.  Return the actual initial length.

static uint64 ReadInitialLength(const char* start,
                                ByteReader* reader, size_t* len) {
  const uint64 initial_length = reader->ReadFourBytes(start);
  start += 4;

  // In DWARF2/3, if the initial length is all 1 bits, then the offset
  // size is 8 and we need to read the next 8 bytes for the real length.
  if (initial_length == 0xffffffff) {
    reader->SetOffsetSize(8);
    *len = 12;
    return reader->ReadOffset(start);
  } else {
    reader->SetOffsetSize(4);
    *len = 4;
  }
  return initial_length;
}

CompilationUnit::CompilationUnit(const SectionMap& sections, uint64 offset,
                                 ByteReader* reader, Dwarf2Handler* handler)
    : offset_from_section_start_(offset), reader_(reader),
      sections_(sections), handler_(handler), abbrevs_(NULL),
      string_buffer_(NULL), string_buffer_length_(0) {}

// Read a DWARF2/3 abbreviation section.
// Each abbrev consists of a abbreviation number, a tag, a byte
// specifying whether the tag has children, and a list of
// attribute/form pairs.
// The list of forms is terminated by a 0 for the attribute, and a
// zero for the form.  The entire abbreviation section is terminated
// by a zero for the code.

void CompilationUnit::ReadAbbrevs() {
  if (abbrevs_)
    return;

  // First get the debug_abbrev section
  SectionMap::const_iterator iter = sections_.find("__debug_abbrev");
  assert(iter != sections_.end());

  abbrevs_ = new vector<Abbrev>;
  abbrevs_->resize(1);

  // The only way to check whether we are reading over the end of the
  // buffer would be to first compute the size of the leb128 data by
  // reading it, then go back and read it again.
  const char* abbrev_start = iter->second.first +
                                      header_.abbrev_offset;
  const char* abbrevptr = abbrev_start;
  const uint64 abbrev_length = iter->second.second - header_.abbrev_offset;

  while (1) {
    CompilationUnit::Abbrev abbrev;
    size_t len;
    const uint32 number = reader_->ReadUnsignedLEB128(abbrevptr, &len);

    if (number == 0)
      break;
    abbrev.number = number;
    abbrevptr += len;

    assert(abbrevptr < abbrev_start + abbrev_length);
    const uint32 tag = reader_->ReadUnsignedLEB128(abbrevptr, &len);
    abbrevptr += len;
    abbrev.tag = static_cast<enum DwarfTag>(tag);

    assert(abbrevptr < abbrev_start + abbrev_length);
    abbrev.has_children = reader_->ReadOneByte(abbrevptr);
    abbrevptr += 1;

    assert(abbrevptr < abbrev_start + abbrev_length);

    while (1) {
      const uint32 nametemp = reader_->ReadUnsignedLEB128(abbrevptr, &len);
      abbrevptr += len;

      assert(abbrevptr < abbrev_start + abbrev_length);
      const uint32 formtemp = reader_->ReadUnsignedLEB128(abbrevptr, &len);
      abbrevptr += len;
      if (nametemp == 0 && formtemp == 0)
        break;

      const enum DwarfAttribute name =
        static_cast<enum DwarfAttribute>(nametemp);
      const enum DwarfForm form = static_cast<enum DwarfForm>(formtemp);
      abbrev.attributes.push_back(make_pair(name, form));
    }
    assert(abbrev.number == abbrevs_->size());
    abbrevs_->push_back(abbrev);
  }
}

// Skips a single DIE's attributes.
const char* CompilationUnit::SkipDIE(const char* start,
                                              const Abbrev& abbrev) {
  for (AttributeList::const_iterator i = abbrev.attributes.begin();
       i != abbrev.attributes.end();
       i++)  {
    start = SkipAttribute(start, i->second);
  }
  return start;
}

// Skips a single attribute form's data.
const char* CompilationUnit::SkipAttribute(const char* start,
                                                    enum DwarfForm form) {
  size_t len;

  switch (form) {
    case DW_FORM_indirect:
      form = static_cast<enum DwarfForm>(reader_->ReadUnsignedLEB128(start,
                                                                     &len));
      start += len;
      return SkipAttribute(start, form);
      break;

    case DW_FORM_data1:
    case DW_FORM_flag:
    case DW_FORM_ref1:
      return start + 1;
      break;
    case DW_FORM_ref2:
    case DW_FORM_data2:
      return start + 2;
      break;
    case DW_FORM_ref4:
    case DW_FORM_data4:
      return start + 4;
      break;
    case DW_FORM_ref8:
    case DW_FORM_data8:
      return start + 8;
      break;
    case DW_FORM_string:
      return start + strlen(start) + 1;
      break;
    case DW_FORM_udata:
    case DW_FORM_ref_udata:
      reader_->ReadUnsignedLEB128(start, &len);
      return start + len;
      break;

    case DW_FORM_sdata:
      reader_->ReadSignedLEB128(start, &len);
      return start + len;
      break;
    case DW_FORM_addr:
      return start + reader_->AddressSize();
      break;
    case DW_FORM_ref_addr:
      // DWARF2 and 3 differ on whether ref_addr is address size or
      // offset size.
      assert(header_.version == 2 || header_.version == 3);
      if (header_.version == 2) {
        return start + reader_->AddressSize();
      } else if (header_.version == 3) {
        return start + reader_->OffsetSize();
      }
      break;

    case DW_FORM_block1:
      return start + 1 + reader_->ReadOneByte(start);
      break;
    case DW_FORM_block2:
      return start + 2 + reader_->ReadTwoBytes(start);
      break;
    case DW_FORM_block4:
      return start + 4 + reader_->ReadFourBytes(start);
      break;
    case DW_FORM_block: {
      uint64 size = reader_->ReadUnsignedLEB128(start, &len);
      return start + size + len;
    }
      break;
    case DW_FORM_strp:
        return start + reader_->OffsetSize();
      break;
    default:
      fprintf(stderr,"Unhandled form type");
  }
  fprintf(stderr,"Unhandled form type");
  return NULL;
}

// Read a DWARF2/3 header.
// The header is variable length in DWARF3 (and DWARF2 as extended by
// most compilers), and consists of an length field, a version number,
// the offset in the .debug_abbrev section for our abbrevs, and an
// address size.
void CompilationUnit::ReadHeader() {
  const char* headerptr = buffer_;
  size_t initial_length_size;

  assert(headerptr + 4 < buffer_ + buffer_length_);
  const uint64 initial_length = ReadInitialLength(headerptr, reader_,
                                                  &initial_length_size);
  headerptr += initial_length_size;
  header_.length = initial_length;

  assert(headerptr + 2 < buffer_ + buffer_length_);
  header_.version = reader_->ReadTwoBytes(headerptr);
  headerptr += 2;

  assert(headerptr + reader_->OffsetSize() < buffer_ + buffer_length_);
  header_.abbrev_offset = reader_->ReadOffset(headerptr);
  headerptr += reader_->OffsetSize();

  assert(headerptr + 1 < buffer_ + buffer_length_);
  header_.address_size = reader_->ReadOneByte(headerptr);
  reader_->SetAddressSize(header_.address_size);
  headerptr += 1;

  after_header_ = headerptr;

  // This check ensures that we don't have to do checking during the
  // reading of DIEs. header_.length does not include the size of the
  // initial length.
  assert(buffer_ + initial_length_size + header_.length <=
        buffer_ + buffer_length_);
}

uint64 CompilationUnit::Start() {
  // First get the debug_info section
  SectionMap::const_iterator iter = sections_.find("__debug_info");
  assert(iter != sections_.end());

  // Set up our buffer
  buffer_ = iter->second.first + offset_from_section_start_;
  buffer_length_ = iter->second.second - offset_from_section_start_;

  // Read the header
  ReadHeader();

  // Figure out the real length from the end of the initial length to
  // the end of the compilation unit, since that is the value we
  // return.
  uint64 ourlength = header_.length;
  if (reader_->OffsetSize() == 8)
    ourlength += 12;
  else
    ourlength += 4;

  // See if the user wants this compilation unit, and if not, just return.
  if (!handler_->StartCompilationUnit(offset_from_section_start_,
                                      reader_->AddressSize(),
                                      reader_->OffsetSize(),
                                      header_.length,
                                      header_.version))
    return ourlength;

  // Otherwise, continue by reading our abbreviation entries.
  ReadAbbrevs();

  // Set the string section if we have one.
  iter = sections_.find("__debug_str");
  if (iter != sections_.end()) {
    string_buffer_ = iter->second.first;
    string_buffer_length_ = iter->second.second;
  }

  // Now that we have our abbreviations, start processing DIE's.
  ProcessDIEs();

  return ourlength;
}

// If one really wanted, you could merge SkipAttribute and
// ProcessAttribute
// This is all boring data manipulation and calling of the handler.
const char* CompilationUnit::ProcessAttribute(
    uint64 dieoffset, const char* start, enum DwarfAttribute attr,
    enum DwarfForm form) {
  size_t len;

  switch (form) {
    // DW_FORM_indirect is never used because it is such a space
    // waster.
    case DW_FORM_indirect:
      form = static_cast<enum DwarfForm>(reader_->ReadUnsignedLEB128(start,
                                                                     &len));
      start += len;
      return ProcessAttribute(dieoffset, start, attr, form);
      break;

    case DW_FORM_data1:
    case DW_FORM_flag:
    case DW_FORM_ref1:
      handler_->ProcessAttributeUnsigned(dieoffset, attr, form,
                                         reader_->ReadOneByte(start));
      return start + 1;
      break;
    case DW_FORM_ref2:
    case DW_FORM_data2:
      handler_->ProcessAttributeUnsigned(dieoffset, attr, form,
                                         reader_->ReadTwoBytes(start));
      return start + 2;
      break;
    case DW_FORM_ref4:
    case DW_FORM_data4:
      handler_->ProcessAttributeUnsigned(dieoffset, attr, form,
                                         reader_->ReadFourBytes(start));
      return start + 4;
      break;
    case DW_FORM_ref8:
    case DW_FORM_data8:
      handler_->ProcessAttributeUnsigned(dieoffset, attr, form,
                                         reader_->ReadEightBytes(start));
      return start + 8;
      break;
    case DW_FORM_string: {
      const char* str = start;
      handler_->ProcessAttributeString(dieoffset, attr, form,
                                       str);
      return start + strlen(str) + 1;
    }
      break;
    case DW_FORM_udata:
    case DW_FORM_ref_udata:
      handler_->ProcessAttributeUnsigned(dieoffset, attr, form,
                                         reader_->ReadUnsignedLEB128(start,
                                                                     &len));
      return start + len;
      break;

    case DW_FORM_sdata:
      handler_->ProcessAttributeSigned(dieoffset, attr, form,
                                      reader_->ReadSignedLEB128(start, &len));
      return start + len;
      break;
    case DW_FORM_addr:
      handler_->ProcessAttributeUnsigned(dieoffset, attr, form,
                                         reader_->ReadAddress(start));
      return start + reader_->AddressSize();
      break;
    case DW_FORM_ref_addr:
      // DWARF2 and 3 differ on whether ref_addr is address size or
      // offset size.
      assert(header_.version == 2 || header_.version == 3);
      if (header_.version == 2) {
        handler_->ProcessAttributeUnsigned(dieoffset, attr, form,
                                           reader_->ReadAddress(start));
        return start + reader_->AddressSize();
      } else if (header_.version == 3) {
        handler_->ProcessAttributeUnsigned(dieoffset, attr, form,
                                           reader_->ReadOffset(start));
        return start + reader_->OffsetSize();
      }
      break;

    case DW_FORM_block1: {
      uint64 datalen = reader_->ReadOneByte(start);
      handler_->ProcessAttributeBuffer(dieoffset, attr, form, start + 1,
                                      datalen);
      return start + 1 + datalen;
    }
      break;
    case DW_FORM_block2: {
      uint64 datalen = reader_->ReadTwoBytes(start);
      handler_->ProcessAttributeBuffer(dieoffset, attr, form, start + 2,
                                      datalen);
      return start + 2 + datalen;
    }
      break;
    case DW_FORM_block4: {
      uint64 datalen = reader_->ReadFourBytes(start);
      handler_->ProcessAttributeBuffer(dieoffset, attr, form, start + 4,
                                      datalen);
      return start + 4 + datalen;
    }
      break;
    case DW_FORM_block: {
      uint64 datalen = reader_->ReadUnsignedLEB128(start, &len);
      handler_->ProcessAttributeBuffer(dieoffset, attr, form, start + len,
                                      datalen);
      return start + datalen + len;
    }
      break;
    case DW_FORM_strp: {
      assert(string_buffer_ != NULL);

      const uint64 offset = reader_->ReadOffset(start);
      assert(string_buffer_ + offset < string_buffer_ + string_buffer_length_);

      const char* str = string_buffer_ + offset;
      handler_->ProcessAttributeString(dieoffset, attr, form,
                                       str);
      return start + reader_->OffsetSize();
    }
      break;
    default:
      fprintf(stderr, "Unhandled form type");
  }
  fprintf(stderr, "Unhandled form type");
  return NULL;
}

const char* CompilationUnit::ProcessDIE(uint64 dieoffset,
                                                 const char* start,
                                                 const Abbrev& abbrev) {
  for (AttributeList::const_iterator i = abbrev.attributes.begin();
       i != abbrev.attributes.end();
       i++)  {
    start = ProcessAttribute(dieoffset, start, i->first, i->second);
  }
  return start;
}

void CompilationUnit::ProcessDIEs() {
  const char* dieptr = after_header_;
  size_t len;

  // lengthstart is the place the length field is based on.
  // It is the point in the header after the initial length field
  const char* lengthstart = buffer_;

  // In 64 bit dwarf, the initial length is 12 bytes, because of the
  // 0xffffffff at the start.
  if (reader_->OffsetSize() == 8)
    lengthstart += 12;
  else
    lengthstart += 4;

  // we need semantics of boost scoped_ptr here - no intention of trasnferring
  // ownership of the stack.  use const, but then we limit ourselves to not
  // ever being able to call .reset() on the smart pointer.
  auto_ptr<stack<uint64> > const die_stack(new stack<uint64>);

  while (dieptr < (lengthstart + header_.length)) {
    // We give the user the absolute offset from the beginning of
    // debug_info, since they need it to deal with ref_addr forms.
    uint64 absolute_offset = (dieptr - buffer_) + offset_from_section_start_;

    uint64 abbrev_num = reader_->ReadUnsignedLEB128(dieptr, &len);

    dieptr += len;

    // Abbrev == 0 represents the end of a list of children.
    if (abbrev_num == 0) {
      const uint64 offset = die_stack->top();
      die_stack->pop();
      handler_->EndDIE(offset);
      continue;
    }

    const Abbrev& abbrev = abbrevs_->at(abbrev_num);
    const enum DwarfTag tag = abbrev.tag;
    if (!handler_->StartDIE(absolute_offset, tag, abbrev.attributes)) {
      dieptr = SkipDIE(dieptr, abbrev);
    } else {
      dieptr = ProcessDIE(absolute_offset, dieptr, abbrev);
    }

    if (abbrev.has_children) {
      die_stack->push(absolute_offset);
    } else {
      handler_->EndDIE(absolute_offset);
    }
  }
}

LineInfo::LineInfo(const char* buffer, uint64 buffer_length,
                   ByteReader* reader, LineInfoHandler* handler):
    handler_(handler), reader_(reader), buffer_(buffer),
    buffer_length_(buffer_length) {
  header_.std_opcode_lengths = NULL;
}

uint64 LineInfo::Start() {
  ReadHeader();
  ReadLines();
  return after_header_ - buffer_;
}

// The header for a debug_line section is mildly complicated, because
// the line info is very tightly encoded.
void LineInfo::ReadHeader() {
  const char* lineptr = buffer_;
  size_t initial_length_size;

  const uint64 initial_length = ReadInitialLength(lineptr, reader_,
                                                  &initial_length_size);

  lineptr += initial_length_size;
  header_.total_length = initial_length;
  assert(buffer_ + initial_length_size + header_.total_length <=
        buffer_ + buffer_length_);

  // Address size *must* be set by CU ahead of time.
  assert(reader_->AddressSize() != 0);

  header_.version = reader_->ReadTwoBytes(lineptr);
  lineptr += 2;

  header_.prologue_length = reader_->ReadOffset(lineptr);
  lineptr += reader_->OffsetSize();

  header_.min_insn_length = reader_->ReadOneByte(lineptr);
  lineptr += 1;

  header_.default_is_stmt = reader_->ReadOneByte(lineptr);
  lineptr += 1;

  header_.line_base = *reinterpret_cast<const int8*>(lineptr);
  lineptr += 1;

  header_.line_range = reader_->ReadOneByte(lineptr);
  lineptr += 1;

  header_.opcode_base = reader_->ReadOneByte(lineptr);
  lineptr += 1;

  header_.std_opcode_lengths = new vector<unsigned char>;
  header_.std_opcode_lengths->resize(header_.opcode_base + 1);
  (*header_.std_opcode_lengths)[0] = 0;
  for (int i = 1; i < header_.opcode_base; i++) {
    (*header_.std_opcode_lengths)[i] = reader_->ReadOneByte(lineptr);
    lineptr += 1;
  }

  // It is legal for the directory entry table to be empty.
  if (*lineptr) {
    uint32 dirindex = 1;
    while (*lineptr) {
      const char* dirname = lineptr;
      handler_->DefineDir(dirname, dirindex);
      lineptr += strlen(dirname) + 1;
      dirindex++;
    }
  }
  lineptr++;

  // It is also legal for the file entry table to be empty.
  if (*lineptr) {
    uint32 fileindex = 1;
    size_t len;
    while (*lineptr) {
      const char* filename = lineptr;
      lineptr += strlen(filename) + 1;

      uint64 dirindex = reader_->ReadUnsignedLEB128(lineptr, &len);
      lineptr += len;

      uint64 mod_time = reader_->ReadUnsignedLEB128(lineptr, &len);
      lineptr += len;

      uint64 filelength = reader_->ReadUnsignedLEB128(lineptr, &len);
      lineptr += len;
      handler_->DefineFile(filename, fileindex, dirindex, mod_time,
                           filelength);
      fileindex++;
    }
  }
  lineptr++;

  after_header_ = lineptr;
}

/* static */
bool LineInfo::ProcessOneOpcode(ByteReader* reader,
                                LineInfoHandler* handler,
                                const struct LineInfoHeader &header,
                                const char* start,
                                struct LineStateMachine* lsm,
                                size_t* len,
                                uintptr_t pc,
                                bool *lsm_passes_pc) {
  size_t oplen = 0;
  size_t templen;
  uint8 opcode = reader->ReadOneByte(start);
  oplen++;
  start++;

  // If the opcode is great than the opcode_base, it is a special
  // opcode. Most line programs consist mainly of special opcodes.
  if (opcode >= header.opcode_base) {
    opcode -= header.opcode_base;
    const int64 advance_address = (opcode / header.line_range)
                                  * header.min_insn_length;
    const int64 advance_line = (opcode % header.line_range)
                               + header.line_base;

    // Check if the lsm passes "pc". If so, mark it as passed.
    if (lsm_passes_pc &&
        lsm->address <= pc && pc < lsm->address + advance_address) {
      *lsm_passes_pc = true;
    }

    lsm->address += advance_address;
    lsm->line_num += advance_line;
    lsm->basic_block = true;
    *len = oplen;
    return true;
  }

  // Otherwise, we have the regular opcodes
  switch (opcode) {
    case DW_LNS_copy: {
      lsm->basic_block = false;
      *len = oplen;
      return true;
    }

    case DW_LNS_advance_pc: {
      uint64 advance_address = reader->ReadUnsignedLEB128(start, &templen);
      oplen += templen;

      // Check if the lsm passes "pc". If so, mark it as passed.
      if (lsm_passes_pc && lsm->address <= pc &&
          pc < lsm->address + header.min_insn_length * advance_address) {
        *lsm_passes_pc = true;
      }

      lsm->address += header.min_insn_length * advance_address;
    }
      break;
    case DW_LNS_advance_line: {
      const int64 advance_line = reader->ReadSignedLEB128(start, &templen);
      oplen += templen;
      lsm->line_num += advance_line;

      // With gcc 4.2.1, we can get the line_no here for the first time
      // since DW_LNS_advance_line is called after DW_LNE_set_address is
      // called. So we check if the lsm passes "pc" here, not in
      // DW_LNE_set_address.
      if (lsm_passes_pc && lsm->address == pc) {
        *lsm_passes_pc = true;
      }
    }
      break;
    case DW_LNS_set_file: {
      const uint64 fileno = reader->ReadUnsignedLEB128(start, &templen);
      oplen += templen;
      lsm->file_num = fileno;
    }
      break;
    case DW_LNS_set_column: {
      const uint64 colno = reader->ReadUnsignedLEB128(start, &templen);
      oplen += templen;
      lsm->column_num = colno;
    }
      break;
    case DW_LNS_negate_stmt: {
      lsm->is_stmt = !lsm->is_stmt;
    }
      break;
    case DW_LNS_set_basic_block: {
      lsm->basic_block = true;
    }
      break;
    case DW_LNS_fixed_advance_pc: {
      const uint16 advance_address = reader->ReadTwoBytes(start);
      oplen += 2;

      // Check if the lsm passes "pc". If so, mark it as passed.
      if (lsm_passes_pc &&
          lsm->address <= pc && pc < lsm->address + advance_address) {
        *lsm_passes_pc = true;
      }

      lsm->address += advance_address;
    }
      break;
    case DW_LNS_const_add_pc: {
      const int64 advance_address = header.min_insn_length
                                    * ((255 - header.opcode_base)
                                       / header.line_range);

      // Check if the lsm passes "pc". If so, mark it as passed.
      if (lsm_passes_pc &&
          lsm->address <= pc && pc < lsm->address + advance_address) {
        *lsm_passes_pc = true;
      }

      lsm->address += advance_address;
    }
      break;
    case DW_LNS_extended_op: {
      const size_t extended_op_len = reader->ReadUnsignedLEB128(start,
                                                                &templen);
      start += templen;
      oplen += templen + extended_op_len;

      const uint64 extended_op = reader->ReadOneByte(start);
      start++;

      switch (extended_op) {
        case DW_LNE_end_sequence: {
          lsm->end_sequence = true;
          *len = oplen;
          return true;
        }
          break;
        case DW_LNE_set_address: {
          // With gcc 4.2.1, we cannot tell the line_no here since
          // DW_LNE_set_address is called before DW_LNS_advance_line is
          // called.  So we do not check if the lsm passes "pc" here.  See
          // also the comment in DW_LNS_advance_line.
          uint64 address = reader->ReadAddress(start);
          lsm->address = address;
        }
          break;
        case DW_LNE_define_file: {
          const char* filename  = start;

          templen = strlen(filename) + 1;
          start += templen;

          uint64 dirindex = reader->ReadUnsignedLEB128(start, &templen);
          oplen += templen;

          const uint64 mod_time = reader->ReadUnsignedLEB128(start,
                                                             &templen);
          oplen += templen;

          const uint64 filelength = reader->ReadUnsignedLEB128(start,
                                                               &templen);
          oplen += templen;

          if (handler) {
            handler->DefineFile(filename, -1, dirindex, mod_time,
                                filelength);
          }
        }
          break;
      }
    }
      break;

    default: {
      // Ignore unknown opcode  silently
      if (header.std_opcode_lengths) {
        for (int i = 0; i < (*header.std_opcode_lengths)[opcode]; i++) {
          size_t templen;
          reader->ReadUnsignedLEB128(start, &templen);
          start += templen;
          oplen += templen;
        }
      }
    }
      break;
  }
  *len = oplen;
  return false;
}

void LineInfo::ReadLines() {
  struct LineStateMachine lsm;

  // lengthstart is the place the length field is based on.
  // It is the point in the header after the initial length field
  const char* lengthstart = buffer_;

  // In 64 bit dwarf, the initial length is 12 bytes, because of the
  // 0xffffffff at the start.
  if (reader_->OffsetSize() == 8)
    lengthstart += 12;
  else
    lengthstart += 4;

  const char* lineptr = after_header_;
  while (lineptr < lengthstart + header_.total_length) {
    lsm.Reset(header_.default_is_stmt);
    while (!lsm.end_sequence) {
      size_t oplength;
      bool add_line = ProcessOneOpcode(reader_, handler_, header_,
                                       lineptr, &lsm, &oplength, (uintptr_t)-1, NULL);
      if (add_line)
        handler_->AddLine(lsm.address, lsm.file_num, lsm.line_num,
                          lsm.column_num);
      lineptr += oplength;
    }
  }

  after_header_ = lengthstart + header_.total_length;
}

}  // namespace dwarf2reader
