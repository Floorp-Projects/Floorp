/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string>

#include "common/mac/arch_utilities.h"
#include "common/mac/file_id.h"

//TODO: move this somewhere common, this is copied from dump_symbols.cc
// Format the Mach-O identifier in IDENTIFIER as a UUID with the
// dashes removed.
std::string FormatIdentifier(unsigned char identifier[16])
{
  char identifier_string[40];
  google_breakpad::FileID::ConvertIdentifierToString(identifier, identifier_string,
                                                     sizeof(identifier_string));
  std::string compacted(identifier_string);
  for(size_t i = compacted.find('-'); i != std::string::npos;
      i = compacted.find('-', i))
    compacted.erase(i, 1);
  compacted += '0';
  return compacted;
}

int main(int argc, char** argv)
{
  if (argc != 2) {
    fprintf(stderr, "usage: fileid <object file>\n");
    return 1;
  }


  unsigned char identifier[16];
  google_breakpad::FileID file_id(argv[1]);

  // We should be able to use NXGetLocalArchInfo for this, but it returns
  // CPU_TYPE_X86 (which is the same as CPU_TYPE_I386) on x86_64 machines,
  // when our binary will typically have CPU_TYPE_X86_64 to match against.
  // So we hard code x86_64. In practice that's where we're running tests,
  // and that's what our debug binaries will contain.
  if (!file_id.MachoIdentifier(CPU_TYPE_X86_64, CPU_SUBTYPE_MULTIPLE,
                               identifier)) {
    fprintf(stderr, "%s: unable to generate file identifier\n",
            argv[1]);
    return 1;
  }

  printf("%s\n", FormatIdentifier(identifier).c_str());
  return 0;
}
