// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <atlsecurity.h>

#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/tests/common/controller.h"

namespace sandbox {


SBOX_TESTS_COMMAND int CheckIntegrityLevel(int argc, wchar_t **argv) {
  ATL::CAccessToken token;
  if (!token.GetEffectiveToken(TOKEN_READ))
    return SBOX_TEST_FAILED;

  char* buffer[100];
  DWORD buf_size = 100;
  if (!::GetTokenInformation(token.GetHandle(), TokenIntegrityLevel,
                             reinterpret_cast<void*>(buffer), buf_size,
                             &buf_size))
    return SBOX_TEST_FAILED;

  TOKEN_MANDATORY_LABEL* label =
      reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer);

  PSID sid_low = NULL;
  if (!::ConvertStringSidToSid(L"S-1-16-4096", &sid_low))
    return SBOX_TEST_FAILED;

  BOOL is_low_sid = ::EqualSid(label->Label.Sid, sid_low);

  ::LocalFree(sid_low);

  if (is_low_sid)
    return SBOX_TEST_SUCCEEDED;

  return SBOX_TEST_DENIED;
}

TEST(IntegrityLevelTest, TestLowILReal) {
  if (base::win::GetVersion() != base::win::VERSION_VISTA)
    return;

  TestRunner runner(JOB_LOCKDOWN, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  runner.GetPolicy()->SetAlternateDesktop(true);
  runner.GetPolicy()->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIntegrityLevel"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIntegrityLevel"));
}

TEST(DelayedIntegrityLevelTest, TestLowILDelayed) {
  if (base::win::GetVersion() != base::win::VERSION_VISTA)
    return;

  TestRunner runner(JOB_LOCKDOWN, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  runner.GetPolicy()->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIntegrityLevel"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"CheckIntegrityLevel"));
}

TEST(IntegrityLevelTest, TestNoILChange) {
  if (base::win::GetVersion() != base::win::VERSION_VISTA)
    return;

  TestRunner runner(JOB_LOCKDOWN, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"CheckIntegrityLevel"));
}

}  // namespace sandbox
