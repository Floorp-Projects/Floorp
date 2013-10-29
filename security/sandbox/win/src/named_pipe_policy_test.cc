// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/tests/common/controller.h"

namespace sandbox {


SBOX_TESTS_COMMAND int NamedPipe_Create(int argc, wchar_t **argv) {
  if (argc != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  if ((NULL == argv) || (NULL == argv[0])) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  HANDLE pipe = ::CreateNamedPipeW(argv[0],
                                   PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, 4096,
                                   4096, 2000, NULL);
  if (INVALID_HANDLE_VALUE == pipe)
    return SBOX_TEST_DENIED;

  OVERLAPPED overlapped = {0};
  overlapped.hEvent = ::CreateEvent(NULL, TRUE, TRUE, NULL);
  BOOL result = ::ConnectNamedPipe(pipe, &overlapped);

  if (!result) {
    DWORD error = ::GetLastError();
    if (ERROR_PIPE_CONNECTED != error &&
        ERROR_IO_PENDING != error) {
          return SBOX_TEST_FAILED;
    }
  }

  if (!::CloseHandle(pipe))
    return SBOX_TEST_FAILED;

  ::CloseHandle(overlapped.hEvent);
  return SBOX_TEST_SUCCEEDED;
}

// Tests if we can create a pipe in the sandbox. On XP, the sandbox can create
// a pipe without any help but it fails on Vista, this is why we do not test
// the "denied" case.
TEST(NamedPipePolicyTest, CreatePipe) {
  TestRunner runner;
  // TODO(nsylvain): This policy is wrong because "*" is a valid char in a
  // namedpipe name. Here we apply it like a wildcard. http://b/893603
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_NAMED_PIPES,
                             TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                              L"\\\\.\\pipe\\test*"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\testbleh"));
}

// The same test as CreatePipe but this time using strict interceptions.
TEST(NamedPipePolicyTest, CreatePipeStrictInterceptions) {
  TestRunner runner;
  runner.GetPolicy()->SetStrictInterceptions();

  // TODO(nsylvain): This policy is wrong because "*" is a valid char in a
  // namedpipe name. Here we apply it like a wildcard. http://b/893603
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_NAMED_PIPES,
                             TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                              L"\\\\.\\pipe\\test*"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\testbleh"));
}

}  // namespace sandbox
