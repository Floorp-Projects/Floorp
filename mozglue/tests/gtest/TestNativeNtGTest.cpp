/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "gtest/gtest.h"

#include "mozilla/NativeNt.h"

TEST(TestNativeNtGTest, GenerateDependentModuleSet)
{
  mozilla::nt::PEHeaders executable(::GetModuleHandleW(nullptr));
  auto dependentModules = executable.GenerateDependentModuleSet();
  EXPECT_NE(dependentModules.GetEntry(NS_LITERAL_STRING("mozglue.dll")),
            nullptr);
  EXPECT_NE(dependentModules.GetEntry(NS_LITERAL_STRING("MOZGLUE.dll")),
            nullptr);
  EXPECT_EQ(dependentModules.GetEntry(NS_LITERAL_STRING("xxx.dll")), nullptr);
}
