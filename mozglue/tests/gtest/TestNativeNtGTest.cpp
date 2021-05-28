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
