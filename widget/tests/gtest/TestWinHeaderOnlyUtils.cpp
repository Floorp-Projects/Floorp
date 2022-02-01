/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/WinHeaderOnlyUtils.h"

#include <shlwapi.h>

TEST(WinHeaderOnlyUtils, MozPathGetDriveNumber)
{
  constexpr auto TestPathGetDriveNumber = [](const wchar_t* aPath) {
    return mozilla::MozPathGetDriveNumber(aPath) ==
           ::PathGetDriveNumberW(aPath);
  };
  EXPECT_TRUE(TestPathGetDriveNumber(nullptr));
  EXPECT_TRUE(TestPathGetDriveNumber(L""));
  EXPECT_TRUE(TestPathGetDriveNumber(L" :"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"a:"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"C:\\file"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"x:/file"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"@:\\\\"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"B"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"abc:\\file"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"\\"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"\\:A"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"\\\\"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"\\\\\\"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"\\\\?"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"\\\\?\\"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"\\\\?\\\\"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"\\\\?\\c:\\"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"\\\\?\\A"));
  EXPECT_TRUE(TestPathGetDriveNumber(L"\\\\?\\ \\"));
}
