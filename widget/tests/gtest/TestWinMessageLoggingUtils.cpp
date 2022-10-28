/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "windows/nsWindowDbg.h"

using namespace mozilla;
using namespace mozilla::widget;

TEST(WinMessageLoggingUtils, AppendFlagsInfo_CombinationFlagsHandledFirst)
{
  const nsTArray<EnumValueAndName> flags = {
      {0x3, "COMBO"}, {0x1, "ONE"}, {0x2, "TWO"}};
  nsAutoCString result;
  EXPECT_EQ(true, AppendFlagsInfo(result, 0x3, flags, nullptr));
  EXPECT_STREQ("COMBO", result.get());
}

TEST(WinMessageLoggingUtils, AppendFlagsInfo_SingleFlag)
{
  const nsTArray<EnumValueAndName> flags = {
      {0x1, "ONE"}, {0x2, "TWO"}, {0x4, "FOUR"}};
  nsAutoCString result;
  EXPECT_EQ(true, AppendFlagsInfo(result, 0x4, flags, nullptr));
  EXPECT_STREQ("FOUR", result.get());
}

TEST(WinMessageLoggingUtils, AppendFlagsInfo_SingleFlagWithName)
{
  const nsTArray<EnumValueAndName> flags = {
      {0x1, "ONE"}, {0x2, "TWO"}, {0x4, "FOUR"}};
  nsAutoCString result;
  EXPECT_EQ(true, AppendFlagsInfo(result, 0x4, flags, "paramName"));
  EXPECT_STREQ("paramName=FOUR", result.get());
}

TEST(WinMessageLoggingUtils, AppendFlagsInfo_MultipleFlags)
{
  const nsTArray<EnumValueAndName> flags = {
      {0x1, "ONE"}, {0x2, "TWO"}, {0x4, "FOUR"}};
  nsAutoCString result;
  EXPECT_EQ(true, AppendFlagsInfo(result, 0x5, flags, nullptr));
  EXPECT_STREQ("ONE|FOUR", result.get());
}

TEST(WinMessageLoggingUtils, AppendFlagsInfo_NoFlags)
{
  const nsTArray<EnumValueAndName> flags = {
      {0x1, "ONE"}, {0x2, "TWO"}, {0x4, "FOUR"}};
  nsAutoCString result;
  EXPECT_EQ(true, AppendFlagsInfo(result, 0x8, flags, nullptr));
  EXPECT_STREQ("Unknown (0x00000008)", result.get());
}

TEST(WinMessageLoggingUtils, AppendFlagsInfo_OnlySomeFlagsMatch)
{
  const nsTArray<EnumValueAndName> flags = {
      {0x1, "ONE"}, {0x2, "TWO"}, {0x4, "FOUR"}};
  nsAutoCString result;
  EXPECT_EQ(true, AppendFlagsInfo(result, 0x9, flags, nullptr));
  EXPECT_STREQ("ONE|Unknown (0x00000008)", result.get());
}

TEST(WinMessageLoggingUtils, AppendFlagsInfo_FlagsMatch_NoZeroReturned)
{
  const nsTArray<EnumValueAndName> flags = {
      {0x1, "ONE"}, {0x2, "TWO"}, {0x4, "FOUR"}, {0x0, "ZERO"}};
  nsAutoCString result;
  EXPECT_EQ(true, AppendFlagsInfo(result, 0x2, flags, nullptr));
  EXPECT_STREQ("TWO", result.get());
}

TEST(WinMessageLoggingUtils, AppendFlagsInfo_NoFlagsMatch_NoZeroReturned)
{
  const nsTArray<EnumValueAndName> flags = {
      {0x1, "ONE"}, {0x2, "TWO"}, {0x4, "FOUR"}, {0x0, "ZERO"}};
  nsAutoCString result;
  EXPECT_EQ(true, AppendFlagsInfo(result, 0x8, flags, nullptr));
  EXPECT_STREQ("Unknown (0x00000008)", result.get());
}

TEST(WinMessageLoggingUtils, AppendFlagsInfo_NameAndNoFlagsMatch_NoZeroReturned)
{
  const nsTArray<EnumValueAndName> flags = {
      {0x1, "ONE"}, {0x2, "TWO"}, {0x4, "FOUR"}, {0x0, "ZERO"}};
  nsAutoCString result;
  EXPECT_EQ(true, AppendFlagsInfo(result, 0x8, flags, "paramName"));
  EXPECT_STREQ("paramName=Unknown (0x00000008)", result.get());
}

TEST(WinMessageLoggingUtils, AppendFlagsInfo_ValueIsZero_ZeroReturned)
{
  const nsTArray<EnumValueAndName> flags = {
      {0x1, "ONE"}, {0x2, "TWO"}, {0x4, "FOUR"}, {0x0, "ZERO"}};
  nsAutoCString result;
  EXPECT_EQ(true, AppendFlagsInfo(result, 0x0, flags, nullptr));
  EXPECT_STREQ("ZERO", result.get());
}
