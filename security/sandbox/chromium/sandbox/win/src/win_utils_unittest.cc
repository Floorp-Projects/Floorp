// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <psapi.h>

#include <vector>

#include "base/numerics/safe_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/win_utils.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ScopedTerminateProcess {
 public:
  ScopedTerminateProcess(HANDLE process) : process_(process) {}

  ~ScopedTerminateProcess() { ::TerminateProcess(process_, 0); }

 private:
  HANDLE process_;
};

bool GetModuleList(HANDLE process, std::vector<HMODULE>* result) {
  std::vector<HMODULE> modules(256);
  DWORD size_needed = 0;
  if (EnumProcessModules(
          process, &modules[0],
          base::checked_cast<DWORD>(modules.size() * sizeof(HMODULE)),
          &size_needed)) {
    result->assign(modules.begin(),
                   modules.begin() + (size_needed / sizeof(HMODULE)));
    return true;
  }
  modules.resize(size_needed / sizeof(HMODULE));
  if (EnumProcessModules(
          process, &modules[0],
          base::checked_cast<DWORD>(modules.size() * sizeof(HMODULE)),
          &size_needed)) {
    result->assign(modules.begin(),
                   modules.begin() + (size_needed / sizeof(HMODULE)));
    return true;
  }
  return false;
}

}  // namespace

TEST(WinUtils, IsReparsePoint) {
  using sandbox::IsReparsePoint;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t my_folder[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, my_folder), 0u);

  // Delete the file and create a directory instead.
  ASSERT_TRUE(::DeleteFile(my_folder));
  ASSERT_TRUE(::CreateDirectory(my_folder, NULL));

  EXPECT_EQ(static_cast<DWORD>(ERROR_NOT_A_REPARSE_POINT),
            IsReparsePoint(my_folder));

  base::string16 not_found = base::string16(my_folder) + L"\\foo\\bar";
  EXPECT_EQ(static_cast<DWORD>(ERROR_NOT_A_REPARSE_POINT),
            IsReparsePoint(not_found));

  base::string16 new_file = base::string16(my_folder) + L"\\foo";
  EXPECT_EQ(static_cast<DWORD>(ERROR_NOT_A_REPARSE_POINT),
            IsReparsePoint(new_file));

  // Replace the directory with a reparse point to %temp%.
  HANDLE dir = ::CreateFile(my_folder, FILE_ALL_ACCESS,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  EXPECT_NE(INVALID_HANDLE_VALUE, dir);

  base::string16 temp_dir_nt = base::string16(L"\\??\\") + temp_directory;
  EXPECT_TRUE(SetReparsePoint(dir, temp_dir_nt.c_str()));

  EXPECT_EQ(static_cast<DWORD>(ERROR_SUCCESS), IsReparsePoint(new_file));

  EXPECT_TRUE(DeleteReparsePoint(dir));
  EXPECT_TRUE(::CloseHandle(dir));
  EXPECT_TRUE(::RemoveDirectory(my_folder));
}

TEST(WinUtils, SameObject) {
  using sandbox::SameObject;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t my_folder[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, my_folder), 0u);

  // Delete the file and create a directory instead.
  ASSERT_TRUE(::DeleteFile(my_folder));
  ASSERT_TRUE(::CreateDirectory(my_folder, NULL));

  base::string16 folder(my_folder);
  base::string16 file_name = folder + L"\\foo.txt";
  const ULONG kSharing = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;
  base::win::ScopedHandle file(CreateFile(
      file_name.c_str(), GENERIC_WRITE, kSharing, NULL, CREATE_ALWAYS,
      FILE_FLAG_DELETE_ON_CLOSE, NULL));

  EXPECT_TRUE(file.IsValid());
  base::string16 file_name_nt1 = base::string16(L"\\??\\") + file_name;
  base::string16 file_name_nt2 =
      base::string16(L"\\??\\") + folder + L"\\FOO.txT";
  EXPECT_TRUE(SameObject(file.Get(), file_name_nt1.c_str()));
  EXPECT_TRUE(SameObject(file.Get(), file_name_nt2.c_str()));

  file.Close();
  EXPECT_TRUE(::RemoveDirectory(my_folder));
}

