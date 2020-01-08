/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/TimeStamp.h"

#include "nsWindowsHelpers.h"

#include <stdio.h>
#include <windows.h>

static char kChildArg[] = "--child";

static nsReturnRef<HANDLE> CreateProcessWrapper(const char* aPath) {
  nsAutoHandle empty;

  STARTUPINFOA si = {sizeof(si)};
  PROCESS_INFORMATION pi;
  BOOL ok = ::CreateProcessA(aPath, kChildArg, nullptr, nullptr, FALSE, 0,
                             nullptr, nullptr, &si, &pi);
  if (!ok) {
    printf(
        "TEST-FAILED | TimeStampWin | "
        "CreateProcess failed - %08lx\n",
        GetLastError());
    return empty.out();
  }

  nsAutoHandle proc(pi.hProcess);
  nsAutoHandle thd(pi.hThread);

  return proc.out();
}

int ChildMain() {
  // Make sure a process creation timestamp is always not bigger than
  // the current timestamp.
  bool inconsistent = false;
  auto t0 = mozilla::TimeStamp::ProcessCreation(&inconsistent);
  auto t1 = mozilla::TimeStamp::Now();
  if (t0 > t1) {
    printf(
        "TEST-FAILED | TimeStampWin | "
        "Process creation timestamp is bigger than the current "
        "timestamp!\n");
    return 1;
  }
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc != 1) {
    printf(
        "TEST-FAILED | TimeStampWin | "
        "Unexpected argc\n");
    return 1;
  }

  if (strcmp(argv[0], kChildArg) == 0) {
    return ChildMain();
  }

  // Start a child process successively, checking any of them terminates with
  // a non-zero value which means an error.
  for (int i = 0; i < 20; ++i) {
    nsAutoHandle childProc(CreateProcessWrapper(argv[0]));

    if (::WaitForSingleObject(childProc, 60000) != WAIT_OBJECT_0) {
      printf(
          "TEST-FAILED | TimeStampWin | "
          "Unexpected result from WaitForSingleObject\n");
      return 1;
    }

    DWORD childExitCode;
    if (!::GetExitCodeProcess(childProc.get(), &childExitCode)) {
      printf(
          "TEST-FAILED | TimeStampWin | "
          "GetExitCodeProcess failed - %08lx\n",
          GetLastError());
      return 1;
    }

    if (childExitCode != 0) {
      return childExitCode;
    }
  }

  return 0;
}
