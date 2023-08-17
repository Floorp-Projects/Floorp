/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __WinRemoteMessage_h__
#define __WinRemoteMessage_h__

#include <windows.h>

#include "nsICommandLineRunner.h"
#include "nsCOMPtr.h"
#include "nsString.h"

// This version number, stored in COPYDATASTRUCT::dwData, defines the format of
// COPYDATASTRUCT::lpData in a WM_COPYDATA message.
//
// Previous versions are documented here, but are no longer produced or
// accepted.
//   * Despite being documented as "UTF-8", older versions could not properly
//     handle non-ASCII characters (bug 1650637).
//   * Backwards-compatibly supporting v0 led to other issues: some ill-behaved
//     applications may broadcast WM_COPYDATA messages to all and sundry, and a
//     version-0 message whose payload happened to start with a NUL byte was
//     treated as valid (bug 1847458).
//
// Whenever possible, we should retain backwards compatibility for currently-
// supported ESR versions. (Any future versions should probably contain a magic
// cookie of some sort to reduce the chances of variants of bug 1847458.)
enum class WinRemoteMessageVersion : uint32_t {
  // "<CommandLine>\0" in utf8
  /* CommandLineOnly = 0, */

  // "<CommandLine>\0<WorkingDir>\0" in utf8
  /* CommandLineAndWorkingDir = 1, */

  // L"<CommandLine>\0<WorkingDir>\0" in utf16
  CommandLineAndWorkingDirInUtf16 = 2,
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
};

class WinRemoteMessageReceiver final {
  nsCOMPtr<nsICommandLineRunner> mCommandLine;

  nsresult ParseV2(const nsAString& aBuffer);

 public:
  WinRemoteMessageReceiver() = default;
  WinRemoteMessageReceiver(const WinRemoteMessageReceiver&) = delete;
  WinRemoteMessageReceiver(WinRemoteMessageReceiver&&) = delete;
  WinRemoteMessageReceiver& operator=(const WinRemoteMessageReceiver&) = delete;
  WinRemoteMessageReceiver& operator=(WinRemoteMessageReceiver&&) = delete;

  nsresult Parse(const COPYDATASTRUCT* aMessageData);
  nsICommandLineRunner* CommandLineRunner();
};

#endif  // __WinRemoteMessage_h__
