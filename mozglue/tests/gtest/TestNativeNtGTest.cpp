/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "gtest/gtest.h"

#include "mozilla/NativeNt.h"
#include "nsHashKeys.h"
#include "nsTHashSet.h"

TEST(TestNativeNtGTest, GenerateDependentModuleSet)
{
  mozilla::nt::PEHeaders executable(::GetModuleHandleW(nullptr));
  nsTHashSet<nsStringCaseInsensitiveHashKey> dependentModules;
  executable.EnumImportChunks([&](const char* aModule) {
    dependentModules.Insert(
        mozilla::nt::GetLeafName(NS_ConvertASCIItoUTF16(aModule)));
  });

  EXPECT_TRUE(dependentModules.Contains(u"mozglue.dll"_ns));
  EXPECT_TRUE(dependentModules.Contains(u"MOZGLUE.dll"_ns));
  EXPECT_FALSE(dependentModules.Contains(u"xxx.dll"_ns));
}

TEST(TestNativeNtGTest, GetLeafName)
{
  nsAutoString str;
  str = mozilla::nt::GetLeafName(u""_ns);
  EXPECT_STREQ(str.get(), L"");
  str = mozilla::nt::GetLeafName(u"\\"_ns);
  EXPECT_STREQ(str.get(), L"");
  str = mozilla::nt::GetLeafName(u"\\\\"_ns);
  EXPECT_STREQ(str.get(), L"");
  str = mozilla::nt::GetLeafName(u"abc\\def\\ghi"_ns);
  EXPECT_STREQ(str.get(), L"ghi");
  str = mozilla::nt::GetLeafName(u"abcdef"_ns);
  EXPECT_STREQ(str.get(), L"abcdef");
  str = mozilla::nt::GetLeafName(u"\\abcdef"_ns);
  EXPECT_STREQ(str.get(), L"abcdef");

  const auto kEntireText =
      u"\\"_ns
      u"\\\\abc"_ns
      u"\\x\\y\\z"_ns
      u"123\\456\\"_ns
      u"789"_ns;
  str = mozilla::nt::GetLeafName(Substring(kEntireText, 0, 0));
  EXPECT_STREQ(str.get(), L"");
  str = mozilla::nt::GetLeafName(Substring(kEntireText, 0, 1));
  EXPECT_STREQ(str.get(), L"");
  str = mozilla::nt::GetLeafName(Substring(kEntireText, 1, 5));
  EXPECT_STREQ(str.get(), L"abc");
  str = mozilla::nt::GetLeafName(Substring(kEntireText, 6, 6));
  EXPECT_STREQ(str.get(), L"z");
  str = mozilla::nt::GetLeafName(Substring(kEntireText, 12, 8));
  EXPECT_STREQ(str.get(), L"");
  str = mozilla::nt::GetLeafName(Substring(kEntireText, 20));
  EXPECT_STREQ(str.get(), L"789");
}
