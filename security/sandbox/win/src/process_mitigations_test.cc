// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/win/scoped_handle.h"

#include "base/win/windows_version.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/src/win_utils.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef BOOL (WINAPI *GetProcessDEPPolicyFunction)(
    HANDLE process,
    LPDWORD flags,
    PBOOL permanent);

typedef BOOL (WINAPI *GetProcessMitigationPolicyFunction)(
    HANDLE process,
    PROCESS_MITIGATION_POLICY mitigation_policy,
    PVOID buffer,
    SIZE_T length);

GetProcessMitigationPolicyFunction get_process_mitigation_policy;

bool CheckWin8DepPolicy() {
  PROCESS_MITIGATION_DEP_POLICY policy;
  if (!get_process_mitigation_policy(::GetCurrentProcess(), ProcessDEPPolicy,
                                     &policy, sizeof(policy))) {
    return false;
  }
  return policy.Enable && policy.Permanent;
}

bool CheckWin8AslrPolicy() {
  PROCESS_MITIGATION_ASLR_POLICY policy;
  if (!get_process_mitigation_policy(::GetCurrentProcess(), ProcessASLRPolicy,
                                     &policy, sizeof(policy))) {
     return false;
  }
  return policy.EnableForceRelocateImages && policy.DisallowStrippedImages;
}

bool CheckWin8StrictHandlePolicy() {
  PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY policy;
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessStrictHandleCheckPolicy,
                                     &policy, sizeof(policy))) {
     return false;
  }
  return policy.RaiseExceptionOnInvalidHandleReference &&
         policy.HandleExceptionsPermanentlyEnabled;
}

bool CheckWin8Win32CallPolicy() {
  PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy;
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessSystemCallDisablePolicy,
                                     &policy, sizeof(policy))) {
     return false;
  }
  return policy.DisallowWin32kSystemCalls;
}

bool CheckWin8DllExtensionPolicy() {
  PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy;
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessExtensionPointDisablePolicy,
                                     &policy, sizeof(policy))) {
     return false;
  }
  return policy.DisableExtensionPoints;
}

}  // namespace

namespace sandbox {

SBOX_TESTS_COMMAND int CheckWin8(int argc, wchar_t **argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(
          ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"),
                           "GetProcessMitigationPolicy"));

  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

  if (!CheckWin8DepPolicy())
    return SBOX_TEST_FIRST_ERROR;

#if defined(NDEBUG)  // ASLR cannot be forced in debug builds.
  if (!CheckWin8AslrPolicy())
    return SBOX_TEST_SECOND_ERROR;
#endif

  if (!CheckWin8StrictHandlePolicy())
    return SBOX_TEST_THIRD_ERROR;

  if (!CheckWin8Win32CallPolicy())
    return SBOX_TEST_FOURTH_ERROR;

  if (!CheckWin8DllExtensionPolicy())
    return SBOX_TEST_FIFTH_ERROR;

  return SBOX_TEST_SUCCEEDED;
}

TEST(ProcessMitigationsTest, CheckWin8) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  sandbox::MitigationFlags mitigations = MITIGATION_DEP |
                                         MITIGATION_DEP_NO_ATL_THUNK |
                                         MITIGATION_EXTENSION_DLL_DISABLE;
#if defined(NDEBUG)  // ASLR cannot be forced in debug builds.
  mitigations |= MITIGATION_RELOCATE_IMAGE |
                 MITIGATION_RELOCATE_IMAGE_REQUIRED;
#endif

  EXPECT_EQ(policy->SetProcessMitigations(mitigations), SBOX_ALL_OK);

  mitigations |= MITIGATION_STRICT_HANDLE_CHECKS |
                 MITIGATION_WIN32K_DISABLE;

  EXPECT_EQ(policy->SetDelayedProcessMitigations(mitigations), SBOX_ALL_OK);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8"));
}


SBOX_TESTS_COMMAND int CheckDep(int argc, wchar_t **argv) {
  GetProcessDEPPolicyFunction get_process_dep_policy =
      reinterpret_cast<GetProcessDEPPolicyFunction>(
          ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"),
                           "GetProcessDEPPolicy"));
  if (get_process_dep_policy) {
    BOOL is_permanent = FALSE;
    DWORD dep_flags = 0;

    if (!get_process_dep_policy(::GetCurrentProcess(), &dep_flags,
        &is_permanent)) {
      return SBOX_TEST_FIRST_ERROR;
    }

    if (!(dep_flags & PROCESS_DEP_ENABLE) || !is_permanent)
      return SBOX_TEST_SECOND_ERROR;

  } else {
    NtQueryInformationProcessFunction query_information_process = NULL;
    ResolveNTFunctionPtr("NtQueryInformationProcess",
                          &query_information_process);
    if (!query_information_process)
      return SBOX_TEST_NOT_FOUND;

    ULONG size = 0;
    ULONG dep_flags = 0;
    if (!SUCCEEDED(query_information_process(::GetCurrentProcess(),
                                             ProcessExecuteFlags, &dep_flags,
                                             sizeof(dep_flags), &size))) {
      return SBOX_TEST_THIRD_ERROR;
    }

    const int MEM_EXECUTE_OPTION_ENABLE = 1;
    const int MEM_EXECUTE_OPTION_DISABLE = 2;
    const int MEM_EXECUTE_OPTION_ATL7_THUNK_EMULATION = 4;
    const int MEM_EXECUTE_OPTION_PERMANENT = 8;
    dep_flags &= 0xff;

    if (dep_flags != (MEM_EXECUTE_OPTION_DISABLE |
                      MEM_EXECUTE_OPTION_PERMANENT)) {
      return SBOX_TEST_FOURTH_ERROR;
    }
  }

  return SBOX_TEST_SUCCEEDED;
}

#if !defined(_WIN64)  // DEP is always enabled on 64-bit.
TEST(ProcessMitigationsTest, CheckDep) {
  if (!IsXPSP2OrLater() || base::win::GetVersion() > base::win::VERSION_WIN7)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(
                MITIGATION_DEP |
                MITIGATION_DEP_NO_ATL_THUNK |
                MITIGATION_SEHOP),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckDep"));
}
#endif

}  // namespace sandbox

