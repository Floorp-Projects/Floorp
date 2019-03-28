/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowsConsole.h"

#include <windows.h>
#include <fcntl.h>

namespace mozilla {

// This code attaches the process to the appropriate console.
void UseParentConsole() {
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    // Redirect the standard streams to the existing console, but
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
}

}  // namespace mozilla
