/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "common/linux/file_id.h"

//TODO: move this somewhere common, this is copied from dump_symbols.cc
// Format the Elf file identifier in IDENTIFIER as a UUID with the
// dashes removed.
void FormatIdentifier(unsigned char identifier[google_breakpad::kMDGUIDSize],
                      char result_guid[40]) {
  char identifier_str[40];
  google_breakpad::FileID::ConvertIdentifierToString(
      identifier,
      identifier_str,
      sizeof(identifier_str));
  int bufpos = 0;
  for (int i = 0; identifier_str[i] != '\0'; ++i)
    if (identifier_str[i] != '-')
      result_guid[bufpos++] = identifier_str[i];
  // Add an extra "0" by the end.  PDB files on Windows have an 'age'
  // number appended to the end of the file identifier; this isn't
  // really used or necessary on other platforms, but let's preserve
  // the pattern.
  result_guid[bufpos++] = '0';
  // And null terminate.
  result_guid[bufpos] = '\0';
}


int main(int argc, char** argv)
{
  if (argc != 2) {
    fprintf(stderr, "usage: fileid <elf file>\n");
    return 1;
  }

  unsigned char identifier[google_breakpad::kMDGUIDSize];
  google_breakpad::FileID file_id(argv[1]);
  if (!file_id.ElfFileIdentifier(identifier)) {
    fprintf(stderr, "%s: unable to generate file identifier\n",
            argv[1]);
    return 1;
  }
  char result_guid[40];
  FormatIdentifier(identifier, result_guid);
  printf("%s\n", result_guid);
  return 0;
}
