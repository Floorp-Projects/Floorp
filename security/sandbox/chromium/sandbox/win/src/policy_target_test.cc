// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

#define BINDNTDLL(name) \
    name ## Function name = reinterpret_cast<name ## Function>( \
      ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), #name))

// Reverts to self and verify that SetInformationToken was faked. Returns
// SBOX_TEST_SUCCEEDED if faked and SBOX_TEST_FAILED if not faked.
SBOX_TESTS_COMMAND int PolicyTargetTest_token(int argc, wchar_t **argv) {
  HANDLE thread_token;
  // Get the thread token, using impersonation.
  if (!::OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE |
                             TOKEN_DUPLICATE, FALSE, &thread_token))
    return ::GetLastError();

  ::RevertToSelf();
  ::CloseHandle(thread_token);

  int ret = SBOX_TEST_FAILED;
  if (::OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                        FALSE, &thread_token)) {
    ret = SBOX_TEST_SUCCEEDED;
    ::CloseHandle(thread_token);
  }
  return ret;
}

// Stores the high privilege token on a static variable, change impersonation
// again to that one and verify that we are not interfering anymore with
// RevertToSelf.
SBOX_TESTS_COMMAND int PolicyTargetTest_steal(int argc, wchar_t **argv) {
  static HANDLE thread_token;
  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf()) {
    if (!::OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE |
                               TOKEN_DUPLICATE, FALSE, &thread_token))
      return ::GetLastError();
  } else {
    if (!::SetThreadToken(NULL, thread_token))
      return ::GetLastError();

    // See if we fake the call again.
    int ret = PolicyTargetTest_token(argc, argv);
    ::CloseHandle(thread_token);
    return ret;
  }
  return 0;
}

// Opens the thread token with and without impersonation.
SBOX_TESTS_COMMAND int PolicyTargetTest_token2(int argc, wchar_t **argv) {
  HANDLE thread_token;
  // Get the thread token, using impersonation.
  if (!::OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE |
                             TOKEN_DUPLICATE, FALSE, &thread_token))
    return ::GetLastError();
  ::CloseHandle(thread_token);

  // Get the thread token, without impersonation.
  if (!OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                       TRUE, &thread_token))
    return ::GetLastError();
  ::CloseHandle(thread_token);
  return SBOX_TEST_SUCCEEDED;
}

// Opens the thread token with and without impersonation, using
// NtOpenThreadTokenEX.
SBOX_TESTS_COMMAND int PolicyTargetTest_token3(int argc, wchar_t **argv) {
  BINDNTDLL(NtOpenThreadTokenEx);
  if (!NtOpenThreadTokenEx)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  HANDLE thread_token;
  // Get the thread token, using impersonation.
  NTSTATUS status = NtOpenThreadTokenEx(GetCurrentThread(),
                                        TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                                        FALSE, 0, &thread_token);
  if (status == STATUS_NO_TOKEN)
    return ERROR_NO_TOKEN;
  if (!NT_SUCCESS(status))
    return SBOX_TEST_FAILED;

  ::CloseHandle(thread_token);

  // Get the thread token, without impersonation.
  status = NtOpenThreadTokenEx(GetCurrentThread(),
                               TOKEN_IMPERSONATE | TOKEN_DUPLICATE, TRUE, 0,
                               &thread_token);
  if (!NT_SUCCESS(status))
    return SBOX_TEST_FAILED;

  ::CloseHandle(thread_token);
  return SBOX_TEST_SUCCEEDED;
}

// Tests that we can open the current thread.
SBOX_TESTS_COMMAND int PolicyTargetTest_thread(int argc, wchar_t **argv) {
  DWORD thread_id = ::GetCurrentThreadId();
  HANDLE thread = ::OpenThread(SYNCHRONIZE, FALSE, thread_id);
  if (!thread)
    return ::GetLastError();
  if (!::CloseHandle(thread))
    return ::GetLastError();

  return SBOX_TEST_SUCCEEDED;
}

