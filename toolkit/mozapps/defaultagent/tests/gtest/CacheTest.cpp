/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include <string>

#include "Cache.h"
#include "common.h"
#include "Registry.h"
#include "UtfConvert.h"

#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WinHeaderOnlyUtils.h"

using namespace mozilla::default_agent;

class WDBACacheTest : public ::testing::Test {
 protected:
  std::wstring mCacheRegKey;

  void SetUp() override {
    // Create a unique registry key to put the cache in for each test.
    const ::testing::TestInfo* const testInfo =
        ::testing::UnitTest::GetInstance()->current_test_info();
    Utf8ToUtf16Result testCaseResult = Utf8ToUtf16(testInfo->test_case_name());
    ASSERT_TRUE(testCaseResult.isOk());
    mCacheRegKey = testCaseResult.unwrap();

    Utf8ToUtf16Result testNameResult = Utf8ToUtf16(testInfo->name());
    ASSERT_TRUE(testNameResult.isOk());
    mCacheRegKey += L'.';
    mCacheRegKey += testNameResult.unwrap();

    FilePathResult uuidResult = GenerateUUIDStr();
    ASSERT_TRUE(uuidResult.isOk());
    mCacheRegKey += L'.';
    mCacheRegKey += uuidResult.unwrap();
  }

  void TearDown() override {
    // It seems like the TearDown probably doesn't run if SetUp doesn't
    // succeed, but I can't find any documentation saying that. And we don't
    // want to accidentally clobber the entirety of AGENT_REGKEY_NAME.
    if (!mCacheRegKey.empty()) {
      std::wstring regKey = AGENT_REGKEY_NAME;
      regKey += L'\\';
      regKey += mCacheRegKey;
      RegDeleteTreeW(HKEY_CURRENT_USER, regKey.c_str());
    }
  }
};

TEST_F(WDBACacheTest, BasicFunctionality) {
  Cache cache(mCacheRegKey.c_str());
  VoidResult result = cache.Init();
  ASSERT_TRUE(result.isOk());

  // Test that the cache starts empty
  Cache::MaybeEntryResult entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  Cache::MaybeEntry entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isNothing());

  // Test that the cache stops accepting items when it is full.
  ASSERT_EQ(Cache::kDefaultCapacity, 2U);
  Cache::Entry toWrite = Cache::Entry{
      .notificationType = "string1",
      .notificationShown = "string2",
      .notificationAction = "string3",
      .prevNotificationAction = "string4",
  };
  result = cache.Enqueue(toWrite);
  ASSERT_TRUE(result.isOk());
  toWrite = Cache::Entry{
      .notificationType = "string5",
      .notificationShown = "string6",
      .notificationAction = "string7",
      .prevNotificationAction = "string8",
  };
  result = cache.Enqueue(toWrite);
  ASSERT_TRUE(result.isOk());
  toWrite = Cache::Entry{
      .notificationType = "string9",
      .notificationShown = "string10",
      .notificationAction = "string11",
      .prevNotificationAction = "string12",
  };
  result = cache.Enqueue(toWrite);
  ASSERT_TRUE(result.isErr());

  // Read the two cache entries back out and test that they match the expected
  // values.
  entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isSome());
  ASSERT_EQ(entry.value().entryVersion, 2U);
  ASSERT_EQ(entry.value().notificationType, "string1");
  ASSERT_EQ(entry.value().notificationShown, "string2");
  ASSERT_EQ(entry.value().notificationAction, "string3");
  ASSERT_TRUE(entry.value().prevNotificationAction.isSome());
  ASSERT_EQ(entry.value().prevNotificationAction.value(), "string4");

  entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isSome());
  ASSERT_EQ(entry.value().entryVersion, 2U);
  ASSERT_EQ(entry.value().notificationType, "string5");
  ASSERT_EQ(entry.value().notificationShown, "string6");
  ASSERT_EQ(entry.value().notificationAction, "string7");
  ASSERT_TRUE(entry.value().prevNotificationAction.isSome());
  ASSERT_EQ(entry.value().prevNotificationAction.value(), "string8");

  entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isNothing());
}

TEST_F(WDBACacheTest, Version1Migration) {
  // Set up 2 version 1 cache entries
  VoidResult result = RegistrySetValueString(
      IsPrefixed::Unprefixed, L"PingCacheNotificationType0", "string1");
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueString(IsPrefixed::Unprefixed,
                                  L"PingCacheNotificationShown0", "string2");
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueString(IsPrefixed::Unprefixed,
                                  L"PingCacheNotificationAction0", "string3");
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueString(IsPrefixed::Unprefixed,
                                  L"PingCacheNotificationType1", "string4");
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueString(IsPrefixed::Unprefixed,
                                  L"PingCacheNotificationShown1", "string5");
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueString(IsPrefixed::Unprefixed,
                                  L"PingCacheNotificationAction1", "string6");
  ASSERT_TRUE(result.isOk());

  Cache cache(mCacheRegKey.c_str());
  result = cache.Init();
  ASSERT_TRUE(result.isOk());

  Cache::MaybeEntryResult entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  Cache::MaybeEntry entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isSome());
  ASSERT_EQ(entry.value().entryVersion, 1U);
  ASSERT_EQ(entry.value().notificationType, "string1");
  ASSERT_EQ(entry.value().notificationShown, "string2");
  ASSERT_EQ(entry.value().notificationAction, "string3");
  ASSERT_TRUE(entry.value().prevNotificationAction.isNothing());

  // Insert a new item to test coexistence of different versions
  Cache::Entry toWrite = Cache::Entry{
      .notificationType = "string7",
      .notificationShown = "string8",
      .notificationAction = "string9",
      .prevNotificationAction = "string10",
  };
  result = cache.Enqueue(toWrite);
  ASSERT_TRUE(result.isOk());

  entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isSome());
  ASSERT_EQ(entry.value().entryVersion, 1U);
  ASSERT_EQ(entry.value().notificationType, "string4");
  ASSERT_EQ(entry.value().notificationShown, "string5");
  ASSERT_EQ(entry.value().notificationAction, "string6");
  ASSERT_TRUE(entry.value().prevNotificationAction.isNothing());

  entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isSome());
  ASSERT_EQ(entry.value().entryVersion, 2U);
  ASSERT_EQ(entry.value().notificationType, "string7");
  ASSERT_EQ(entry.value().notificationShown, "string8");
  ASSERT_EQ(entry.value().notificationAction, "string9");
  ASSERT_TRUE(entry.value().prevNotificationAction.isSome());
  ASSERT_EQ(entry.value().prevNotificationAction.value(), "string10");

  entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isNothing());
}

