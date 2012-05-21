/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include <stdio.h>

#include "common/linux/file_id.h"

//TODO: move this somewhere common, this is copied from dump_symbols.cc
// Format the Elf file identifier in IDENTIFIER as a UUID with the
// dashes removed.
std::string FormatIdentifier(unsigned char identifier[16]) {
  char identifier_str[40];
  google_breakpad::FileID::ConvertIdentifierToString(
      identifier,
      identifier_str,
      sizeof(identifier_str));
  std::string id_no_dash;
  for (int i = 0; identifier_str[i] != '\0'; ++i)
    if (identifier_str[i] != '-')
      id_no_dash += identifier_str[i];
  // Add an extra "0" by the end.  PDB files on Windows have an 'age'
  // number appended to the end of the file identifier; this isn't
  // really used or necessary on other platforms, but let's preserve
  // the pattern.
  id_no_dash += '0';
  return id_no_dash;
}


int main(int argc, char** argv)
{
  if (argc != 2) {
    fprintf(stderr, "usage: file_id <elf file>\n");
    return 1;
  }

  unsigned char identifier[google_breakpad::kMDGUIDSize];
  google_breakpad::FileID file_id(argv[1]);
  if (!file_id.ElfFileIdentifier(identifier)) {
    fprintf(stderr, "%s: unable to generate file identifier\n",
            argv[1]);
    return 1;
  }
  printf("%s\n", FormatIdentifier(identifier).c_str());
  return 0;
}
