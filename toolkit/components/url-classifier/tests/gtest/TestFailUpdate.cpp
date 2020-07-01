/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HashStore.h"
#include "mozilla/Unused.h"
#include "nsPrintfCString.h"
#include "string.h"

#include "Common.h"

static const char* kFilesInV2[] = {".vlpset", ".sbstore"};
static const char* kFilesInV4[] = {".vlpset", ".metadata"};

#define GTEST_MALWARE_TABLE_V4 "goog-malware-proto"_ns
#define GTEST_PHISH_TABLE_V4 "goog-phish-proto"_ns

#define ROOT_DIR u"safebrowsing"_ns
#define SB_FILE(x, y) NS_ConvertUTF8toUTF16(nsPrintfCString("%s%s", x, y))

template <typename T, size_t N>
static void CheckFileExist(const nsCString& aTable, const T (&aFiles)[N],
                           bool aExpectExists, const char* aMsg = nullptr) {
  for (uint32_t i = 0; i < N; i++) {
    // This is just a quick way to know if this is v4 table
    NS_ConvertUTF8toUTF16 SUB_DIR(strstr(aTable.get(), "-proto") ? "google4"
                                                                 : "");
    nsCOMPtr<nsIFile> file = GetFile(nsTArray<nsString>{
        ROOT_DIR, SUB_DIR, SB_FILE(aTable.get(), aFiles[i])});

    bool exists;
    file->Exists(&exists);

    if (aMsg) {
      ASSERT_EQ(aExpectExists, exists)
          << file->HumanReadablePath().get() << " " << aMsg;
    } else {
      ASSERT_EQ(aExpectExists, exists) << file->HumanReadablePath().get();
    }
  }
}

TEST(UrlClassifierFailUpdate, CheckTableReset)
{
  const bool FULL_UPDATE = true;
  const bool PARTIAL_UPDATE = false;

  // Apply V2 update
  {
    RefPtr<TableUpdateV2> update = new TableUpdateV2(GTEST_TABLE_V2);
    Unused << update->NewAddChunk(1);

    ApplyUpdate(update);

    // A successful V2 update should create .vlpset & .sbstore files
    CheckFileExist(GTEST_TABLE_V2, kFilesInV2, true,
                   "V2 update doesn't create vlpset or sbstore");
  }

  // Helper function to generate table update data
  auto func = [](RefPtr<TableUpdateV4> update, bool full, const char* str) {
    update->SetFullUpdate(full);
    nsCString prefix(str);
    update->NewPrefixes(prefix.Length(), prefix);
  };

  // Apply V4 update for table1
  {
    RefPtr<TableUpdateV4> update = new TableUpdateV4(GTEST_MALWARE_TABLE_V4);
    func(update, FULL_UPDATE, "test_prefix");

    ApplyUpdate(update);

    // A successful V4 update should create .vlpset & .metadata files
    CheckFileExist(GTEST_MALWARE_TABLE_V4, kFilesInV4, true,
                   "v4 update doesn't create vlpset or metadata");
  }

  // Apply V4 update for table2
  {
    RefPtr<TableUpdateV4> update = new TableUpdateV4(GTEST_PHISH_TABLE_V4);
    func(update, FULL_UPDATE, "test_prefix");

    ApplyUpdate(update);

    CheckFileExist(GTEST_PHISH_TABLE_V4, kFilesInV4, true,
                   "v4 update doesn't create vlpset or metadata");
  }

  // Apply V4 update with the same prefix in previous full udpate
  // This should cause an update error.
  {
    RefPtr<TableUpdateV4> update = new TableUpdateV4(GTEST_MALWARE_TABLE_V4);
    func(update, PARTIAL_UPDATE, "test_prefix");

    ApplyUpdate(update);

    // A fail update should remove files for that table
    CheckFileExist(GTEST_MALWARE_TABLE_V4, kFilesInV4, false,
                   "a fail v4 update doesn't remove the tables");

    // A fail update should NOT remove files for the other tables
    CheckFileExist(GTEST_TABLE_V2, kFilesInV2, true,
                   "a fail v4 update removes a v2 table");
    CheckFileExist(GTEST_PHISH_TABLE_V4, kFilesInV4, true,
                   "a fail v4 update removes the other v4 table");
  }
}
