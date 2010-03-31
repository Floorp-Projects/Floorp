// Copyright (c) 2010 Google Inc. All Rights Reserved.
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

// This file implements the google_breakpad::StabsReader class.
// See stabs_reader.h.

#include <a.out.h>
#include <stab.h>
#include <cstring>
#include <cassert>

#include "common/linux/stabs_reader.h"

namespace google_breakpad {

StabsReader::StabsReader(const uint8_t *stab,    size_t stab_size,
                         const uint8_t *stabstr, size_t stabstr_size,
                         StabsHandler *handler) :
    stabstr_(stabstr),
    stabstr_size_(stabstr_size),
    handler_(handler),
    string_offset_(0),
    next_cu_string_offset_(0),
    symbol_(NULL),
    current_source_file_(NULL) { 
  symbols_ = reinterpret_cast<const struct nlist *>(stab);
  symbols_end_ = symbols_ + (stab_size / sizeof (*symbols_));
}

const char *StabsReader::SymbolString() {
  ptrdiff_t offset = string_offset_ + symbol_->n_un.n_strx;
  if (offset < 0 || (size_t) offset >= stabstr_size_) {
    handler_->Warning("symbol %d: name offset outside the string section",
                      symbol_ - symbols_);
    // Return our null string, to keep our promise about all names being
    // taken from the string section.
    offset = 0;
  }
  return reinterpret_cast<const char *>(stabstr_ + offset);
}

bool StabsReader::Process() {
  symbol_ = symbols_;
  while (symbol_ < symbols_end_) {
    if (symbol_->n_type == N_SO) {
      if (! ProcessCompilationUnit())
        return false;
    } else if (symbol_->n_type == N_UNDF) {
      // At the head of each compilation unit's entries there is an
      // N_UNDF stab giving the number of symbols in the compilation
      // unit, and the number of bytes that compilation unit's strings
      // take up in the .stabstr section. Each CU's strings are
      // separate; the n_strx values are offsets within the current
      // CU's portion of the .stabstr section.
      //
      // As an optimization, the GNU linker combines all the
      // compilation units into one, with a single N_UNDF at the
      // beginning. However, other linkers, like Gold, do not perform
      // this optimization.
      string_offset_ = next_cu_string_offset_;
      next_cu_string_offset_ = SymbolValue();
      symbol_++;
    } else
      symbol_++;
  }
  return true;
}

bool StabsReader::ProcessCompilationUnit() {
  assert(symbol_ < symbols_end_ && symbol_->n_type == N_SO);

  // There may be an N_SO entry whose name ends with a slash,
  // indicating the directory in which the compilation occurred.
  // The build directory defaults to NULL.
  const char *build_directory = NULL;  
  {
    const char *name = SymbolString();
    if (name[0] && name[strlen(name) - 1] == '/') {
      build_directory = name;
      symbol_++;
    }
  }
      
  // We expect to see an N_SO entry with a filename next, indicating
  // the start of the compilation unit.
  {
    if (symbol_ >= symbols_end_ || symbol_->n_type != N_SO)
      return true;
    const char *name = SymbolString();
    if (name[0] == '\0') {
      // This seems to be a stray end-of-compilation-unit marker;
      // consume it, but don't report the end, since we didn't see a
      // beginning.
      symbol_++;
      return true;
    }
    current_source_file_ = name;
  }

  if (! handler_->StartCompilationUnit(current_source_file_,
                                       SymbolValue(),
                                       build_directory))
    return false;

  symbol_++;

  // The STABS documentation says that some compilers may emit
  // additional N_SO entries with names immediately following the
  // first, and that they should be ignored.  However, the original
  // Breakpad STABS reader doesn't ignore them, so we won't either.

  // Process the body of the compilation unit, up to the next N_SO.
  while (symbol_ < symbols_end_ && symbol_->n_type != N_SO) {
    if (symbol_->n_type == N_FUN) {
      if (! ProcessFunction())
        return false;
    } else
      // Ignore anything else.
      symbol_++;
  }

  // An N_SO with an empty name indicates the end of the compilation
  // unit.  Default to zero.
  uint64_t ending_address = 0;
  if (symbol_ < symbols_end_) {
    assert(symbol_->n_type == N_SO);
    const char *name = SymbolString();
    if (name[0] == '\0') {
      ending_address = SymbolValue();
      symbol_++;
    }
  }

  if (! handler_->EndCompilationUnit(ending_address))
    return false;

  return true;
}          

bool StabsReader::ProcessFunction() {
  assert(symbol_ < symbols_end_ && symbol_->n_type == N_FUN);

  uint64_t function_address = SymbolValue();
  // The STABS string for an N_FUN entry is the name of the function,
  // followed by a colon, followed by type information for the
  // function.  We want to pass the name alone to StartFunction.
  const char *stab_string = SymbolString();
  const char *name_end = strchr(stab_string, ':');
  if (! name_end)
    name_end = stab_string + strlen(stab_string);
  std::string name(stab_string, name_end - stab_string);
  if (! handler_->StartFunction(name, function_address))
    return false;
  symbol_++;

  while (symbol_ < symbols_end_) {
    if (symbol_->n_type == N_SO || symbol_->n_type == N_FUN)
      break;
    else if (symbol_->n_type == N_SLINE) {
      // The value of an N_SLINE entry is the offset of the line from
      // the function's start address.
      uint64_t line_address = function_address + SymbolValue();
      // The n_desc of a N_SLINE entry is the line number.  It's a
      // signed 16-bit field; line numbers from 32768 to 65535 are
      // stored as n-65536.
      uint16_t line_number = symbol_->n_desc;
      if (! handler_->Line(line_address, current_source_file_, line_number))
        return false;
      symbol_++;
    } else if (symbol_->n_type == N_SOL) {
      current_source_file_ = SymbolString();
      symbol_++;
    } else
      // Ignore anything else.
      symbol_++;
  }

  // If there is a subsequent N_SO or N_FUN entry, its address is our
  // end address.
  uint64_t ending_address = 0;
  if (symbol_ < symbols_end_) {
    assert(symbol_->n_type == N_SO || symbol_->n_type == N_FUN);
    ending_address = SymbolValue();
    // Note: we do not increment symbol_ here, since we haven't consumed it.
  }

  if (! handler_->EndFunction(ending_address))
    return false;

  return true;
}

} // namespace google_breakpad
