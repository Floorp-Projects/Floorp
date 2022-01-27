/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/TimeStamp.h"

#include "nsWindowsHelpers.h"

#include <stdio.h>
#include <windows.h>

static wchar_t kChildArg[] = L"--child";

static nsReturnRef<HANDLE> CreateProcessWrapper(const wchar_t* aPath) {
  nsAutoHandle empty;

  const wchar_t* childArgv[] = {aPath, kChildArg};
  mozilla::UniquePtr<wchar_t[]> cmdLine(
      mozilla::MakeCommandLine(mozilla::ArrayLength(childArgv), childArgv));

  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi;
  BOOL ok = ::CreateProcessW(aPath, cmdLine.get(), nullptr, nullptr, FALSE, 0,
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
  auto t0 = mozilla::TimeStamp::ProcessCreation();
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

int wmain(int argc, wchar_t* argv[]) {
  if (argc == 2 && wcscmp(argv[1], kChildArg) == 0) {
    return ChildMain();
  }

  if (argc != 1) {
    printf(
        "TEST-FAILED | TimeStampWin | "
        "Unexpected argc\n");
    return 1;
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
