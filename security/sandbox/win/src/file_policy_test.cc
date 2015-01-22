// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cctype>

#include <windows.h>
#include <winioctl.h>

#include "base/win/scoped_handle.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/win_utils.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#define BINDNTDLL(name) \
  name ## Function name = reinterpret_cast<name ## Function>( \
    ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), #name))

namespace sandbox {

const ULONG kSharing = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;

// Creates a file using different desired access. Returns if the call succeeded
// or not.  The first argument in argv is the filename. If the second argument
// is "read", we try read only access. Otherwise we try read-write access.
SBOX_TESTS_COMMAND int File_Create(int argc, wchar_t **argv) {
  if (argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  bool read = (_wcsicmp(argv[0], L"Read") == 0);

  if (read) {
    base::win::ScopedHandle file1(CreateFile(
        argv[1], GENERIC_READ, kSharing, NULL, OPEN_EXISTING, 0, NULL));
    base::win::ScopedHandle file2(CreateFile(
        argv[1], FILE_EXECUTE, kSharing, NULL, OPEN_EXISTING, 0, NULL));

    if (file1.Get() && file2.Get())
      return SBOX_TEST_SUCCEEDED;
    return SBOX_TEST_DENIED;
  } else {
    base::win::ScopedHandle file1(CreateFile(
        argv[1], GENERIC_ALL, kSharing, NULL, OPEN_EXISTING, 0, NULL));
    base::win::ScopedHandle file2(CreateFile(
        argv[1], GENERIC_READ | FILE_WRITE_DATA, kSharing, NULL, OPEN_EXISTING,
        0, NULL));

    if (file1.Get() && file2.Get())
      return SBOX_TEST_SUCCEEDED;
    return SBOX_TEST_DENIED;
  }
}

SBOX_TESTS_COMMAND int File_Win32Create(int argc, wchar_t **argv) {
  if (argc != 1) {
    SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  base::string16 full_path = MakePathToSys(argv[0], false);
  if (full_path.empty()) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  HANDLE file = ::CreateFileW(full_path.c_str(), GENERIC_READ, kSharing,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (INVALID_HANDLE_VALUE != file) {
    ::CloseHandle(file);
    return SBOX_TEST_SUCCEEDED;
  } else {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    } else {
      return SBOX_TEST_FAILED;
    }
  }
  return SBOX_TEST_SUCCEEDED;
}

// Creates the file in parameter using the NtCreateFile api and returns if the
// call succeeded or not.
SBOX_TESTS_COMMAND int File_CreateSys32(int argc, wchar_t **argv) {
  BINDNTDLL(NtCreateFile);
  BINDNTDLL(RtlInitUnicodeString);
  if (!NtCreateFile || !RtlInitUnicodeString)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  base::string16 file(argv[0]);
  if (0 != _wcsnicmp(file.c_str(), kNTObjManPrefix, kNTObjManPrefixLen))
    file = MakePathToSys(argv[0], true);

  UNICODE_STRING object_name;
  RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {0};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, NULL, NULL);

  HANDLE handle;
  IO_STATUS_BLOCK io_block = {0};
  NTSTATUS status = NtCreateFile(&handle, FILE_READ_DATA, &obj_attributes,
                                 &io_block, NULL, 0, kSharing, FILE_OPEN,
                                 0, NULL, 0);
  if (NT_SUCCESS(status)) {
    ::CloseHandle(handle);
    return SBOX_TEST_SUCCEEDED;
  } else if (STATUS_ACCESS_DENIED == status) {
    return SBOX_TEST_DENIED;
  } else if (STATUS_OBJECT_NAME_NOT_FOUND == status) {
    return SBOX_TEST_NOT_FOUND;
  }
  return SBOX_TEST_FAILED;
}

// Opens the file in parameter using the NtOpenFile api and returns if the
// call succeeded or not.
SBOX_TESTS_COMMAND int File_OpenSys32(int argc, wchar_t **argv) {
  BINDNTDLL(NtOpenFile);
  BINDNTDLL(RtlInitUnicodeString);
  if (!NtOpenFile || !RtlInitUnicodeString)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  base::string16 file = MakePathToSys(argv[0], true);
  UNICODE_STRING object_name;
  RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {0};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, NULL, NULL);

  HANDLE handle;
  IO_STATUS_BLOCK io_block = {0};
  NTSTATUS status = NtOpenFile(&handle, FILE_READ_DATA, &obj_attributes,
                               &io_block, kSharing, 0);
  if (NT_SUCCESS(status)) {
    ::CloseHandle(handle);
    return SBOX_TEST_SUCCEEDED;
  } else if (STATUS_ACCESS_DENIED == status) {
    return SBOX_TEST_DENIED;
  } else if (STATUS_OBJECT_NAME_NOT_FOUND == status) {
    return SBOX_TEST_NOT_FOUND;
  }
  return SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int File_GetDiskSpace(int argc, wchar_t **argv) {
  base::string16 sys_path = MakePathToSys(L"", false);
  if (sys_path.empty()) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  ULARGE_INTEGER free_user = {0};
  ULARGE_INTEGER total = {0};
  ULARGE_INTEGER free_total = {0};
  if (::GetDiskFreeSpaceExW(sys_path.c_str(), &free_user, &total,
                            &free_total)) {
    if ((total.QuadPart != 0) && (free_total.QuadPart !=0)) {
      return SBOX_TEST_SUCCEEDED;
    }
  } else {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    } else {
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
  }
  return SBOX_TEST_SUCCEEDED;
}

// Move a file using the MoveFileEx api and returns if the call succeeded or
// not.
SBOX_TESTS_COMMAND int File_Rename(int argc, wchar_t **argv) {
  if (argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (::MoveFileEx(argv[0], argv[1], 0))
    return SBOX_TEST_SUCCEEDED;

  if (::GetLastError() != ERROR_ACCESS_DENIED)
    return SBOX_TEST_FAILED;

  return SBOX_TEST_DENIED;
}

// Query the attributes of file in parameter using the NtQueryAttributesFile api
// and NtQueryFullAttributesFile and returns if the call succeeded or not. The
// second argument in argv is "d" or "f" telling if we expect the attributes to
// specify a file or a directory. The expected attribute has to match the real
// attributes for the call to be successful.
SBOX_TESTS_COMMAND int File_QueryAttributes(int argc, wchar_t **argv) {
  BINDNTDLL(NtQueryAttributesFile);
  BINDNTDLL(NtQueryFullAttributesFile);
  BINDNTDLL(RtlInitUnicodeString);
  if (!NtQueryAttributesFile || !NtQueryFullAttributesFile ||
      !RtlInitUnicodeString)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  bool expect_directory = (L'd' == argv[1][0]);

  UNICODE_STRING object_name;
  base::string16 file = MakePathToSys(argv[0], true);
  RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {0};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, NULL, NULL);

  FILE_BASIC_INFORMATION info = {0};
  FILE_NETWORK_OPEN_INFORMATION full_info = {0};
  NTSTATUS status1 = NtQueryAttributesFile(&obj_attributes, &info);
  NTSTATUS status2 = NtQueryFullAttributesFile(&obj_attributes, &full_info);

  if (status1 != status2)
    return SBOX_TEST_FAILED;

  if (NT_SUCCESS(status1)) {
    if (info.FileAttributes != full_info.FileAttributes)
      return SBOX_TEST_FAILED;

    bool is_directory1 = (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (expect_directory == is_directory1)
      return SBOX_TEST_SUCCEEDED;
  } else if (STATUS_ACCESS_DENIED == status1) {
    return SBOX_TEST_DENIED;
  } else if (STATUS_OBJECT_NAME_NOT_FOUND == status1) {
    return SBOX_TEST_NOT_FOUND;
  }

  return SBOX_TEST_FAILED;
}

TEST(FilePolicyTest, DenyNtCreateCalc) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_DIR_ANY,
                                  L"calc.exe"));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_CreateSys32 calc.exe"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_CreateSys32 calc.exe"));
}

