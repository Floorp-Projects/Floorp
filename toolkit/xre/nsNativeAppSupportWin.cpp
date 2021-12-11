/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeAppSupportBase.h"
#include "nsNativeAppSupportWin.h"

#include "mozilla/WindowsConsole.h"

using namespace mozilla;

/*
 * This code attaches the process to the appropriate console.
 */

class nsNativeAppSupportWin : public nsNativeAppSupportBase {
 public:
  // Utility function to handle a Win32-specific command line
  // option: "--console", which dynamically creates a Windows
  // console.
  void CheckConsole();

 private:
  ~nsNativeAppSupportWin() {}
};  // nsNativeAppSupportWin

void nsNativeAppSupportWin::CheckConsole() {
  for (int i = 1; i < gArgc; ++i) {
    if (strcmp("-console", gArgv[i]) == 0 ||
        strcmp("--console", gArgv[i]) == 0 ||
        strcmp("/console", gArgv[i]) == 0) {
      if (AllocConsole()) {
        // Redirect the standard streams to the new console, but
        // only if they haven't been redirected to a valid file.
        // Visual Studio's _fileno() returns -2 for the standard
        // streams if they aren't associated with an output stream.
        if (_fileno(stdout) == -2) {
          freopen("CONOUT$", "w", stdout);
        }
        // There is no CONERR$, so use CONOUT$ for stderr as well.
        if (_fileno(stderr) == -2) {
          freopen("CONOUT$", "w", stderr);
        }
        if (_fileno(stdin) == -2) {
          freopen("CONIN$", "r", stdin);
        }
      }
    } else if (strcmp("-attach-console", gArgv[i]) == 0 ||
               strcmp("--attach-console", gArgv[i]) == 0 ||
               strcmp("/attach-console", gArgv[i]) == 0) {
      UseParentConsole();
    }
  }
}

// Create and return an instance of class nsNativeAppSupportWin.
nsresult NS_CreateNativeAppSupport(nsINativeAppSupport** aResult) {
  nsNativeAppSupportWin* pNative = new nsNativeAppSupportWin;
  if (!pNative) return NS_ERROR_OUT_OF_MEMORY;

  // Check for dynamic console creation request.
  pNative->CheckConsole();

  *aResult = pNative;
  NS_ADDREF(*aResult);

  return NS_OK;
}
