/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/AssembleCmdLine.h"
#include "mozilla/UniquePtrExtensions.h"

using namespace mozilla;

template <typename T>
struct TestCase {
  const T* mArgs[4];
  const wchar_t* mExpected;
};

#define ALPHA_IN_UTF8 "\xe3\x82\xa2\xe3\x83\xab\xe3\x83\x95\xe3\x82\xa1"
#define OMEGA_IN_UTF8 "\xe3\x82\xaa\xe3\x83\xa1\xe3\x82\xac"
#define ALPHA_IN_UTF16 L"\u30A2\u30EB\u30D5\u30A1"
#define OMEGA_IN_UTF16 L"\u30AA\u30E1\u30AC"

TestCase<char> testCases[] = {
    // Copied from TestXREMakeCommandLineWin.ini
    {{"a:\\", nullptr}, L"a:\\"},
    {{"a:\"", nullptr}, L"a:\\\""},
    {{"a:\\b c", nullptr}, L"\"a:\\b c\""},
    {{"a:\\b c\"", nullptr}, L"\"a:\\b c\\\"\""},
    {{"a:\\b c\\d e", nullptr}, L"\"a:\\b c\\d e\""},
    {{"a:\\b c\\d e\"", nullptr}, L"\"a:\\b c\\d e\\\"\""},
    {{"a:\\", nullptr}, L"a:\\"},
    {{"a:\"", "b:\\c d", nullptr}, L"a:\\\" \"b:\\c d\""},
    {{"a", "b:\" c:\\d", "e", nullptr}, L"a \"b:\\\" c:\\d\" e"},
    {{"abc", "d", "e", nullptr}, L"abc d e"},
    {{"a b c", "d", "e", nullptr}, L"\"a b c\" d e"},
    {{"a\\\\\\b", "de fg", "h", nullptr}, L"a\\\\\\b \"de fg\" h"},
    {{"a", "b", nullptr}, L"a b"},
    {{"a\tb", nullptr}, L"\"a\tb\""},
    {{"a\\\"b", "c", "d", nullptr}, L"a\\\\\\\"b c d"},
    {{"a\\\"b", "c", nullptr}, L"a\\\\\\\"b c"},
    {{"a\\\\\\b c", nullptr}, L"\"a\\\\\\b c\""},
    {{"\"a", nullptr}, L"\\\"a"},
    {{"\\a", nullptr}, L"\\a"},
    {{"\\\\\\a", nullptr}, L"\\\\\\a"},
    {{"\\\\\\\"a", nullptr}, L"\\\\\\\\\\\\\\\"a"},
    {{"a\\\"b c\" d e", nullptr}, L"\"a\\\\\\\"b c\\\" d e\""},
    {{"a\\\\\"b", "c d e", nullptr}, L"a\\\\\\\\\\\"b \"c d e\""},
    {{"a:\\b", "c\\" ALPHA_IN_UTF8, OMEGA_IN_UTF8 "\\d", nullptr},
     L"a:\\b c\\" ALPHA_IN_UTF16 L" " OMEGA_IN_UTF16 L"\\d"},
    {{"a:\\b", "c\\" ALPHA_IN_UTF8 " " OMEGA_IN_UTF8 "\\d", nullptr},
     L"a:\\b \"c\\" ALPHA_IN_UTF16 L" " OMEGA_IN_UTF16 L"\\d\""},
    {{ALPHA_IN_UTF8, OMEGA_IN_UTF8, nullptr},
     ALPHA_IN_UTF16 L" " OMEGA_IN_UTF16},

    // More single-argument cases
    {{"", nullptr}, L""},
    {{"a\fb", nullptr}, L"\"a\fb\""},
    {{"a\nb", nullptr}, L"\"a\nb\""},
    {{"a\rb", nullptr}, L"\"a\rb\""},
    {{"a\vb", nullptr}, L"\"a\vb\""},
    {{"\"a\" \"b\"", nullptr}, L"\"\\\"a\\\" \\\"b\\\"\""},
    {{"\"a\\b\" \"c\\d\"", nullptr}, L"\"\\\"a\\b\\\" \\\"c\\d\\\"\""},
    {{"\\\\ \\\\", nullptr}, L"\"\\\\ \\\\\\\\\""},
    {{"\"\" \"\"", nullptr}, L"\"\\\"\\\" \\\"\\\"\""},
    {{ALPHA_IN_UTF8 "\\" OMEGA_IN_UTF8, nullptr},
     ALPHA_IN_UTF16 L"\\" OMEGA_IN_UTF16},
    {{ALPHA_IN_UTF8 " " OMEGA_IN_UTF8, nullptr},
     L"\"" ALPHA_IN_UTF16 L" " OMEGA_IN_UTF16 L"\""},
};

TEST(AssembleCommandLineWin, assembleCmdLine)
{
  for (const auto& testCase : testCases) {
    UniqueFreePtr<wchar_t> assembled;
    wchar_t* assembledRaw = nullptr;
    EXPECT_EQ(assembleCmdLine(testCase.mArgs, &assembledRaw, CP_UTF8), 0);
    assembled.reset(assembledRaw);

    EXPECT_STREQ(assembled.get(), testCase.mExpected);
  }
}
