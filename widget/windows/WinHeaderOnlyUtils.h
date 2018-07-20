/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WinHeaderOnlyUtils_h
#define mozilla_WinHeaderOnlyUtils_h

#include <windows.h>

/**
 * This header is intended for self-contained, header-only, utility code for
 * Win32. It may be used outside of xul.dll, in places such as firefox.exe or
 * mozglue.dll. If your code creates dependencies on Mozilla libraries, you
 * should put it elsewhere.
 */

namespace mozilla {

// How long to wait for a created process to become available for input,
// to prevent that process's windows being forced to the background.
// This is used across update, restart, and the launcher.
const DWORD kWaitForInputIdleTimeoutMS = 10*1000;

/**
 * Wait for a child GUI process to become "idle." Idle means that the process
 * has created its message queue and has begun waiting for user input.
 *
 * Note that this must only be used when the child process is going to display
 * GUI! Otherwise you're going to be waiting for a very long time ;-)
 *
 * @return true if we successfully waited for input idle;
 *         false if we timed out or failed to wait.
 */
inline bool
WaitForInputIdle(HANDLE aProcess, DWORD aTimeoutMs = kWaitForInputIdleTimeoutMS)
{
  const DWORD kSleepTimeMs = 10;
  const DWORD waitStart = aTimeoutMs == INFINITE ? 0 : ::GetTickCount();
  DWORD elapsed = 0;

  while (true) {
    if (aTimeoutMs != INFINITE) {
      elapsed = ::GetTickCount() - waitStart;
    }

    if (elapsed >= aTimeoutMs) {
      return false;
    }

    DWORD waitResult = ::WaitForInputIdle(aProcess, aTimeoutMs - elapsed);
    if (!waitResult) {
      return true;
    }

    if (waitResult == WAIT_FAILED && ::GetLastError() == ERROR_NOT_GUI_PROCESS) {
      ::Sleep(kSleepTimeMs);
      continue;
    }

    return false;
  }
}

} // namespace mozilla

#endif // mozilla_WinHeaderOnlyUtils_h