TEST(FilePolicyTest, AllowNtCreateCalc) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY, L"calc.exe"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_CreateSys32 calc.exe"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_CreateSys32 calc.exe"));
}

TEST(FilePolicyTest, AllowNtCreateWithNativePath) {
  base::string16 calc = MakePathToSys(L"calc.exe", false);
  base::string16 nt_path;
  ASSERT_TRUE(GetNtPathFromWin32Path(calc, &nt_path));
  TestRunner runner;
  runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY, nt_path.c_str());

  wchar_t buff[MAX_PATH];
  ::wsprintfW(buff, L"File_CreateSys32 %s", nt_path.c_str());
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(buff));

  std::transform(nt_path.begin(), nt_path.end(), nt_path.begin(), std::tolower);
  ::wsprintfW(buff, L"File_CreateSys32 %s", nt_path.c_str());
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(buff));
}

TEST(FilePolicyTest, AllowReadOnly) {
  TestRunner runner;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0u);

  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               temp_file_name));

  wchar_t command_read[MAX_PATH + 20] = {0};
  wsprintf(command_read, L"File_Create Read \"%ls\"", temp_file_name);
  wchar_t command_write[MAX_PATH + 20] = {0};
  wsprintf(command_write, L"File_Create Write \"%ls\"", temp_file_name);

  // Verify that we have read access after revert.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command_read));

  // Verify that we don't have write access after revert.
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command_write));

  // Verify that we really have write access to the file.
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command_write));

  DeleteFile(temp_file_name);
}

