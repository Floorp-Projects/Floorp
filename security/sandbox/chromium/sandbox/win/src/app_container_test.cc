// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/sync_policy_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t kAppContainerSid[] =
    L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
    L"924012148-2839372144";

}  // namespace

namespace sandbox {

TEST(AppContainerTest, DenyOpenEventForLowBox) {
  if (base::win::OSInfo::GetInstance()->version() < base::win::VERSION_WIN8)
    return;

  TestRunner runner(JOB_UNPROTECTED, USER_UNPROTECTED, USER_UNPROTECTED);

  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetLowBox(kAppContainerSid));
  // Run test once, this ensures the app container directory exists, we
  // ignore the result.
  runner.RunTest(L"Event_Open f test");
  base::string16 event_name = L"AppContainerNamedObjects\\";
  event_name += kAppContainerSid;
  event_name += L"\\test";

  base::win::ScopedHandle event(
      ::CreateEvent(NULL, FALSE, FALSE, event_name.c_str()));
  ASSERT_TRUE(event.IsValid());

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_Open f test"));
}

// TODO(shrikant): Please add some tests to prove usage of lowbox token like
// socket connection to local server in lock down mode.

}  // namespace sandbox
