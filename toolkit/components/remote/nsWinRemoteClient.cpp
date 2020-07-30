/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWinRemoteClient.h"
#include <windows.h>
#include "RemoteUtils.h"
#include "WinRemoteMessage.h"

using namespace mozilla;

nsresult nsWinRemoteClient::Init() { return NS_OK; }

nsresult nsWinRemoteClient::SendCommandLine(
    const char* aProgram, const char* aProfile, int32_t argc, char** argv,
    const char* aDesktopStartupID, char** aResponse, bool* aSucceeded) {
  *aSucceeded = false;

  nsString className;
  BuildClassName(aProgram, aProfile, className);

  HWND handle = ::FindWindowW(className.get(), 0);

  if (!handle) {
    return NS_OK;
  }

  WCHAR cwd[MAX_PATH];
  _wgetcwd(cwd, MAX_PATH);
  WinRemoteMessageSender sender(::GetCommandLineW(), cwd);

  // Bring the already running Mozilla process to the foreground.
  // nsWindow will restore the window (if minimized) and raise it.
  ::SetForegroundWindow(handle);
  ::SendMessage(handle, WM_COPYDATA, 0,
                reinterpret_cast<LPARAM>(sender.CopyData()));

  *aSucceeded = true;

  return NS_OK;
}