// New thread entry point: do  nothing.
DWORD WINAPI PolicyTargetTest_thread_main(void* param) {
  ::Sleep(INFINITE);
  return 0;
}

// Tests that we can create a new thread, and open it.
SBOX_TESTS_COMMAND int PolicyTargetTest_thread2(int argc, wchar_t **argv) {
  // Use default values to create a new thread.
  DWORD thread_id;
  HANDLE thread = ::CreateThread(NULL, 0, &PolicyTargetTest_thread_main, 0, 0,
                                 &thread_id);
  if (!thread)
    return ::GetLastError();
  if (!::CloseHandle(thread))
    return ::GetLastError();

  thread = ::OpenThread(SYNCHRONIZE, FALSE, thread_id);
  if (!thread)
    return ::GetLastError();

  if (!::CloseHandle(thread))
    return ::GetLastError();

  return SBOX_TEST_SUCCEEDED;
}

// Tests that we can call CreateProcess.
SBOX_TESTS_COMMAND int PolicyTargetTest_process(int argc, wchar_t **argv) {
  // Use default values to create a new process.
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION temp_process_info = {};
  // Note: CreateProcessW() can write to its lpCommandLine, don't pass a
  // raw string literal.
  base::string16 writable_cmdline_str(L"foo.exe");
  if (!::CreateProcessW(L"foo.exe", &writable_cmdline_str[0], NULL, NULL, FALSE,
                        0, NULL, NULL, &startup_info, &temp_process_info))
    return SBOX_TEST_SUCCEEDED;
  base::win::ScopedProcessInformation process_info(temp_process_info);
  return SBOX_TEST_FAILED;
}

TEST(PolicyTargetTest, SetInformationThread) {
  TestRunner runner;
  if (base::win::GetVersion() >= base::win::VERSION_XP) {
    runner.SetTestState(BEFORE_REVERT);
    EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_token"));
  }

  runner.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner.RunTest(L"PolicyTargetTest_token"));

  runner.SetTestState(EVERY_STATE);
  if (base::win::GetVersion() >= base::win::VERSION_XP)
    EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(L"PolicyTargetTest_steal"));
}

TEST(PolicyTargetTest, OpenThreadToken) {
  TestRunner runner;
  if (base::win::GetVersion() >= base::win::VERSION_XP) {
    runner.SetTestState(BEFORE_REVERT);
    EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_token2"));
  }

  runner.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner.RunTest(L"PolicyTargetTest_token2"));
}

TEST(PolicyTargetTest, OpenThreadTokenEx) {
  TestRunner runner;
  if (base::win::GetVersion() < base::win::VERSION_XP)
    return;

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_token3"));

  runner.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner.RunTest(L"PolicyTargetTest_token3"));
}

TEST(PolicyTargetTest, OpenThread) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_thread")) <<
      "Opens the current thread";

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_thread2")) <<
      "Creates a new thread and opens it";
}

TEST(PolicyTargetTest, OpenProcess) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_process")) <<
      "Opens a process";
}

