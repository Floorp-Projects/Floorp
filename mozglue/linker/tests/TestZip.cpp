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
 * test.zip is a basic test zip file with a central directory. It contains
 * four entries, in the following order:
 * "foo", "bar", "baz", "qux".
 * The entries are going to be read out of order.
 */
extern const unsigned char TEST_ZIP[];
extern const unsigned int TEST_ZIP_SIZE;
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
extern const unsigned char NO_CENTRAL_DIR_ZIP[];
extern const unsigned int NO_CENTRAL_DIR_ZIP_SIZE;
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