TEST(WinUtils, IsPipe) {
  using sandbox::IsPipe;

  base::string16 pipe_name = L"\\??\\pipe\\mypipe";
  EXPECT_TRUE(IsPipe(pipe_name));

  pipe_name = L"\\??\\PiPe\\mypipe";
  EXPECT_TRUE(IsPipe(pipe_name));

  pipe_name = L"\\??\\pipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\??\\_pipe_\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\??\\ABCD\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));


  // Written as two strings to prevent trigraph '?' '?' '/'.
  pipe_name = L"/?" L"?/pipe/mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\XX\\pipe\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\Device\\NamedPipe\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));
}

TEST(WinUtils, NtStatusToWin32Error) {
  using sandbox::GetLastErrorFromNtStatus;
  EXPECT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            GetLastErrorFromNtStatus(STATUS_SUCCESS));
  EXPECT_EQ(static_cast<DWORD>(ERROR_NOT_SUPPORTED),
            GetLastErrorFromNtStatus(STATUS_NOT_SUPPORTED));
  EXPECT_EQ(static_cast<DWORD>(ERROR_ALREADY_EXISTS),
            GetLastErrorFromNtStatus(STATUS_OBJECT_NAME_COLLISION));
  EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED),
            GetLastErrorFromNtStatus(STATUS_ACCESS_DENIED));
}

TEST(WinUtils, GetProcessBaseAddress) {
  using sandbox::GetProcessBaseAddress;
  STARTUPINFO start_info = {};
  PROCESS_INFORMATION proc_info = {};
  WCHAR command_line[] = L"notepad";
  start_info.cb = sizeof(start_info);
  start_info.dwFlags = STARTF_USESHOWWINDOW;
  start_info.wShowWindow = SW_HIDE;
  EXPECT_TRUE(::CreateProcessW(nullptr, command_line, nullptr, nullptr, FALSE,
                               CREATE_SUSPENDED, nullptr, nullptr, &start_info,
                               &proc_info));
  base::win::ScopedProcessInformation scoped_proc_info(proc_info);
  ScopedTerminateProcess process_terminate(scoped_proc_info.process_handle());
  void* base_address = GetProcessBaseAddress(scoped_proc_info.process_handle());
  EXPECT_NE(nullptr, base_address);
  EXPECT_NE(static_cast<DWORD>(-1),
            ::ResumeThread(scoped_proc_info.thread_handle()));
  ::WaitForInputIdle(scoped_proc_info.process_handle(), 1000);
  EXPECT_NE(static_cast<DWORD>(-1),
            ::SuspendThread(scoped_proc_info.thread_handle()));
  // Check again, the process will have done some more memory initialization.
  EXPECT_EQ(base_address,
            GetProcessBaseAddress(scoped_proc_info.process_handle()));

  std::vector<HMODULE> modules;
  // Compare against the loader's module list (which should now be initialized).
  // GetModuleList could fail if the target process hasn't fully initialized.
  // If so skip this check and log it as a warning.
  if (GetModuleList(scoped_proc_info.process_handle(), &modules) &&
      modules.size() > 0) {
    // First module should be the main executable.
    EXPECT_EQ(base_address, modules[0]);
  } else {
    LOG(WARNING) << "Couldn't test base address against module list";
  }
  // Fill in some of the virtual memory with 10MiB chunks and try again.
  for (int count = 0; count < 100; ++count) {
    EXPECT_NE(nullptr,
              ::VirtualAllocEx(scoped_proc_info.process_handle(), nullptr,
                               10 * 1024 * 1024, MEM_RESERVE, PAGE_NOACCESS));
  }
  EXPECT_EQ(base_address,
            GetProcessBaseAddress(scoped_proc_info.process_handle()));
}