/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsWindowsPackageManager.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::toolkit::system;

TEST(WindowsPackageManager, TestWithMatches)
{
  nsCOMPtr<nsIWindowsPackageManager> wpm(
      do_GetService("@mozilla.org/windows-package-manager;1"));
  nsTArray<nsString> prefixes, packages;
  // We're assuming that there will always be at least _one_ Microsoft package
  // installed when we run tests. This will _probably_ hold true.
  prefixes.AppendElement(u"Microsoft"_ns);
  nsresult retVal = wpm->FindUserInstalledPackages(prefixes, packages);
  ASSERT_GT(packages.Length(), 0U);
  ASSERT_EQ(NS_OK, retVal);
}

TEST(WindowsPackageManager, TestWithoutMatches)
{
  nsCOMPtr<nsIWindowsPackageManager> wpm(
      do_GetService("@mozilla.org/windows-package-manager;1"));
  nsTArray<nsString> prefixes, packages;
  prefixes.AppendElement(u"DoesntExist"_ns);
  nsresult retVal = wpm->FindUserInstalledPackages(prefixes, packages);
  ASSERT_EQ(packages.Length(), 0U);
  ASSERT_EQ(NS_OK, retVal);
}
