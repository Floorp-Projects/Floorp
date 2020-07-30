/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __WinRemoteMessage_h__
#define __WinRemoteMessage_h__

#include <windows.h>

#include "nsICommandLineRunner.h"
#include "nsCOMPtr.h"

// This version defines the format of COPYDATASTRUCT::lpData in a message of
// WM_COPYDATA.
// Always use the latest version for production use because the older versions
// have a bug that a non-ascii character in a utf-8 message cannot be parsed
// correctly (bug 1650637).  We keep the older versions for backward
// compatibility and the testing purpose only.
enum class WinRemoteMessageVersion : uint32_t {
  // "<CommandLine>\0" in utf8
  CommandLineOnly = 0,
  // "<CommandLine>\0<WorkingDir>\0" in utf8
  CommandLineAndWorkingDir,
  // L"<CommandLine>\0<WorkingDir>\0" in utf16
  CommandLineAndWorkingDirInUtf16,
};

class WinRemoteMessageSender final {
  COPYDATASTRUCT mData;
  nsAutoString mUtf16Buffer;
  nsAutoCString mUtf8Buffer;

 public:
  WinRemoteMessageSender(const wchar_t* aCommandLine,
                         const wchar_t* aWorkingDir);

  WinRemoteMessageSender(const WinRemoteMessageSender&) = delete;
  WinRemoteMessageSender(WinRemoteMessageSender&&) = delete;
  WinRemoteMessageSender& operator=(const WinRemoteMessageSender&) = delete;
  WinRemoteMessageSender& operator=(WinRemoteMessageSender&&) = delete;

  COPYDATASTRUCT* CopyData();

  // Constructors for the old formats.  Testing purpose only.
  explicit WinRemoteMessageSender(const char* aCommandLine);
  WinRemoteMessageSender(const char* aCommandLine, const char* aWorkingDir);
};

class WinRemoteMessageReceiver final {
  nsCOMPtr<nsICommandLineRunner> mCommandLine;

  nsresult ParseV0(char* aBuffer);
  nsresult ParseV1(char* aBuffer);
  nsresult ParseV2(char16_t* aBuffer);

 public:
  WinRemoteMessageReceiver() = default;
  WinRemoteMessageReceiver(const WinRemoteMessageReceiver&) = delete;
  WinRemoteMessageReceiver(WinRemoteMessageReceiver&&) = delete;
  WinRemoteMessageReceiver& operator=(const WinRemoteMessageReceiver&) = delete;
  WinRemoteMessageReceiver& operator=(WinRemoteMessageReceiver&&) = delete;

  nsresult Parse(COPYDATASTRUCT* aMessageData);
  nsICommandLineRunner* CommandLineRunner();
};

#endif  // __WinRemoteMessage_h__
