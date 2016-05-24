/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "gtest/gtest.h"

static void
nsXPIDLStringTest_Value(char16_t** aResult)
{
  *aResult = ToNewUnicode(NS_LITERAL_STRING("Hello, World"));
}

TEST(XPIDLString, Main)
{
  nsXPIDLString s1;
  nsXPIDLStringTest_Value(getter_Copies(s1));
  EXPECT_TRUE(s1.EqualsLiteral("Hello, World"));
}

