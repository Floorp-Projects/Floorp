/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <unistd.h>
#include "Zip.h"
#include "mozilla/RefPtr.h"

#include "gtest/gtest.h"

Logging Logging::Singleton;

/**
 * ZIP_DATA(FOO, "foo") defines the variables FOO and FOO_SIZE.
 * The former contains the content of the "foo" file in the same directory
 * as this file, and FOO_SIZE its size.
 */
/* clang-format off */
#define ZIP_DATA(name, file)                          \
  __asm__(".global " #name "\n"                       \
          ".data\n"                                   \
          ".balign 16\n"                              \
          #name ":\n"                                 \
          "  .incbin \"" SRCDIR "/" file "\"\n"       \
          ".L" #name "_END:\n"                        \
          "  .size " #name ", .L" #name "_END-" #name \
          "\n"                                        \
          ".global " #name "_SIZE\n"                  \
          ".data\n"                                   \
          ".balign 4\n"                               \
          #name "_SIZE:\n"                            \
          "  .int .L" #name "_END-" #name "\n");      \
  extern const unsigned char name[];                  \
  extern const unsigned int name##_SIZE
/* clang-format on */

/**
 * test.zip is a basic test zip file with a central directory. It contains
 * four entries, in the following order:
 * "foo", "bar", "baz", "qux".
 * The entries are going to be read out of order.
 */
ZIP_DATA(TEST_ZIP, "test.zip");
const char* test_entries[] = {"baz", "foo", "bar", "qux"};

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
ZIP_DATA(NO_CENTRAL_DIR_ZIP, "no_central_dir.zip");
const char* no_central_dir_entries[] = {"a", "b", "c", "d"};

TEST(Zip, TestZip)
{
  Zip::Stream s;
  RefPtr<Zip> z = Zip::Create((void*)TEST_ZIP, TEST_ZIP_SIZE);
  for (auto& entry : test_entries) {
    ASSERT_TRUE(z->GetStream(entry, &s))
    << "Could not get entry \"" << entry << "\"";
  }
}

TEST(Zip, NoCentralDir)
{
  Zip::Stream s;
  RefPtr<Zip> z =
      Zip::Create((void*)NO_CENTRAL_DIR_ZIP, NO_CENTRAL_DIR_ZIP_SIZE);
  for (auto& entry : no_central_dir_entries) {
    ASSERT_TRUE(z->GetStream(entry, &s))
    << "Could not get entry \"" << entry << "\"";
  }
}
