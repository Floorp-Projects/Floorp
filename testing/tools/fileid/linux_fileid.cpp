/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string>

#include "common/linux/file_id.h"
#include "common/memory.h"

using std::string;

using google_breakpad::auto_wasteful_vector;
using google_breakpad::FileID;
using google_breakpad::PageAllocator;

int main(int argc, char** argv)
{

  if (argc != 2) {
    fprintf(stderr, "usage: fileid <elf file>\n");
    return 1;
  }

  PageAllocator allocator;
  auto_wasteful_vector<uint8_t, sizeof(MDGUID)> identifier(&allocator);
  FileID file_id(argv[1]);
  if (!file_id.ElfFileIdentifier(identifier)) {
    fprintf(stderr, "%s: unable to generate file identifier\n",
            argv[1]);
    return 1;
  }

  string result_guid = FileID::ConvertIdentifierToUUIDString(identifier);

  // Add an extra "0" at the end.  PDB files on Windows have an 'age'
  // number appended to the end of the file identifier; this isn't
  // really used or necessary on other platforms, but be consistent.
  printf("%s0\n", result_guid.c_str());
  return 0;
}
