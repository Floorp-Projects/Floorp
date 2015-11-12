/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/DataStorage.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsStreamUtils.h"
#include "prtime.h"

using namespace mozilla;

class DataStorageTest : public ::testing::Test
{
protected:
  virtual void SetUp()
  {
    const ::testing::TestInfo* const testInfo =
      ::testing::UnitTest::GetInstance()->current_test_info();
    NS_ConvertUTF8toUTF16 testName(testInfo->name());
    storage = DataStorage::Get(testName);
    storage->Init(dataWillPersist);
  }

  RefPtr<DataStorage> storage;
  bool dataWillPersist;
};

NS_NAMED_LITERAL_CSTRING(testKey, "test");
NS_NAMED_LITERAL_CSTRING(testValue, "value");
NS_NAMED_LITERAL_CSTRING(privateTestValue, "private");

TEST_F(DataStorageTest, GetPutRemove)
{
  EXPECT_TRUE(dataWillPersist);

  // Test Put/Get on Persistent data
  EXPECT_EQ(NS_OK, storage->Put(testKey, testValue, DataStorage_Persistent));
  // Don't re-use testKey / testValue here, to make sure that this works as
  // expected with objects that have the same semantic value but are not
  // literally the same object.
  nsCString result = storage->Get(NS_LITERAL_CSTRING("test"),
                                  DataStorage_Persistent);
  EXPECT_STREQ("value", result.get());

  // Get on Temporary/Private data with the same key should give nothing
  result = storage->Get(testKey, DataStorage_Temporary);
  EXPECT_TRUE(result.IsEmpty());
  result = storage->Get(testKey, DataStorage_Private);
  EXPECT_TRUE(result.IsEmpty());

  // Put with Temporary/Private data shouldn't affect Persistent data
  NS_NAMED_LITERAL_CSTRING(temporaryTestValue, "temporary");
  EXPECT_EQ(NS_OK, storage->Put(testKey, temporaryTestValue,
                                DataStorage_Temporary));
  EXPECT_EQ(NS_OK, storage->Put(testKey, privateTestValue,
                                DataStorage_Private));
  result = storage->Get(testKey, DataStorage_Temporary);
  EXPECT_STREQ("temporary", result.get());
  result = storage->Get(testKey, DataStorage_Private);
  EXPECT_STREQ("private", result.get());
  result = storage->Get(testKey, DataStorage_Persistent);
  EXPECT_STREQ("value", result.get());

  // Put of a previously-present key overwrites it (if of the same type)
  NS_NAMED_LITERAL_CSTRING(newValue, "new");
  EXPECT_EQ(NS_OK, storage->Put(testKey, newValue, DataStorage_Persistent));
  result = storage->Get(testKey, DataStorage_Persistent);
  EXPECT_STREQ("new", result.get());

  // Removal should work
  storage->Remove(testKey, DataStorage_Temporary);
  result = storage->Get(testKey, DataStorage_Temporary);
  EXPECT_TRUE(result.IsEmpty());
  // But removing one type shouldn't affect the others
  result = storage->Get(testKey, DataStorage_Private);
  EXPECT_STREQ("private", result.get());
  result = storage->Get(testKey, DataStorage_Persistent);
  EXPECT_STREQ("new", result.get());
  // Test removing the other types as well
  storage->Remove(testKey, DataStorage_Private);
  result = storage->Get(testKey, DataStorage_Private);
  EXPECT_TRUE(result.IsEmpty());
  storage->Remove(testKey, DataStorage_Persistent);
  result = storage->Get(testKey, DataStorage_Persistent);
  EXPECT_TRUE(result.IsEmpty());
}

