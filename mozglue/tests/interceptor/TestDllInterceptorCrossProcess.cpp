/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "nsWindowsDllInterceptor.h"
#include "nsWindowsHelpers.h"

#include <string>

using std::wstring;

static void* gOrigReturnResult;

extern "C" __declspec(dllexport) int
ReturnResult()
{
  return 2;
}

static int
ReturnResultHook()
{
  auto origFn = reinterpret_cast<decltype(&ReturnResult)>(gOrigReturnResult);
  if (origFn() != 2) {
    return 3;
  }

  return 0;
}

int ParentMain()
{
  // We'll add the child process to a job so that, in the event of a failure in
  // this parent process, the child process will be automatically terminated.
  nsAutoHandle job(::CreateJobObject(nullptr, nullptr));
  if (!job) {
    printf("TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Job creation failed\n");
    return 1;
  }

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo{};
  jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

  if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                 &jobInfo, sizeof(jobInfo))) {
    printf("TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Job config failed\n");
    return 1;
  }

  wstring cmdLine(::GetCommandLineW());
  cmdLine += L" -child";

  STARTUPINFOW si = { sizeof(si) };
  PROCESS_INFORMATION pi;
  if (!::CreateProcessW(nullptr, const_cast<LPWSTR>(cmdLine.c_str()), nullptr,
                        nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si,
                        &pi)) {
    printf("TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to spawn child process\n");
    return 1;
  }

  nsAutoHandle childProcess(pi.hProcess);
  nsAutoHandle childMainThread(pi.hThread);

  if (!::AssignProcessToJobObject(job.get(), childProcess.get())) {
    printf("TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to assign child process to job\n");
    ::TerminateProcess(childProcess.get(), 1);
    return 1;
  }

  mozilla::CrossProcessDllInterceptor intcpt(childProcess.get());
  intcpt.Init("TestDllInterceptorCrossProcess.exe");

  if (!intcpt.AddHook("ReturnResult",
                      reinterpret_cast<intptr_t>(&ReturnResultHook),
                      &gOrigReturnResult)) {
    printf("TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to add hook\n");
    return 1;
  }

  printf("TEST-PASS | DllInterceptorCrossProcess | Hook added\n");

  // Let's save the original hook
  SIZE_T bytesWritten;
  if (!::WriteProcessMemory(childProcess.get(), &gOrigReturnResult,
                            &gOrigReturnResult, sizeof(gOrigReturnResult),
                            &bytesWritten)) {
    printf("TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to write original function pointer\n");
    return 1;
  }

  if (::ResumeThread(childMainThread.get()) == static_cast<DWORD>(-1)) {
    printf("TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to resume child thread\n");
    return 1;
  }

  BOOL remoteDebugging;
  bool debugging = ::IsDebuggerPresent() ||
                   (::CheckRemoteDebuggerPresent(childProcess.get(),
                                                 &remoteDebugging) &&
                    remoteDebugging);

  DWORD waitResult = ::WaitForSingleObject(childProcess.get(),
                                           debugging ? INFINITE : 60000);
  if (waitResult != WAIT_OBJECT_0) {
    printf("TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Child process failed to finish\n");
    return 1;
  }

  DWORD childExitCode;
  if (!::GetExitCodeProcess(childProcess.get(), &childExitCode)) {
    printf("TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Failed to obtain child process exit code\n");
    return 1;
  }

  if (childExitCode) {
    printf("TEST-UNEXPECTED-FAIL | DllInterceptorCrossProcess | Child process exit code is %u instead of 0\n", childExitCode);
    return 1;
  }

  printf("TEST-PASS | DllInterceptorCrossProcess | Child process exit code is zero\n");
  return 0;
}

int main(int argc, char* argv[])
{
  if (argc > 1) {
    return ReturnResult();
  }

  return ParentMain();
}