TEST(FilePolicyTest, AllowWildcard) {
  TestRunner runner;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0u);

  wcscat_s(temp_directory, MAX_PATH, L"*");
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_directory));

  wchar_t command_write[MAX_PATH + 20] = {0};
  wsprintf(command_write, L"File_Create Write \"%ls\"", temp_file_name);

  // Verify that we have write access after revert.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command_write));

  DeleteFile(temp_file_name);
}

TEST(FilePolicyTest, AllowNtCreatePatternRule) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY, L"App*.dll"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_OpenSys32 appmgmts.dll"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_OpenSys32 appwiz.cpl"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_OpenSys32 appmgmts.dll"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_OpenSys32 appwiz.cpl"));
}

TEST(FilePolicyTest, CheckNotFound) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY, L"n*.dll"));

  EXPECT_EQ(SBOX_TEST_NOT_FOUND,
            runner.RunTest(L"File_OpenSys32 notfound.dll"));
}

TEST(FilePolicyTest, CheckNoLeak) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_CreateSys32 notfound.exe"));
}

TEST(FilePolicyTest, TestQueryAttributesFile) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY,
                                  L"appmgmts.dll"));
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY,
                                  L"notfound.exe"));
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY, L"drivers"));
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_QUERY,
                                  L"ipconfig.exe"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_QueryAttributes drivers d"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_QueryAttributes appmgmts.dll f"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_QueryAttributes ipconfig.exe f"));

  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"File_QueryAttributes ftp.exe f"));

  EXPECT_EQ(SBOX_TEST_NOT_FOUND,
            runner.RunTest(L"File_QueryAttributes notfound.exe f"));
}

// Makes sure that we don't leak information when there is not policy to allow
// a path.
TEST(FilePolicyTest, TestQueryAttributesFileNoPolicy) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"File_QueryAttributes ftp.exe f"));

  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"File_QueryAttributes notfound.exe f"));
}

TEST(FilePolicyTest, TestRename) {
  TestRunner runner;

  // Give access to the temp directory.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name1[MAX_PATH];
  wchar_t temp_file_name2[MAX_PATH];
  wchar_t temp_file_name3[MAX_PATH];
  wchar_t temp_file_name4[MAX_PATH];
  wchar_t temp_file_name5[MAX_PATH];
  wchar_t temp_file_name6[MAX_PATH];
  wchar_t temp_file_name7[MAX_PATH];
  wchar_t temp_file_name8[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name1), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name2), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name3), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name4), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name5), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name6), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name7), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name8), 0u);


  // Add rules to make file1->file2 succeed.
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_file_name1));
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_file_name2));

  // Add rules to make file3->file4 fail.
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_file_name3));
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               temp_file_name4));

  // Add rules to make file5->file6 fail.
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               temp_file_name5));
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_file_name6));

  // Add rules to make file7->no_pol_file fail.
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_file_name7));

  // Delete the files where the files are going to be renamed to.
  ::DeleteFile(temp_file_name2);
  ::DeleteFile(temp_file_name4);
  ::DeleteFile(temp_file_name6);
  ::DeleteFile(temp_file_name8);


  wchar_t command[MAX_PATH*2 + 20] = {0};
  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_file_name1,
           temp_file_name2);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command));

  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_file_name3,
           temp_file_name4);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));

  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_file_name5,
           temp_file_name6);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));

  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_file_name7,
           temp_file_name8);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));


  // Delete all the files in case they are still there.
  ::DeleteFile(temp_file_name1);
  ::DeleteFile(temp_file_name2);
  ::DeleteFile(temp_file_name3);
  ::DeleteFile(temp_file_name4);
  ::DeleteFile(temp_file_name5);
  ::DeleteFile(temp_file_name6);
  ::DeleteFile(temp_file_name7);
  ::DeleteFile(temp_file_name8);
}

