/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowsConsole.h"

#include <windows.h>
#include <fcntl.h>
#include <cstdio>
#include <io.h>

namespace mozilla {

static void AssignStdHandle(const char* aPath, const char* aMode, FILE* aStream,
                            DWORD aStdHandle) {
  // Visual Studio's _fileno() returns -2 for the standard
  // streams if they aren't associated with an output stream.
  const int fd = _fileno(aStream);
  if (fd == -2) {
    freopen(aPath, aMode, aStream);
    return;
  }
  if (fd < 0) {
    return;
  }

  const HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  if (handle == INVALID_HANDLE_VALUE) {
    return;
  }

  const HANDLE oldHandle = GetStdHandle(aStdHandle);
  if (handle == oldHandle) {
    return;
  }

  SetStdHandle(aStdHandle, handle);
}

// This code attaches the process to the appropriate console.
void UseParentConsole() {
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    // Redirect the standard streams to the existing console, but
    // only if they haven't been redirected to a valid file.
    AssignStdHandle("CONOUT$", "w", stdout, STD_OUTPUT_HANDLE);
    // There is no CONERR$, so use CONOUT$ for stderr as well.
    AssignStdHandle("CONOUT$", "w", stderr, STD_ERROR_HANDLE);
    AssignStdHandle("CONIN$", "r", stdin, STD_INPUT_HANDLE);
  }
}

}  // namespace mozilla
