/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <unistd.h>
#include "Zip.h"
#include "mozilla/RefPtr.h"

extern "C" void report_mapping() { }

/**
 * test.zip is a basic test zip file with a central directory. It contains
 * four entries, in the following order:
 * "foo", "bar", "baz", "qux".
 * The entries are going to be read out of order.
 */
const char *test_entries[] = {
  "baz", "foo", "bar", "qux"
};

/**
 * no_central_dir.zip is a hand crafted test zip with no central directory
 * entries. The Zip reader is expected to be able to traverse these entries
 * if requested in order, without reading a central directory
 * - First entry is a file "a", STOREd.
 * - Second entry is a file "b", STOREd, using a data descriptor. CRC is
 *   unknown, but compressed and uncompressed sizes are known in the local
 *   file header.
 * - Third entry is a file "c", DEFLATEd, using a data descriptor. CRC,
 *   compressed and uncompressed sizes are known in the local file header.
 *   This is the kind of entry that can be found in a zip that went through
 *   zipalign if it had a data descriptor originally.
 * - Fourth entry is a file "d", STOREd.
 */
const char *no_central_dir_entries[] = {
  "a", "b", "c", "d"
};

int main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "TEST-FAIL | TestZip | Expecting the directory containing test Zips\n");
    return 1;
  }
  chdir(argv[1]);
  Zip::Stream s;
  mozilla::RefPtr<Zip> z = ZipCollection::GetZip("test.zip");
  for (size_t i = 0; i < sizeof(test_entries) / sizeof(*test_entries); i++) {
    if (!z->GetStream(test_entries[i], &s)) {
      fprintf(stderr, "TEST-UNEXPECTED-FAIL | TestZip | test.zip: Couldn't get entry \"%s\"\n", test_entries[i]);
      return 1;
    }
  }
  fprintf(stderr, "TEST-PASS | TestZip | test.zip could be accessed fully\n");

  z = ZipCollection::GetZip("no_central_dir.zip");
  for (size_t i = 0; i < sizeof(no_central_dir_entries)
                         / sizeof(*no_central_dir_entries); i++) {
    if (!z->GetStream(no_central_dir_entries[i], &s)) {
      fprintf(stderr, "TEST-UNEXPECTED-FAIL | TestZip | no_central_dir.zip: Couldn't get entry \"%s\"\n", no_central_dir_entries[i]);
      return 1;
    }
  }
  fprintf(stderr, "TEST-PASS | TestZip | no_central_dir.zip could be accessed in order\n");

  return 0;
}
