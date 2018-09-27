/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <locale.h>

#include "gtest/gtest.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

TEST(PrefsBasics, Errors)
{
  Preferences::SetBool("foo.bool", true, PrefValueKind::Default);
  Preferences::SetBool("foo.bool", false, PrefValueKind::User);
  ASSERT_EQ(Preferences::GetBool("foo.bool", false, PrefValueKind::Default),
            true);
  ASSERT_EQ(Preferences::GetBool("foo.bool", true, PrefValueKind::User), false);

  Preferences::SetInt("foo.int", -66, PrefValueKind::Default);
  Preferences::SetInt("foo.int", -77, PrefValueKind::User);
  ASSERT_EQ(Preferences::GetInt("foo.int", 1, PrefValueKind::Default), -66);
  ASSERT_EQ(Preferences::GetInt("foo.int", 1, PrefValueKind::User), -77);

  Preferences::SetUint("foo.uint", 88, PrefValueKind::Default);
  Preferences::SetUint("foo.uint", 99, PrefValueKind::User);
  ASSERT_EQ(Preferences::GetUint("foo.uint", 1, PrefValueKind::Default), 88U);
  ASSERT_EQ(Preferences::GetUint("foo.uint", 1, PrefValueKind::User), 99U);

  Preferences::SetFloat("foo.float", 3.33f, PrefValueKind::Default);
  Preferences::SetFloat("foo.float", 4.44f, PrefValueKind::User);
  ASSERT_FLOAT_EQ(
    Preferences::GetFloat("foo.float", 1.0f, PrefValueKind::Default), 3.33f);
  ASSERT_FLOAT_EQ(Preferences::GetFloat("foo.float", 1.0f, PrefValueKind::User),
                  4.44f);
}

TEST(PrefsBasics, FloatConversions)
{
  // Set a global locale that uses the comma as the decimal separator. Since
  // we can't tell which locales will be available on a machine the tests are
  // executed only if the locale was set correctly.
  const char* oldLocale = setlocale(LC_NUMERIC, "nl_NL");
  if (oldLocale != nullptr) {
    Preferences::SetFloat("foo.float", 3.33f, PrefValueKind::Default);
    Preferences::SetFloat("foo.float", 4.44f, PrefValueKind::User);
    ASSERT_FLOAT_EQ(
      Preferences::GetFloat("foo.float", 1.0f, PrefValueKind::Default), 3.33f);
    ASSERT_FLOAT_EQ(
      Preferences::GetFloat("foo.float", 1.0f, PrefValueKind::User), 4.44f);

    // Restore the original locale
    setlocale(LC_NUMERIC, oldLocale);
  }
}
