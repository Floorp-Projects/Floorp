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

// simple_symbol_supplier.cc: A simple SymbolSupplier implementation
//
// See simple_symbol_supplier.h for documentation.
//
// Author: Mark Mentovai

#include <sys/types.h>
#include <sys/stat.h>

#include <cassert>

#include "processor/simple_symbol_supplier.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/system_info.h"
#include "processor/pathname_stripper.h"

namespace google_breakpad {

static bool file_exists(const string &file_name) {
  struct stat sb;
  return stat(file_name.c_str(), &sb) == 0;
}

SymbolSupplier::SymbolResult SimpleSymbolSupplier::GetSymbolFile(
    const CodeModule *module, const SystemInfo *system_info,
    string *symbol_file) {
  assert(symbol_file);
  for (unsigned int path_index = 0; path_index < paths_.size(); ++path_index) {
    SymbolResult result;
    if ((result = GetSymbolFileAtPath(module, system_info, paths_[path_index],
                                      symbol_file)) != NOT_FOUND) {
      return result;
    }
  }
  return NOT_FOUND;
}

SymbolSupplier::SymbolResult SimpleSymbolSupplier::GetSymbolFileAtPath(
    const CodeModule *module, const SystemInfo *system_info,
    const string &root_path, string *symbol_file) {
  assert(symbol_file);
  if (!module)
    return NOT_FOUND;

  // Start with the base path.
  string path = root_path;

  // Append the debug (pdb) file name as a directory name.
  path.append("/");
  string debug_file_name = PathnameStripper::File(module->debug_file());
  if (debug_file_name.empty())
    return NOT_FOUND;
  path.append(debug_file_name);

  // Append the identifier as a directory name.
  path.append("/");
  string identifier = module->debug_identifier();
  if (identifier.empty())
    return NOT_FOUND;
  path.append(identifier);

  // Transform the debug file name into one ending in .sym.  If the existing
  // name ends in .pdb, strip the .pdb.  Otherwise, add .sym to the non-.pdb
  // name.
  path.append("/");
  string debug_file_extension =
      debug_file_name.substr(debug_file_name.size() - 4);
  transform(debug_file_extension.begin(), debug_file_extension.end(),
            debug_file_extension.begin(), tolower);
  if (debug_file_extension == ".pdb") {
    path.append(debug_file_name.substr(0, debug_file_name.size() - 4));
  } else {
    path.append(debug_file_name);
  }
  path.append(".sym");

  if (!file_exists(path))
    return NOT_FOUND;

  *symbol_file = path;
  return FOUND;
}

}  // namespace google_breakpad
