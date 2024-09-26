/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "nsWindowsDllInterceptor.h"
#include "nsWindowsHelpers.h"

#include <string>

using std::wstring;

extern "C" MOZ_NEVER_INLINE MOZ_NOPROFILE MOZ_NOINSTRUMENT
    __declspec(dllexport) int
    ReturnResult() {
  return 2;
}

static mozilla::CrossProcessDllInterceptor::FuncHookType<
    decltype(&ReturnResult)>
    gOrigReturnResult;

static int ReturnResultHook() {
  if (gOrigReturnResult() != 2) {
    return 3;
  }

  return 0;
}

int ParentMain(int argc, wchar_t* argv[]) {
  mozilla::SetArgv0ToFullBinaryPath(argv);

  // We'll add the child process to a job so that, in the event of a failure in
  // this parent process, the child process will be automatically terminated.
  nsAutoHandle job(::CreateJobObjectW(nullptr, nullptr));
  if (!job) {
    printf(
        "TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Job creation "
        "failed\n");
    return 1;
  }

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
  jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

  if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                 &jobInfo, sizeof(jobInfo))) {
    printf(
        "TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Job config "
        "failed\n");
    return 1;
  }

  wchar_t childArgv_1[] = L"-child";

  wchar_t* childArgv[] = {argv[0], childArgv_1};

  mozilla::UniquePtr<wchar_t[]> cmdLine(
      mozilla::MakeCommandLine(mozilla::ArrayLength(childArgv), childArgv));

  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi;
  if (!::CreateProcessW(argv[0], cmdLine.get(), nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
    printf(
        "TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to spawn "
        "child process\n");
    return 1;
  }

  nsAutoHandle childProcess(pi.hProcess);
  nsAutoHandle childMainThread(pi.hThread);

  if (!::AssignProcessToJobObject(job.get(), childProcess.get())) {
    printf(
        "TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to assign "
        "child process to job\n");
    ::TerminateProcess(childProcess.get(), 1);
    return 1;
  }

  mozilla::nt::CrossExecTransferManager transferMgr(childProcess);
  if (!transferMgr) {
    printf(
        "TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | "
        "CrossExecTransferManager instantiation failed.\n");
    return 1;
  }

  mozilla::CrossProcessDllInterceptor intcpt(childProcess.get());
  intcpt.Init("TestDllInterceptorCrossProcess.exe");

  if (!gOrigReturnResult.Set(transferMgr, intcpt, "ReturnResult",
                             &ReturnResultHook)) {
    printf(
        "TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to add "
        "hook\n");
    return 1;
  }

  printf("TEST-PASS | DllInterceptorCrossProcess | Hook added\n");

  if (::ResumeThread(childMainThread.get()) == static_cast<DWORD>(-1)) {
    printf(
        "TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to resume "
        "child thread\n");
    return 1;
  }

  BOOL remoteDebugging;
  bool debugging =
      ::IsDebuggerPresent() ||
      (::CheckRemoteDebuggerPresent(childProcess.get(), &remoteDebugging) &&
       remoteDebugging);

  DWORD waitResult =
      ::WaitForSingleObject(childProcess.get(), debugging ? INFINITE : 60000);
  if (waitResult != WAIT_OBJECT_0) {
    printf(
        "TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Child process "
        "failed to finish\n");
    return 1;
  }

  DWORD childExitCode;
  if (!::GetExitCodeProcess(childProcess.get(), &childExitCode)) {
    printf(
        "TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to obtain "
        "child process exit code\n");
    return 1;
  }

  if (childExitCode) {
    printf(
        "TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Child process "
        "exit code is %lu instead of 0\n",
        childExitCode);
    return 1;
  }

  printf(
      "TEST-PASS | DllInterceptorCrossProcess | Child process exit code is "
      "zero\n");
  return 0;
}

extern "C" int wmain(int argc, wchar_t* argv[]) {
  if (argc > 1) {
    // clang keeps inlining this call despite every attempt to force it to do
    // otherwise. We'll use GetProcAddress and call its function pointer
    // instead.
    auto pReturnResult = reinterpret_cast<decltype(&ReturnResult)>(
        ::GetProcAddress(::GetModuleHandleW(nullptr), "ReturnResult"));
    return pReturnResult();
  }

  return ParentMain(argc, argv);
}
