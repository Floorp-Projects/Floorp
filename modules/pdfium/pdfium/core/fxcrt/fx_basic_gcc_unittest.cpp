// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/fx_system.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(fxcrt, FXSYS_atoi) {
  EXPECT_EQ(0, FXSYS_atoi(""));
  EXPECT_EQ(0, FXSYS_atoi("0"));
  EXPECT_EQ(-1, FXSYS_atoi("-1"));
  EXPECT_EQ(2345, FXSYS_atoi("2345"));
  EXPECT_EQ(-2147483647, FXSYS_atoi("-2147483647"));
  // Handle the sign.
  EXPECT_EQ(-2345, FXSYS_atoi("-2345"));
  EXPECT_EQ(2345, FXSYS_atoi("+2345"));
  // The max value.
  EXPECT_EQ(2147483647, FXSYS_atoi("2147483647"));
  // The min value. Written in -1 format to avoid "unary minus operator applied
  // to unsigned type" warning.
  EXPECT_EQ(-2147483647 - 1, FXSYS_atoi("-2147483648"));
  // With invalid char.
  EXPECT_EQ(9, FXSYS_atoi("9x9"));

  // Out of range values.
  EXPECT_EQ(2147483647, FXSYS_atoi("2147483623423412348"));
  EXPECT_EQ(2147483647, FXSYS_atoi("2147483648"));
  EXPECT_EQ(-2147483647 - 1, FXSYS_atoi("-2147483650"));
}

TEST(fxcrt, FXSYS_atoi64) {
  EXPECT_EQ(0, FXSYS_atoi64(""));
  EXPECT_EQ(0, FXSYS_atoi64("0"));
  EXPECT_EQ(-1, FXSYS_atoi64("-1"));
  EXPECT_EQ(2345, FXSYS_atoi64("2345"));
  EXPECT_EQ(-9223372036854775807LL, FXSYS_atoi64("-9223372036854775807"));
  // Handle the sign.
  EXPECT_EQ(-2345, FXSYS_atoi64("-2345"));
  EXPECT_EQ(2345, FXSYS_atoi64("+2345"));
  // The max value.
  EXPECT_EQ(9223372036854775807LL, FXSYS_atoi64("9223372036854775807"));
  // The min value. Written in -1 format to avoid implicit unsigned warning.
  EXPECT_EQ(-9223372036854775807LL - 1LL, FXSYS_atoi64("-9223372036854775808"));
  // With invalid char.
  EXPECT_EQ(9, FXSYS_atoi64("9x9"));

  // Out of range values.
  EXPECT_EQ(9223372036854775807LL, FXSYS_atoi64("922337203685471234123475807"));
  EXPECT_EQ(9223372036854775807LL, FXSYS_atoi64("9223372036854775808"));
  EXPECT_EQ(-9223372036854775807LL - 1LL, FXSYS_atoi64("-9223372036854775810"));
}

TEST(fxcrt, FXSYS_wtoi) {
  EXPECT_EQ(0, FXSYS_wtoi(L""));
  EXPECT_EQ(0, FXSYS_wtoi(L"0"));
  EXPECT_EQ(-1, FXSYS_wtoi(L"-1"));
  EXPECT_EQ(2345, FXSYS_wtoi(L"2345"));
  EXPECT_EQ(-2147483647, FXSYS_wtoi(L"-2147483647"));
  // The max value.
  EXPECT_EQ(2147483647, FXSYS_wtoi(L"2147483647"));
  // The min value.
  EXPECT_EQ(-2147483647 - 1, FXSYS_wtoi(L"-2147483648"));
  EXPECT_EQ(9, FXSYS_wtoi64(L"9x9"));

  // Out of range values.
  EXPECT_EQ(2147483647, FXSYS_wtoi(L"2147483623423412348"));
  EXPECT_EQ(2147483647, FXSYS_wtoi(L"2147483648"));
  EXPECT_EQ(-2147483647 - 1, FXSYS_wtoi(L"-2147483652"));
}

TEST(fxcrt, FXSYS_wtoi64) {
  EXPECT_EQ(0, FXSYS_wtoi64(L""));
  EXPECT_EQ(0, FXSYS_wtoi64(L"0"));
  EXPECT_EQ(-1, FXSYS_wtoi64(L"-1"));
  EXPECT_EQ(2345, FXSYS_wtoi64(L"2345"));
  EXPECT_EQ(-9223372036854775807LL, FXSYS_wtoi64(L"-9223372036854775807"));
  // Handle the sign.
  EXPECT_EQ(-2345, FXSYS_wtoi64(L"-2345"));
  EXPECT_EQ(2345, FXSYS_wtoi64(L"+2345"));
  // The max value.
  EXPECT_EQ(9223372036854775807LL, FXSYS_wtoi64(L"9223372036854775807"));
  // The min value.
  EXPECT_EQ(-9223372036854775807LL - 1LL,
            FXSYS_wtoi64(L"-9223372036854775808"));
  // With invalid char.
  EXPECT_EQ(9, FXSYS_wtoi64(L"9x9"));

  // Out of range values.
  EXPECT_EQ(9223372036854775807LL,
            FXSYS_wtoi64(L"922337203685471234123475807"));
  EXPECT_EQ(9223372036854775807LL, FXSYS_wtoi64(L"9223372036854775808"));
  EXPECT_EQ(-9223372036854775807LL - 1LL,
            FXSYS_wtoi64(L"-9223372036854775810"));
}

TEST(fxcrt, FXSYS_atoui) {
  EXPECT_EQ(0u, FXSYS_atoui(""));
  EXPECT_EQ(0u, FXSYS_atoui("0"));
  EXPECT_EQ(4294967295, FXSYS_atoui("-1"));
  EXPECT_EQ(2345u, FXSYS_atoui("2345"));
  // Handle the sign.
  EXPECT_EQ(4294964951, FXSYS_atoui("-2345"));
  EXPECT_EQ(2345u, FXSYS_atoui("+2345"));
  // The max value.
  EXPECT_EQ(4294967295, FXSYS_atoui("4294967295"));
  EXPECT_EQ(9u, FXSYS_atoui("9x9"));

  // Out of range values.
  EXPECT_EQ(4294967295, FXSYS_atoui("2147483623423412348"));
  EXPECT_EQ(4294967295, FXSYS_atoui("4294967296"));
  EXPECT_EQ(4294967295, FXSYS_atoui("-4294967345"));
}
