// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/files/file_util.h"
#include "base/win/windows_version.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

SBOX_TESTS_COMMAND int HandleInheritanceTests_PrintToStdout(int argc,
                                                            wchar_t** argv) {
  printf("Example output to stdout\n");
  return SBOX_TEST_SUCCEEDED;
}

TEST(HandleInheritanceTests, TestStdoutInheritance) {
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0u);

  SECURITY_ATTRIBUTES attrs = {};
  attrs.nLength = sizeof(attrs);
  attrs.lpSecurityDescriptor = NULL;
  attrs.bInheritHandle = TRUE;
  HANDLE file_handle = CreateFile(
      temp_file_name, GENERIC_WRITE,
      FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
      &attrs, OPEN_EXISTING, 0, NULL);
  EXPECT_NE(file_handle, INVALID_HANDLE_VALUE);

  TestRunner runner;
  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetStdoutHandle(file_handle));
  int result = runner.RunTest(L"HandleInheritanceTests_PrintToStdout");
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, result);
  EXPECT_TRUE(::CloseHandle(file_handle));

  std::string data;
  EXPECT_TRUE(base::ReadFileToString(base::FilePath(temp_file_name), &data));
  // Redirection uses a feature that was added in Windows Vista.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    EXPECT_EQ("Example output to stdout\r\n", data);
  } else {
    EXPECT_EQ("", data);
  }

  EXPECT_TRUE(::DeleteFile(temp_file_name));
}

}