TEST_F(DataStorageTest, InputValidation)
{
  EXPECT_TRUE(dataWillPersist);

  // Keys may not have tabs or newlines
  EXPECT_EQ(NS_ERROR_INVALID_ARG,
            storage->Put(NS_LITERAL_CSTRING("key\thas tab"), testValue,
                   DataStorage_Persistent));
  nsCString result = storage->Get(NS_LITERAL_CSTRING("key\thas tab"),
                            DataStorage_Persistent);
  EXPECT_TRUE(result.IsEmpty());
  EXPECT_EQ(NS_ERROR_INVALID_ARG,
            storage->Put(NS_LITERAL_CSTRING("key has\nnewline"), testValue,
                   DataStorage_Persistent));
  result = storage->Get(NS_LITERAL_CSTRING("keyhas\nnewline"),
                  DataStorage_Persistent);
  EXPECT_TRUE(result.IsEmpty());
  // Values may not have newlines
  EXPECT_EQ(NS_ERROR_INVALID_ARG,
            storage->Put(testKey, NS_LITERAL_CSTRING("value\nhas newline"),
                   DataStorage_Persistent));
  result = storage->Get(testKey, DataStorage_Persistent);
  // Values may have tabs
  EXPECT_TRUE(result.IsEmpty());
  EXPECT_EQ(NS_OK, storage->Put(testKey,
                                NS_LITERAL_CSTRING("val\thas tab; this is ok"),
                                DataStorage_Persistent));
  result = storage->Get(testKey, DataStorage_Persistent);
  EXPECT_STREQ("val\thas tab; this is ok", result.get());

  nsCString longKey("a");
  for (int i = 0; i < 8; i++) {
    longKey.Append(longKey);
  }
  // A key of length 256 will work
  EXPECT_EQ(NS_OK, storage->Put(longKey, testValue, DataStorage_Persistent));
  result = storage->Get(longKey, DataStorage_Persistent);
  EXPECT_STREQ("value", result.get());
  longKey.Append("a");
  // A key longer than that will not work
  EXPECT_EQ(NS_ERROR_INVALID_ARG,
            storage->Put(longKey, testValue, DataStorage_Persistent));
  result = storage->Get(longKey, DataStorage_Persistent);
  EXPECT_TRUE(result.IsEmpty());

  nsCString longValue("a");
  for (int i = 0; i < 10; i++) {
    longValue.Append(longValue);
  }
  // A value of length 1024 will work
  EXPECT_EQ(NS_OK, storage->Put(testKey, longValue, DataStorage_Persistent));
  result = storage->Get(testKey, DataStorage_Persistent);
  EXPECT_STREQ(longValue.get(), result.get());
  longValue.Append("a");
  // A value longer than that will not work
  storage->Remove(testKey, DataStorage_Persistent);
  EXPECT_EQ(NS_ERROR_INVALID_ARG,
            storage->Put(testKey, longValue, DataStorage_Persistent));
  result = storage->Get(testKey, DataStorage_Persistent);
  EXPECT_TRUE(result.IsEmpty());
}

TEST_F(DataStorageTest, Eviction)
{
  EXPECT_TRUE(dataWillPersist);

  // Eviction is on a per-table basis. Tables shouldn't affect each other.
  EXPECT_EQ(NS_OK, storage->Put(testKey, testValue, DataStorage_Persistent));
  for (int i = 0; i < 1025; i++) {
    EXPECT_EQ(NS_OK, storage->Put(nsPrintfCString("%d", i),
                                  nsPrintfCString("%d", i),
                                  DataStorage_Temporary));
    nsCString result = storage->Get(nsPrintfCString("%d", i),
                                    DataStorage_Temporary);
    EXPECT_STREQ(nsPrintfCString("%d", i).get(), result.get());
  }
  // We don't know which entry got evicted, but we can count them.
  int entries = 0;
  for (int i = 0; i < 1025; i++) {
    nsCString result = storage->Get(nsPrintfCString("%d", i),
                                    DataStorage_Temporary);
    if (!result.IsEmpty()) {
      entries++;
    }
  }
  EXPECT_EQ(entries, 1024);
  nsCString result = storage->Get(testKey, DataStorage_Persistent);
  EXPECT_STREQ("value", result.get());
}

TEST_F(DataStorageTest, ClearPrivateData)
{
  EXPECT_TRUE(dataWillPersist);

  EXPECT_EQ(NS_OK, storage->Put(testKey, privateTestValue,
                                DataStorage_Private));
  nsCString result = storage->Get(testKey, DataStorage_Private);
  EXPECT_STREQ("private", result.get());
  storage->Observe(nullptr, "last-pb-context-exited", nullptr);
  result = storage->Get(testKey, DataStorage_Private);
  EXPECT_TRUE(result.IsEmpty());
}

TEST_F(DataStorageTest, Shutdown)
{
  EXPECT_TRUE(dataWillPersist);

  EXPECT_EQ(NS_OK, storage->Put(testKey, testValue, DataStorage_Persistent));
  nsCString result = storage->Get(testKey, DataStorage_Persistent);
  EXPECT_STREQ("value", result.get());
  // Get "now" (in days) close to when the data was last touched, so we won't
  // get intermittent failures with the day not matching.
  int64_t microsecondsPerDay = 24 * 60 * 60 * int64_t(PR_USEC_PER_SEC);
  int32_t nowInDays = int32_t(PR_Now() / microsecondsPerDay);
  storage->Observe(nullptr, "profile-before-change", nullptr);
  nsCOMPtr<nsIFile> backingFile;
  EXPECT_EQ(NS_OK, NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                          getter_AddRefs(backingFile)));
  const ::testing::TestInfo* const testInfo =
    ::testing::UnitTest::GetInstance()->current_test_info();
  NS_ConvertUTF8toUTF16 testName(testInfo->name());
  EXPECT_EQ(NS_OK, backingFile->Append(testName));
  nsCOMPtr<nsIInputStream> fileInputStream;
  EXPECT_EQ(NS_OK, NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream),
                                              backingFile));
  nsCString data;
  EXPECT_EQ(NS_OK, NS_ConsumeStream(fileInputStream, UINT32_MAX, data));
  // The data will be of the form 'test\t0\t<days since the epoch>\tvalue'
  EXPECT_STREQ(nsPrintfCString("test\t0\t%d\tvalue\n", nowInDays).get(),
               data.get());
}
