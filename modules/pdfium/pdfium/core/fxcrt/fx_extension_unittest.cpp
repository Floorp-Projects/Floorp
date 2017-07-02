// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "core/fxcrt/fx_ext.h"

TEST(fxcrt, FXSYS_toHexDigit) {
  EXPECT_EQ(10, FXSYS_toHexDigit('a'));
  EXPECT_EQ(10, FXSYS_toHexDigit('A'));
  EXPECT_EQ(7, FXSYS_toHexDigit('7'));
  EXPECT_EQ(0, FXSYS_toHexDigit('i'));
}

TEST(fxcrt, FXSYS_toDecimalDigit) {
  EXPECT_EQ(7, FXSYS_toDecimalDigit('7'));
  EXPECT_EQ(0, FXSYS_toDecimalDigit('a'));
  EXPECT_EQ(7, FXSYS_toDecimalDigit(L'7'));
  EXPECT_EQ(0, FXSYS_toDecimalDigit(L'a'));
}

TEST(fxcrt, FXSYS_isDecimalDigit) {
  EXPECT_TRUE(FXSYS_isDecimalDigit('7'));
  EXPECT_TRUE(FXSYS_isDecimalDigit(L'7'));
  EXPECT_FALSE(FXSYS_isDecimalDigit('a'));
  EXPECT_FALSE(FXSYS_isDecimalDigit(L'a'));
}

TEST(fxcrt, FX_HashCode_Ascii) {
  EXPECT_EQ(0u, FX_HashCode_GetA("", false));
  EXPECT_EQ(65u, FX_HashCode_GetA("A", false));
  EXPECT_EQ(97u, FX_HashCode_GetA("A", true));
  EXPECT_EQ(31 * 65u + 66u, FX_HashCode_GetA("AB", false));
}

TEST(fxcrt, FX_HashCode_Wide) {
  EXPECT_EQ(0u, FX_HashCode_GetW(L"", false));
  EXPECT_EQ(65u, FX_HashCode_GetW(L"A", false));
  EXPECT_EQ(97u, FX_HashCode_GetW(L"A", true));
  EXPECT_EQ(1313 * 65u + 66u, FX_HashCode_GetW(L"AB", false));
}
