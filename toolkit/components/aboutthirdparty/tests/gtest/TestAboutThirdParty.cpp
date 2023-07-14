/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include "gtest/gtest.h"

#include "../../AboutThirdPartyUtils.h"
#include "mozilla/AboutThirdParty.h"
#include "mozilla/ArrayUtils.h"
#include "nsTArray.h"

using namespace mozilla;

#define WEATHER_RU u"\x041F\x043E\x0433\x043E\x0434\x0430"_ns
#define WEATHER_JA u"\x5929\x6C17"_ns

TEST(AboutThirdParty, CompareIgnoreCase)
{
  EXPECT_EQ(CompareIgnoreCase(u""_ns, u""_ns), 0);
  EXPECT_EQ(CompareIgnoreCase(u"abc"_ns, u"aBc"_ns), 0);
  EXPECT_LT(CompareIgnoreCase(u"a"_ns, u"ab"_ns), 0);
  EXPECT_GT(CompareIgnoreCase(u"ab"_ns, u"A"_ns), 0);
  EXPECT_LT(CompareIgnoreCase(u""_ns, u"aB"_ns), 0);
  EXPECT_GT(CompareIgnoreCase(u"ab"_ns, u""_ns), 0);

  // non-ascii testcases
  EXPECT_EQ(CompareIgnoreCase(WEATHER_JA, WEATHER_JA), 0);
  EXPECT_EQ(CompareIgnoreCase(WEATHER_RU, WEATHER_RU), 0);
  EXPECT_LT(CompareIgnoreCase(WEATHER_RU, WEATHER_JA), 0);
  EXPECT_GT(CompareIgnoreCase(WEATHER_JA, WEATHER_RU), 0);
  EXPECT_EQ(CompareIgnoreCase(WEATHER_RU u"x"_ns WEATHER_JA,
                              WEATHER_RU u"X"_ns WEATHER_JA),
            0);
  EXPECT_GT(
      CompareIgnoreCase(WEATHER_RU u"a"_ns WEATHER_JA, WEATHER_RU u"A"_ns), 0);
  EXPECT_LT(CompareIgnoreCase(WEATHER_RU u"a"_ns WEATHER_RU,
                              WEATHER_RU u"A"_ns WEATHER_JA),
            0);
}

TEST(AboutThirdParty, MsiPackGuid)
{
  nsAutoString packedGuid;
  EXPECT_FALSE(
      MsiPackGuid(u"EDA620E3-AA98-3846-B81E-3493CB2E0E02"_ns, packedGuid));
  EXPECT_FALSE(
      MsiPackGuid(u"*EDA620E3-AA98-3846-B81E-3493CB2E0E02*"_ns, packedGuid));
  EXPECT_TRUE(
      MsiPackGuid(u"{EDA620E3-AA98-3846-B81E-3493CB2E0E02}"_ns, packedGuid));
  EXPECT_STREQ(packedGuid.get(), L"3E026ADE89AA64838BE14339BCE2E020");
}

TEST(AboutThirdParty, CorrectMsiComponentPath)
{
  nsAutoString testPath;

  testPath = u""_ns;
  EXPECT_FALSE(CorrectMsiComponentPath(testPath));

  testPath = u"\\\\server\\share"_ns;
  EXPECT_FALSE(CorrectMsiComponentPath(testPath));

  testPath = u"hello"_ns;
  EXPECT_FALSE(CorrectMsiComponentPath(testPath));

  testPath = u"02:\\Software"_ns;
  EXPECT_FALSE(CorrectMsiComponentPath(testPath));

  testPath = u"C:\\path\\"_ns;
  EXPECT_TRUE(CorrectMsiComponentPath(testPath));
  EXPECT_STREQ(testPath.get(), L"C:\\path\\");

  testPath = u"C?\\path\\"_ns;
  EXPECT_TRUE(CorrectMsiComponentPath(testPath));
  EXPECT_STREQ(testPath.get(), L"C:\\path\\");

  testPath = u"C:\\?path\\"_ns;
  EXPECT_TRUE(CorrectMsiComponentPath(testPath));
  EXPECT_STREQ(testPath.get(), L"C:\\path\\");

  testPath = u"\\?path\\"_ns;
  EXPECT_FALSE(CorrectMsiComponentPath(testPath));
}

TEST(AboutThirdParty, InstallLocations)
{
  const nsLiteralString kDirectoriesUnsorted[] = {
      u"C:\\duplicate\\"_ns, u"C:\\duplicate\\"_ns, u"C:\\app1\\"_ns,
      u"C:\\app2\\"_ns,      u"C:\\app11\\"_ns,     u"C:\\app12\\"_ns,
  };

  struct TestCase {
    nsLiteralString mFile;
    nsLiteralString mInstallPath;
  } const kTestCases[] = {
      {u"C:\\app\\sub\\file.dll"_ns, u""_ns},
      {u"C:\\app1\\sub\\file.dll"_ns, u"C:\\app1\\"_ns},
      {u"C:\\app11\\sub\\file.dll"_ns, u"C:\\app11\\"_ns},
      {u"C:\\app12\\sub\\file.dll"_ns, u"C:\\app12\\"_ns},
      {u"C:\\app13\\sub\\file.dll"_ns, u""_ns},
      {u"C:\\duplicate\\sub\\file.dll"_ns, u""_ns},
  };

  nsTArray<InstallLocationT> locations(ArrayLength(kDirectoriesUnsorted));
  for (size_t i = 0; i < ArrayLength(kDirectoriesUnsorted); ++i) {
    locations.EmplaceBack(kDirectoriesUnsorted[i], new InstalledApplication());
  }

  locations.Sort([](const InstallLocationT& aA, const InstallLocationT& aB) {
    return CompareIgnoreCase(aA.first(), aB.first());
  });

  for (const auto& testCase : kTestCases) {
    auto bounds = EqualRange(locations, 0, locations.Length(),
                             InstallLocationComparator(testCase.mFile));
    if (bounds.second - bounds.first != 1) {
      EXPECT_TRUE(testCase.mInstallPath.IsEmpty());
      continue;
    }

    EXPECT_EQ(locations[bounds.first].first(), testCase.mInstallPath);
  }
}
