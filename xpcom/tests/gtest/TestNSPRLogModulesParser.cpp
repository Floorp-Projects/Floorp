/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSPRLogModulesParser.h"
#include "gtest/gtest.h"

using namespace mozilla;

TEST(NSPRLogModulesParser, Empty)
{
  bool callbackInvoked = false;
  auto callback = [&](const char*, mozilla::LogLevel, int32_t) mutable { callbackInvoked = true; };

  mozilla::NSPRLogModulesParser(nullptr, callback);
  EXPECT_FALSE(callbackInvoked);

  mozilla::NSPRLogModulesParser("", callback);
  EXPECT_FALSE(callbackInvoked);
}

TEST(NSPRLogModulesParser, DefaultLevel)
{
  bool callbackInvoked = false;
  auto callback =
      [&](const char* aName, mozilla::LogLevel aLevel, int32_t) {
        EXPECT_STREQ("Foo", aName);
        EXPECT_EQ(mozilla::LogLevel::Error, aLevel);
        callbackInvoked = true;
      };

  mozilla::NSPRLogModulesParser("Foo", callback);
  EXPECT_TRUE(callbackInvoked);

  callbackInvoked = false;
  mozilla::NSPRLogModulesParser("Foo:", callback);
  EXPECT_TRUE(callbackInvoked);
}

TEST(NSPRLogModulesParser, LevelSpecified)
{
  std::pair<const char*, mozilla::LogLevel> expected[] = {
    { "Foo:0", mozilla::LogLevel::Disabled },
    { "Foo:1", mozilla::LogLevel::Error },
    { "Foo:2", mozilla::LogLevel::Warning },
    { "Foo:3", mozilla::LogLevel::Info },
    { "Foo:4", mozilla::LogLevel::Debug },
    { "Foo:5", mozilla::LogLevel::Verbose },
    { "Foo:25", mozilla::LogLevel::Verbose },  // too high
    { "Foo:-12", mozilla::LogLevel::Disabled } // too low
  };

  auto* currTest = expected;

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(expected); i++) {
    bool callbackInvoked = false;
    mozilla::NSPRLogModulesParser(currTest->first,
        [&](const char* aName, mozilla::LogLevel aLevel, int32_t) {
          EXPECT_STREQ("Foo", aName);
          EXPECT_EQ(currTest->second, aLevel);
          callbackInvoked = true;
        });
    EXPECT_TRUE(callbackInvoked);
    currTest++;
  }
}

TEST(NSPRLogModulesParser, Multiple)
{
  std::pair<const char*, mozilla::LogLevel> expected[] = {
    { "timestamp", mozilla::LogLevel::Error },
    { "Foo", mozilla::LogLevel::Info },
    { "Bar", mozilla::LogLevel::Error },
    { "Baz", mozilla::LogLevel::Warning },
    { "Qux", mozilla::LogLevel::Verbose },
  };

  const size_t kExpectedCount = MOZ_ARRAY_LENGTH(expected);

  auto* currTest = expected;

  size_t count = 0;
  mozilla::NSPRLogModulesParser("timestamp,Foo:3, Bar,Baz:2,    Qux:5",
      [&](const char* aName, mozilla::LogLevel aLevel, int32_t) mutable {
        ASSERT_LT(count, kExpectedCount);
        EXPECT_STREQ(currTest->first, aName);
        EXPECT_EQ(currTest->second, aLevel);
        currTest++;
        count++;
     });

  EXPECT_EQ(kExpectedCount, count);
}

TEST(NSPRLogModulesParser, RawArg)
{
  bool callbackInvoked = false;
  auto callback =
    [&](const char* aName, mozilla::LogLevel aLevel, int32_t aRawValue) {
    EXPECT_STREQ("Foo", aName);
    EXPECT_EQ(mozilla::LogLevel::Verbose, aLevel);
    EXPECT_EQ(1000, aRawValue);
    callbackInvoked = true;
  };

  mozilla::NSPRLogModulesParser("Foo:1000", callback);
  EXPECT_TRUE(callbackInvoked);
}