TEST(FilePolicyTest, OpenSys32FilesDenyBecauseOfDir) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_DIR_ANY,
                                  L"notepad.exe"));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_Win32Create notepad.exe"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_Win32Create notepad.exe"));
}

TEST(FilePolicyTest, OpenSys32FilesAllowNotepad) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY,
                                  L"notepad.exe"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_Win32Create notepad.exe"));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_Win32Create calc.exe"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_Win32Create notepad.exe"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_Win32Create calc.exe"));
}

TEST(FilePolicyTest, FileGetDiskSpace) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_GetDiskSpace"));
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_GetDiskSpace"));

  // Add an 'allow' rule in the windows\system32 such that GetDiskFreeSpaceEx
  // succeeds (it does an NtOpenFile) but windows\system32\notepad.exe is
  // denied since there is no wild card in the rule.
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_DIR_ANY, L""));
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_GetDiskSpace"));

  runner.SetTestState(AFTER_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_GetDiskSpace"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_Win32Create notepad.exe"));
}

// http://crbug.com/146944
TEST(FilePolicyTest, DISABLED_TestReparsePoint) {
  TestRunner runner;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0u);

  // Delete the file and create a directory instead.
  ASSERT_TRUE(::DeleteFile(temp_file_name));
  ASSERT_TRUE(::CreateDirectory(temp_file_name, NULL));

  // Create a temporary file in the subfolder.
  base::string16 subfolder = temp_file_name;
  base::string16 temp_file_title = subfolder.substr(subfolder.rfind(L"\\") + 1);
  base::string16 temp_file = subfolder + L"\\file_" + temp_file_title;

  HANDLE file = ::CreateFile(temp_file.c_str(), FILE_ALL_ACCESS,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             CREATE_ALWAYS, 0, NULL);
  ASSERT_TRUE(INVALID_HANDLE_VALUE != file);
  ASSERT_TRUE(::CloseHandle(file));

  // Create a temporary file in the temp directory.
  base::string16 temp_dir = temp_directory;
  base::string16 temp_file_in_temp = temp_dir + L"file_" + temp_file_title;
  file = ::CreateFile(temp_file_in_temp.c_str(), FILE_ALL_ACCESS,
                      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                      CREATE_ALWAYS, 0, NULL);
  ASSERT_TRUE(file != NULL);
  ASSERT_TRUE(::CloseHandle(file));

  // Give write access to the temp directory.
  base::string16 temp_dir_wildcard = temp_dir + L"*";
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY,
                               temp_dir_wildcard.c_str()));

  // Prepare the command to execute.
  base::string16 command_write;
  command_write += L"File_Create Write \"";
  command_write += temp_file;
  command_write += L"\"";

  // Verify that we have write access to the original file
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command_write.c_str()));

  // Replace the subfolder by a reparse point to %temp%.
  ::DeleteFile(temp_file.c_str());
  HANDLE dir = ::CreateFile(subfolder.c_str(), FILE_ALL_ACCESS,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  EXPECT_TRUE(INVALID_HANDLE_VALUE != dir);

  base::string16 temp_dir_nt;
  temp_dir_nt += L"\\??\\";
  temp_dir_nt += temp_dir;
  EXPECT_TRUE(SetReparsePoint(dir, temp_dir_nt.c_str()));
  EXPECT_TRUE(::CloseHandle(dir));

  // Try to open the file again.
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command_write.c_str()));

  // Remove the reparse point.
  dir = ::CreateFile(subfolder.c_str(), FILE_ALL_ACCESS,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                     FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                     NULL);
  EXPECT_TRUE(INVALID_HANDLE_VALUE != dir);
  EXPECT_TRUE(DeleteReparsePoint(dir));
  EXPECT_TRUE(::CloseHandle(dir));

  // Cleanup.
  EXPECT_TRUE(::DeleteFile(temp_file_in_temp.c_str()));
  EXPECT_TRUE(::RemoveDirectory(subfolder.c_str()));
}

}  // namespace sandbox
