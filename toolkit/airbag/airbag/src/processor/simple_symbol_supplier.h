// Copyright (c) 2006, Google Inc.
// All rights reserved.
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

// simple_symbol_supplier.h: A simple SymbolSupplier implementation
//
// SimpleSymbolSupplier is a straightforward implementation of SymbolSupplier
// that stores symbol files in a filesystem tree.  A SimpleSymbolSupplier is
// created with a base directory, which is the root for all symbol files.
// Each symbol file contained therien has a directory entry in the base
// directory with a name identical to the corresponding pdb file.  Within
// each of these directories, there are subdirectories named for the uuid and
// age of each pdb file.  The uuid is presented in hexadecimal form, with
// uppercase characters and no dashes.  The age is appended to it in
// hexadecimal form, without any separators.  Within that subdirectory,
// SimpleSymbolSupplier expects to find the symbol file, which is named
// identically to the pdb file, but with a .sym extension.  This sample
// hierarchy is rooted at the "symbols" base directory:
//
// symbols
// symbols/test_app.pdb
// symbols/test_app.pdb/63FE4780728D49379B9D7BB6460CB42A1
// symbols/test_app.pdb/63FE4780728D49379B9D7BB6460CB42A1/test_app.sym
// symbols/kernel32.pdb
// symbols/kernel32.pdb/BCE8785C57B44245A669896B6A19B9542
// symbols/kernel32.pdb/BCE8785C57B44245A669896B6A19B9542/kernel32.sym
//
// In this case, the uuid of test_app.pdb is
// 63fe4780-728d-4937-9b9d-7bb6460cb42a and its age is 1.
//
// This scheme was chosen to be roughly analogous to the way that
// symbol files may be accessed from Microsoft Symbol Server.  A hierarchy
// used for Microsoft Symbol Server storage is usable as a hierarchy for
// SimpleSymbolServer, provided that the pdb files are transformed to dumped
// format using a tool such as dump_syms, and given a .sym extension.
//
// SimpleSymbolSupplier presently only supports symbol files that have
// the MSVC 7.0 CodeView record format.  See MDCVInfoPDB70 in
// minidump_format.h.
//
// Author: Mark Mentovai

#ifndef PROCESSOR_SIMPLE_SYMBOL_SUPPLIER_H__
#define PROCESSOR_SIMPLE_SYMBOL_SUPPLIER_H__

#include <string>

#include "google_airbag/processor/symbol_supplier.h"

namespace google_airbag {

using std::string;

class MinidumpModule;

class SimpleSymbolSupplier : public SymbolSupplier {
 public:
  // Creates a new SimpleSymbolSupplier, using path as the root path where
  // symbols are stored.
  explicit SimpleSymbolSupplier(const string &path) : path_(path) {}

  virtual ~SimpleSymbolSupplier() {}

  // Returns the path to the symbol file for the given module.  See the
  // description above.
  virtual string GetSymbolFile(MinidumpModule *module) {
    return GetSymbolFileAtPath(module, path_);
  }

 protected:
  string GetSymbolFileAtPath(MinidumpModule *module, const string &root_path);

 private:
  string path_;
};

}  // namespace google_airbag

#endif  // PROCESSOR_SIMPLE_SYMBOL_SUPPLIER_H__
