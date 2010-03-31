// -*- mode: c++ -*-

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

// stabs_reader.h: Define StabsReader, a parser for STABS debugging
// information. A description of the STABS debugging format can be
// found at:
//
//    http://sourceware.org/gdb/current/onlinedocs/stabs_toc.html
//
// The comments here assume you understand the format.
//
// This parser assumes that the system's <a.out.h> and <stab.h>
// headers accurately describe the layout of the STABS data; this code
// will not parse STABS data for a system with a different address
// size or endianness.

#ifndef COMMON_LINUX_STABS_READER_H__
#define COMMON_LINUX_STABS_READER_H__

#include <stdint.h>
#include <cstddef>
#include <a.out.h>

#include <string>

namespace google_breakpad {

class StabsHandler;

class StabsReader {
 public:
  // Create a reader for the STABS debug information whose .stab
  // section is the STAB_SIZE bytes at STAB, and whose .stabstr
  // section is the STABSTR_SIZE bytes at STABSTR.  The reader will
  // call the member functions of HANDLER to report the information it
  // finds, when the reader's 'Process' member function is called.
  // 
  // Note that, in ELF, the .stabstr section should be found using the
  // 'sh_link' field of the .stab section header, not by name.
  StabsReader(const uint8_t *stab,    size_t stab_size,
              const uint8_t *stabstr, size_t stabstr_size,
              StabsHandler *handler);

  // Process the STABS data, calling the handler's member functions to
  // report what we find.  While the handler functions return true,
  // continue to process until we reach the end of the section.  If we
  // processed the entire section and all handlers returned true,
  // return true.  If any handler returned false, return false.
  bool Process();

 private:
  // Return the name of the current symbol.
  const char *SymbolString();

  // Return the value of the current symbol.
  const uint64_t SymbolValue() {
    return symbol_->n_value;
  }

  // Process a compilation unit starting at symbol_.  Return true
  // to continue processing, or false to abort.
  bool ProcessCompilationUnit();

  // Process a function in current_source_file_ starting at symbol_.
  // Return true to continue processing, or false to abort.
  bool ProcessFunction();

  // The debugging information we're reading.
  const struct nlist *symbols_, *symbols_end_;
  const uint8_t *stabstr_;
  size_t stabstr_size_;

  StabsHandler *handler_;

  // The offset of the current compilation unit's strings within stabstr_.
  size_t string_offset_;

  // The value string_offset_ should have for the next compilation unit,
  // as established by N_UNDF entries.
  size_t next_cu_string_offset_;

  // The current symbol we're processing.
  const struct nlist *symbol_;

  // The current source file name.
  const char *current_source_file_;
};

// Consumer-provided callback structure for the STABS reader.  Clients
// of the STABS reader provide an instance of this structure.  The
// reader then invokes the member functions of that instance to report
// the information it finds.
//
// The default definitions of the member functions do nothing, and return
// true so processing will continue.
class StabsHandler {
 public:
  StabsHandler() { }
  virtual ~StabsHandler() { }

  // Some general notes about the handler callback functions:

  // Processing proceeds until the end of the .stabs section, or until
  // one of these functions returns false.

  // The addresses given are as reported in the STABS info, without
  // regard for whether the module may be loaded at different
  // addresses at different times (a shared library, say).  When
  // processing STABS from an ELF shared library, the addresses given
  // all assume the library is loaded at its nominal load address.
  // They are *not* offsets from the nominal load address.  If you
  // want offsets, you must subtract off the library's nominal load
  // address.

  // The arguments to these functions named FILENAME are all
  // references to strings stored in the .stabstr section.  Because
  // both the Linux and Solaris linkers factor out duplicate strings
  // from the .stabstr section, the consumer can assume that if two
  // FILENAME values are different addresses, they represent different
  // file names.
  //
  // Thus, it's safe to use (say) std::map<char *, ...>, which does
  // string address comparisons, not string content comparisons.
  // Since all the strings are in same array of characters --- the
  // .stabstr section --- comparing their addresses produces
  // predictable, if not lexicographically meaningful, results.

  // Begin processing a compilation unit whose main source file is
  // named FILENAME, and whose base address is ADDRESS.  If
  // BUILD_DIRECTORY is non-NULL, it is the name of the build
  // directory in which the compilation occurred.
  virtual bool StartCompilationUnit(const char *filename, uint64_t address,
                                    const char *build_directory) {
    return true;
  }

  // Finish processing the compilation unit.  If ADDRESS is non-zero,
  // it is the ending address of the compilation unit.  If ADDRESS is
  // zero, then the compilation unit's ending address is not
  // available, and the consumer must infer it by other means.
  virtual bool EndCompilationUnit(uint64_t address) { return true; }

  // Begin processing a function named NAME, whose starting address is
  // ADDRESS.  This function belongs to the compilation unit that was
  // most recently started but not ended.
  //
  // Note that, unlike filenames, NAME is not a pointer into the
  // .stabstr section; this is because the name as it appears in the
  // STABS data is followed by type information.  The value passed to
  // StartFunction is the function name alone.
  //
  // In languages that use name mangling, like C++, NAME is mangled.
  virtual bool StartFunction(const std::string &name, uint64_t address) {
    return true;
  }

  // Finish processing the function.  If ADDRESS is non-zero, it is
  // the ending address for the function.  If ADDRESS is zero, then
  // the function's ending address is not available, and the consumer
  // must infer it by other means.
  virtual bool EndFunction(uint64_t address) { return true; }
  
  // Report that the code at ADDRESS is attributable to line NUMBER of
  // the source file named FILENAME.  The caller must infer the ending
  // address of the line.
  virtual bool Line(uint64_t address, const char *filename, int number) {
    return true;
  }

  // Report a warning.  FORMAT is a printf-like format string,
  // specifying how to format the subsequent arguments.
  virtual void Warning(const char *format, ...) = 0;
};

} // namespace google_breakpad

#endif  // COMMON_LINUX_STABS_READER_H__
