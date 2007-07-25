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

#include "processor/simple_symbol_supplier.h"
#include "google_airbag/processor/minidump.h"
#include "processor/pathname_stripper.h"

namespace google_airbag {

string SimpleSymbolSupplier::GetSymbolFileAtPath(MinidumpModule *module,
                                                 const string &root_path) {
  // For now, only support modules that have GUIDs - which means
  // MDCVInfoPDB70.

  if (!module)
    return "";

  const MDCVInfoPDB70 *cv_record =
      reinterpret_cast<const MDCVInfoPDB70*>(module->GetCVRecord());
  if (!cv_record)
    return "";

  if (cv_record->cv_signature != MD_CVINFOPDB70_SIGNATURE)
    return "";

  // Start with the base path.
  string path = root_path;

  // Append the pdb file name as a directory name.
  path.append("/");
  string pdb_file_name = PathnameStripper::File(
      reinterpret_cast<const char *>(cv_record->pdb_file_name));
  path.append(pdb_file_name);

  // Append the uuid and age as a directory name.
  path.append("/");
  char uuid_age_string[43];
  snprintf(uuid_age_string, sizeof(uuid_age_string),
           "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%X",
           cv_record->signature.data1, cv_record->signature.data2,
           cv_record->signature.data3,
           cv_record->signature.data4[0], cv_record->signature.data4[1],
           cv_record->signature.data4[2], cv_record->signature.data4[3],
           cv_record->signature.data4[4], cv_record->signature.data4[5],
           cv_record->signature.data4[6], cv_record->signature.data4[7],
           cv_record->age);
  path.append(uuid_age_string);

  // Transform the pdb file name into one ending in .sym.  If the existing
  // name ends in .pdb, strip the .pdb.  Otherwise, add .sym to the non-.pdb
  // name.
  path.append("/");
  string pdb_file_extension = pdb_file_name.substr(pdb_file_name.size() - 4);
  transform(pdb_file_extension.begin(), pdb_file_extension.end(),
            pdb_file_extension.begin(), tolower);
  if (pdb_file_extension == ".pdb") {
    path.append(pdb_file_name.substr(0, pdb_file_name.size() - 4));
  } else {
    path.append(pdb_file_name);
  }
  path.append(".sym");

  return path;
}

}  // namespace google_airbag
