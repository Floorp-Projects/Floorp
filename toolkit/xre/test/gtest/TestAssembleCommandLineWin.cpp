/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/AssembleCmdLine.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/UniquePtrExtensions.h"
#include "WinRemoteMessage.h"

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
#define UPPER_CYRILLIC_P_IN_UTF8 "\xd0\xa0"
#define LOWER_CYRILLIC_P_IN_UTF8 "\xd1\x80"
#define UPPER_CYRILLIC_P_IN_UTF16 L"\u0420"
#define LOWER_CYRILLIC_P_IN_UTF16 L"\u0440"

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

    // Empty string cases
    {{"", nullptr}, L"\"\""},
    {{"foo", "", nullptr}, L"foo \"\""},
    {{"", "bar", nullptr}, L"\"\" bar"},
    {{"foo", "", "bar", nullptr}, L"foo \"\" bar"},
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

TEST(CommandLineParserWin, HandleCommandLine)
{
  CommandLineParserWin<char> parser;
  for (const auto& testCase : testCases) {
    NS_ConvertUTF16toUTF8 utf8(testCase.mExpected);
    parser.HandleCommandLine(utf8);

    if (utf8.Length() == 0) {
      EXPECT_EQ(parser.Argc(), 0);
      continue;
    }

    for (int i = 0; i < parser.Argc(); ++i) {
      EXPECT_NE(testCase.mArgs[i], nullptr);
      EXPECT_STREQ(parser.Argv()[i], testCase.mArgs[i]);
    }
    EXPECT_EQ(testCase.mArgs[parser.Argc()], nullptr);
  }
}

TEST(WinRemoteMessage, SendReceive)
{
  const wchar_t kCommandlineW[] =
      L"dummy.exe /arg1 --arg2 \"3rd arg\" "
      L"4th=\"" UPPER_CYRILLIC_P_IN_UTF16 L" " LOWER_CYRILLIC_P_IN_UTF16 L"\"";
  const wchar_t* kExpectedArgsW[] = {
      L"-arg1", L"-arg2", L"3rd arg",
      L"4th=" UPPER_CYRILLIC_P_IN_UTF16 L" " LOWER_CYRILLIC_P_IN_UTF16};

  wchar_t workingDirW[MAX_PATH];
  EXPECT_NE(_wgetcwd(workingDirW, MAX_PATH), nullptr);

  WinRemoteMessageSender v2(kCommandlineW, workingDirW);

  WinRemoteMessageReceiver receiver;
  int32_t len;
  nsAutoString arg;
  nsCOMPtr<nsIFile> workingDir;

  receiver.Parse(v2.CopyData());
  EXPECT_NS_SUCCEEDED(receiver.CommandLineRunner()->GetLength(&len));
  EXPECT_EQ(static_cast<size_t>(len), ArrayLength(kExpectedArgsW));
  for (size_t i = 0; i < ArrayLength(kExpectedArgsW); ++i) {
    EXPECT_TRUE(
        NS_SUCCEEDED(receiver.CommandLineRunner()->GetArgument(i, arg)));
    EXPECT_STREQ(arg.get(), kExpectedArgsW[i]);
  }
  EXPECT_TRUE(NS_SUCCEEDED(receiver.CommandLineRunner()->GetWorkingDirectory(
      getter_AddRefs(workingDir))));
  EXPECT_NS_SUCCEEDED(workingDir->GetPath(arg));
  EXPECT_STREQ(arg.get(), workingDirW);
}

TEST(WinRemoteMessage, NonNullTerminatedBuffer)
{
  // Reserve two pages and commit the first one
  const uint32_t kPageSize = 4096;
  UniquePtr<void, VirtualFreeDeleter> pages(
      ::VirtualAlloc(nullptr, kPageSize * 2, MEM_RESERVE, PAGE_NOACCESS));
  EXPECT_TRUE(pages);
  EXPECT_TRUE(
      ::VirtualAlloc(pages.get(), kPageSize, MEM_COMMIT, PAGE_READWRITE));

  // Test strings with lengths between 0 and |kMaxBufferSize| bytes
  const int kMaxBufferSize = 10;

  // Set a string just before the boundary between the two pages.
  uint8_t* bufferEnd = reinterpret_cast<uint8_t*>(pages.get()) + kPageSize;
  memset(bufferEnd - kMaxBufferSize, '$', kMaxBufferSize);

  nsCOMPtr<nsIFile> workingDir;
  COPYDATASTRUCT copyData = {};
  for (int i = 0; i < kMaxBufferSize; ++i) {
    WinRemoteMessageReceiver receiver;

    copyData.cbData = i;
    copyData.lpData = bufferEnd - i;

    copyData.dwData = static_cast<ULONG_PTR>(
        WinRemoteMessageVersion::CommandLineAndWorkingDirInUtf16);
    EXPECT_NS_SUCCEEDED(receiver.Parse(&copyData));
    EXPECT_EQ(receiver.CommandLineRunner()->GetWorkingDirectory(
                  getter_AddRefs(workingDir)),
              NS_ERROR_NOT_INITIALIZED);
  }
}
