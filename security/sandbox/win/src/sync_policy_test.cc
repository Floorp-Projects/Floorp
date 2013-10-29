// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sync_policy_test.h"

#include "base/win/scoped_handle.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/nt_internals.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

SBOX_TESTS_COMMAND int Event_Open(int argc, wchar_t **argv) {
  if (argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  DWORD desired_access = SYNCHRONIZE;
  if (L'f' == argv[0][0])
    desired_access = EVENT_ALL_ACCESS;

  base::win::ScopedHandle event_open(::OpenEvent(
      desired_access, FALSE, argv[1]));
  DWORD error_open = ::GetLastError();

  if (event_open.Get())
    return SBOX_TEST_SUCCEEDED;

  if (ERROR_ACCESS_DENIED == error_open ||
      ERROR_BAD_PATHNAME == error_open)
    return SBOX_TEST_DENIED;

  return SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int Event_CreateOpen(int argc, wchar_t **argv) {
  if (argc < 2 || argc > 3)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  wchar_t *event_name = NULL;
  if (3 == argc)
    event_name = argv[2];

  BOOL manual_reset = FALSE;
  BOOL initial_state = FALSE;
  if (L't' == argv[0][0])
    manual_reset = TRUE;
  if (L't' == argv[1][0])
    initial_state = TRUE;

  base::win::ScopedHandle event_create(::CreateEvent(
      NULL, manual_reset, initial_state, event_name));
  DWORD error_create = ::GetLastError();
  base::win::ScopedHandle event_open;
  if (event_name)
    event_open.Set(::OpenEvent(EVENT_ALL_ACCESS, FALSE, event_name));

  if (event_create.Get()) {
    DWORD wait = ::WaitForSingleObject(event_create.Get(), 0);
    if (initial_state && WAIT_OBJECT_0 != wait)
      return SBOX_TEST_FAILED;

    if (!initial_state && WAIT_TIMEOUT != wait)
      return SBOX_TEST_FAILED;
  }

  if (event_name) {
    // Both event_open and event_create have to be valid.
    if (event_open.Get() && event_create)
      return SBOX_TEST_SUCCEEDED;

    if (event_open.Get() && !event_create || !event_open.Get() && event_create)
      return SBOX_TEST_FAILED;
  } else {
    // Only event_create has to be valid.
    if (event_create.Get())
      return SBOX_TEST_SUCCEEDED;
  }

  if (ERROR_ACCESS_DENIED == error_create ||
      ERROR_BAD_PATHNAME == error_create)
    return SBOX_TEST_DENIED;

  return SBOX_TEST_FAILED;
}

// Tests the creation of events using all the possible combinations.
TEST(SyncPolicyTest, TestEvent) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_SYNC,
                             TargetPolicy::EVENTS_ALLOW_ANY,
                             L"test1"));
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_SYNC,
                             TargetPolicy::EVENTS_ALLOW_ANY,
                             L"test2"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_CreateOpen f f"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_CreateOpen t f"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_CreateOpen f t"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_CreateOpen t t"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_CreateOpen f f test1"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_CreateOpen t f test2"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_CreateOpen f t test1"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_CreateOpen t t test2"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_CreateOpen f f test3"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_CreateOpen t f test4"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_CreateOpen f t test3"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_CreateOpen t t test4"));
}

// Tests opening events with read only access.
TEST(SyncPolicyTest, TestEventReadOnly) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_SYNC,
                             TargetPolicy::EVENTS_ALLOW_READONLY,
                             L"test1"));
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_SYNC,
                             TargetPolicy::EVENTS_ALLOW_READONLY,
                             L"test2"));
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_SYNC,
                             TargetPolicy::EVENTS_ALLOW_READONLY,
                             L"test5"));
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_SYNC,
                             TargetPolicy::EVENTS_ALLOW_READONLY,
                             L"test6"));

  base::win::ScopedHandle handle1(::CreateEvent(NULL, FALSE, FALSE, L"test1"));
  base::win::ScopedHandle handle2(::CreateEvent(NULL, FALSE, FALSE, L"test2"));
  base::win::ScopedHandle handle3(::CreateEvent(NULL, FALSE, FALSE, L"test3"));
  base::win::ScopedHandle handle4(::CreateEvent(NULL, FALSE, FALSE, L"test4"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_CreateOpen f f"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_CreateOpen t f"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_Open f test1"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_Open s test2"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_Open f test3"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_Open s test4"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_CreateOpen f f test5"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_CreateOpen t f test6"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_CreateOpen f t test5"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_CreateOpen t t test6"));
}

}  // namespace sandbox