// Launches the app in the sandbox and ask it to wait in an
// infinite loop. Waits for 2 seconds and then check if the
// desktop associated with the app thread is not the same as the
// current desktop.
TEST(PolicyTargetTest, DesktopPolicy) {
  BrokerServices* broker = GetBroker();

  // Precreate the desktop.
  TargetPolicy* temp_policy = broker->CreatePolicy();
  temp_policy->CreateAlternateDesktop(false);
  temp_policy->Release();

  ASSERT_TRUE(broker != NULL);

  // Get the path to the sandboxed app.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(NULL, prog_name, MAX_PATH);

  base::string16 arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child 0 wait";  // Don't care about the "state" argument.

  // Launch the app.
  ResultCode result = SBOX_ALL_OK;
  base::win::ScopedProcessInformation target;

  TargetPolicy* policy = broker->CreatePolicy();
  policy->SetAlternateDesktop(false);
  policy->SetTokenLevel(USER_INTERACTIVE, USER_LOCKDOWN);
  PROCESS_INFORMATION temp_process_info = {};
  result = broker->SpawnTarget(prog_name, arguments.c_str(), policy,
                               &temp_process_info);
  base::string16 desktop_name = policy->GetAlternateDesktop();
  policy->Release();

  EXPECT_EQ(SBOX_ALL_OK, result);
  if (result == SBOX_ALL_OK)
    target.Set(temp_process_info);

  EXPECT_EQ(1, ::ResumeThread(target.thread_handle()));

  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(target.process_handle(), 2000));

  EXPECT_NE(::GetThreadDesktop(target.thread_id()),
            ::GetThreadDesktop(::GetCurrentThreadId()));

  HDESK desk = ::OpenDesktop(desktop_name.c_str(), 0, FALSE, DESKTOP_ENUMERATE);
  EXPECT_TRUE(NULL != desk);
  EXPECT_TRUE(::CloseDesktop(desk));
  EXPECT_TRUE(::TerminateProcess(target.process_handle(), 0));

  ::WaitForSingleObject(target.process_handle(), INFINITE);

  // Close the desktop handle.
  temp_policy = broker->CreatePolicy();
  temp_policy->DestroyAlternateDesktop();
  temp_policy->Release();

  // Make sure the desktop does not exist anymore.
  desk = ::OpenDesktop(desktop_name.c_str(), 0, FALSE, DESKTOP_ENUMERATE);
  EXPECT_TRUE(NULL == desk);
}

// Launches the app in the sandbox and ask it to wait in an
// infinite loop. Waits for 2 seconds and then check if the
// winstation associated with the app thread is not the same as the
// current desktop.
TEST(PolicyTargetTest, WinstaPolicy) {
  BrokerServices* broker = GetBroker();

  // Precreate the desktop.
  TargetPolicy* temp_policy = broker->CreatePolicy();
  temp_policy->CreateAlternateDesktop(true);
  temp_policy->Release();

  ASSERT_TRUE(broker != NULL);

  // Get the path to the sandboxed app.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(NULL, prog_name, MAX_PATH);

  base::string16 arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child 0 wait";  // Don't care about the "state" argument.

  // Launch the app.
  ResultCode result = SBOX_ALL_OK;
  base::win::ScopedProcessInformation target;

  TargetPolicy* policy = broker->CreatePolicy();
  policy->SetAlternateDesktop(true);
  policy->SetTokenLevel(USER_INTERACTIVE, USER_LOCKDOWN);
  PROCESS_INFORMATION temp_process_info = {};
  result = broker->SpawnTarget(prog_name, arguments.c_str(), policy,
                               &temp_process_info);
  base::string16 desktop_name = policy->GetAlternateDesktop();
  policy->Release();

  EXPECT_EQ(SBOX_ALL_OK, result);
  if (result == SBOX_ALL_OK)
    target.Set(temp_process_info);

  EXPECT_EQ(1, ::ResumeThread(target.thread_handle()));

  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(target.process_handle(), 2000));

  EXPECT_NE(::GetThreadDesktop(target.thread_id()),
            ::GetThreadDesktop(::GetCurrentThreadId()));

  ASSERT_FALSE(desktop_name.empty());

  // Make sure there is a backslash, for the window station name.
  EXPECT_NE(desktop_name.find_first_of(L'\\'), base::string16::npos);

  // Isolate the desktop name.
  desktop_name = desktop_name.substr(desktop_name.find_first_of(L'\\') + 1);

  HDESK desk = ::OpenDesktop(desktop_name.c_str(), 0, FALSE, DESKTOP_ENUMERATE);
  // This should fail if the desktop is really on another window station.
  EXPECT_FALSE(NULL != desk);
  EXPECT_TRUE(::TerminateProcess(target.process_handle(), 0));

  ::WaitForSingleObject(target.process_handle(), INFINITE);

  // Close the desktop handle.
  temp_policy = broker->CreatePolicy();
  temp_policy->DestroyAlternateDesktop();
  temp_policy->Release();
}

}  // namespace sandbox
