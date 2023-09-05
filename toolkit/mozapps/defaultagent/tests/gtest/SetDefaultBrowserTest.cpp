/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include <windows.h>
#include "mozilla/UniquePtr.h"
#include "WindowsUserChoice.h"

#include "SetDefaultBrowser.h"

using namespace mozilla::default_agent;

TEST(SetDefaultBrowserUserChoice, Hash)
{
  // Hashes set by System Settings on 64-bit Windows 10 Pro 20H2 (19042.928).
  const wchar_t* sid = L"S-1-5-21-636376821-3290315252-1794850287-1001";

  // length mod 8 = 0
  EXPECT_STREQ(
      GenerateUserChoiceHash(L"https", sid, L"FirefoxURL-308046B0AF4A39CB",
                             (SYSTEMTIME){2021, 4, 1, 19, 23, 7, 56, 506})
          .get(),
      L"uzpIsMVyZ1g=");

  // length mod 8 = 2 (confirm that the incomplete last block is dropped)
  EXPECT_STREQ(
      GenerateUserChoiceHash(L".html", sid, L"FirefoxHTML-308046B0AF4A39CB",
                             (SYSTEMTIME){2021, 4, 1, 19, 23, 7, 56, 519})
          .get(),
      L"7fjRtUPASlc=");

  // length mod 8 = 4
  EXPECT_STREQ(
      GenerateUserChoiceHash(L"https", sid, L"MSEdgeHTM",
                             (SYSTEMTIME){2021, 4, 1, 19, 23, 3, 48, 119})
          .get(),
      L"Fz0kA3Ymmps=");

  // length mod 8 = 6
  EXPECT_STREQ(
      GenerateUserChoiceHash(L".html", sid, L"ChromeHTML",
                             (SYSTEMTIME){2021, 4, 1, 19, 23, 6, 3, 628})
          .get(),
      L"R5TD9LGJ5Xw=");

  // non-ASCII
  EXPECT_STREQ(
      GenerateUserChoiceHash(L".html", sid, L"FirefoxHTML-ÀBÇDË😀†",
                             (SYSTEMTIME){2021, 4, 2, 20, 0, 38, 55, 101})
          .get(),
      L"F3NsK3uNv5E=");
}