TEST_F(WDBACacheTest, ForwardsCompatibility) {
  // Set up a cache that might have been made by a future version with a larger
  // capacity set and more keys per entry.
  std::wstring settingsKey = mCacheRegKey + L"\\version2";
  VoidResult result = RegistrySetValueDword(
      IsPrefixed::Unprefixed, Cache::kCapacityRegName, 8, settingsKey.c_str());
  ASSERT_TRUE(result.isOk());
  // We're going to insert the future version's entry at index 6 so there's
  // space for 1 more before we loop back to index 0. Then we are going to
  // enqueue 2 new values to test that this works properly.
  result = RegistrySetValueDword(IsPrefixed::Unprefixed, Cache::kFrontRegName,
                                 6, settingsKey.c_str());
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueDword(IsPrefixed::Unprefixed, Cache::kSizeRegName, 1,
                                 settingsKey.c_str());
  ASSERT_TRUE(result.isOk());

  // Insert an entry as if it was inserted by a future version
  std::wstring entryRegKey = settingsKey + L"\\6";
  result =
      RegistrySetValueDword(IsPrefixed::Unprefixed, Cache::kEntryVersionKey,
                            9999, entryRegKey.c_str());
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueString(IsPrefixed::Unprefixed,
                                  Cache::kNotificationTypeKey, "string1",
                                  entryRegKey.c_str());
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueString(IsPrefixed::Unprefixed,
                                  Cache::kNotificationShownKey, "string2",
                                  entryRegKey.c_str());
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueString(IsPrefixed::Unprefixed,
                                  Cache::kNotificationActionKey, "string3",
                                  entryRegKey.c_str());
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueString(IsPrefixed::Unprefixed,
                                  Cache::kPrevNotificationActionKey, "string4",
                                  entryRegKey.c_str());
  ASSERT_TRUE(result.isOk());
  result = RegistrySetValueString(IsPrefixed::Unprefixed, L"UnknownFutureKey",
                                  "string5", entryRegKey.c_str());
  ASSERT_TRUE(result.isOk());

  Cache cache(mCacheRegKey.c_str());
  result = cache.Init();
  ASSERT_TRUE(result.isOk());

  // Insert 2 new items to test that these features work with a different
  // capacity.
  Cache::Entry toWrite = Cache::Entry{
      .notificationType = "string6",
      .notificationShown = "string7",
      .notificationAction = "string8",
      .prevNotificationAction = "string9",
  };
  result = cache.Enqueue(toWrite);
  ASSERT_TRUE(result.isOk());
  toWrite = Cache::Entry{
      .notificationType = "string10",
      .notificationShown = "string11",
      .notificationAction = "string12",
      .prevNotificationAction = "string13",
  };
  result = cache.Enqueue(toWrite);
  ASSERT_TRUE(result.isOk());

  // Read cache and verify the output
  Cache::MaybeEntryResult entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  Cache::MaybeEntry entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isSome());
  ASSERT_EQ(entry.value().entryVersion, 9999U);
  ASSERT_EQ(entry.value().notificationType, "string1");
  ASSERT_EQ(entry.value().notificationShown, "string2");
  ASSERT_EQ(entry.value().notificationAction, "string3");
  ASSERT_TRUE(entry.value().prevNotificationAction.isSome());
  ASSERT_EQ(entry.value().prevNotificationAction.value(), "string4");

  entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isSome());
  ASSERT_EQ(entry.value().entryVersion, 2U);
  ASSERT_EQ(entry.value().notificationType, "string6");
  ASSERT_EQ(entry.value().notificationShown, "string7");
  ASSERT_EQ(entry.value().notificationAction, "string8");
  ASSERT_TRUE(entry.value().prevNotificationAction.isSome());
  ASSERT_EQ(entry.value().prevNotificationAction.value(), "string9");

  entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isSome());
  ASSERT_EQ(entry.value().entryVersion, 2U);
  ASSERT_EQ(entry.value().notificationType, "string10");
  ASSERT_EQ(entry.value().notificationShown, "string11");
  ASSERT_EQ(entry.value().notificationAction, "string12");
  ASSERT_TRUE(entry.value().prevNotificationAction.isSome());
  ASSERT_EQ(entry.value().prevNotificationAction.value(), "string13");

  entryResult = cache.Dequeue();
  ASSERT_TRUE(entryResult.isOk());
  entry = entryResult.unwrap();
  ASSERT_TRUE(entry.isNothing());
}
