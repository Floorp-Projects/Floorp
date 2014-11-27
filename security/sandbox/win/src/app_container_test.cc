// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#define _ATL_NO_EXCEPTIONS
#include <atlbase.h>
#include <atlsecurity.h>

#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/sync_policy_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t kAppContainerName[] = L"sbox_test";
const wchar_t kAppContainerSid[] =
    L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
    L"924012148-2839372144";

const ULONG kSharing = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;

HANDLE CreateTaggedEvent(const base::string16& name,
                         const base::string16& sid) {
  base::win::ScopedHandle event(CreateEvent(NULL, FALSE, FALSE, name.c_str()));
  if (!event.IsValid())
    return NULL;

  wchar_t file_name[MAX_PATH] = {};
  wchar_t temp_directory[MAX_PATH] = {};
  GetTempPath(MAX_PATH, temp_directory);
  GetTempFileName(temp_directory, L"test", 0, file_name);

  base::win::ScopedHandle file;
  file.Set(CreateFile(file_name, GENERIC_READ | STANDARD_RIGHTS_READ, kSharing,
                      NULL, OPEN_EXISTING, 0, NULL));
  DeleteFile(file_name);
  if (!file.IsValid())
    return NULL;

  CSecurityDesc sd;
  if (!AtlGetSecurityDescriptor(file.Get(), SE_FILE_OBJECT, &sd,
                                OWNER_SECURITY_INFORMATION |
                                    GROUP_SECURITY_INFORMATION |
                                    DACL_SECURITY_INFORMATION)) {
    return NULL;
  }

  PSID local_sid;
  if (!ConvertStringSidToSid(sid.c_str(), &local_sid))
    return NULL;

  CDacl new_dacl;
  sd.GetDacl(&new_dacl);
  CSid csid(reinterpret_cast<SID*>(local_sid));
  new_dacl.AddAllowedAce(csid, EVENT_ALL_ACCESS);
  if (!AtlSetDacl(event.Get(), SE_KERNEL_OBJECT, new_dacl))
    event.Close();

  LocalFree(local_sid);
  return event.IsValid() ? event.Take() : NULL;
}

}  // namespace

namespace sandbox {

TEST(AppContainerTest, AllowOpenEvent) {
  if (base::win::OSInfo::GetInstance()->version() < base::win::VERSION_WIN8)
    return;

  TestRunner runner(JOB_UNPROTECTED, USER_UNPROTECTED, USER_UNPROTECTED);

  const wchar_t capability[] = L"S-1-15-3-12345678-87654321";
  base::win::ScopedHandle handle(CreateTaggedEvent(L"test", capability));
  ASSERT_TRUE(handle.IsValid());

  EXPECT_EQ(SBOX_ALL_OK,
            runner.broker()->InstallAppContainer(kAppContainerSid,
                                                 kAppContainerName));
  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetCapability(capability));
  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetAppContainer(kAppContainerSid));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_Open f test"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Event_Open f test"));
  EXPECT_EQ(SBOX_ALL_OK,
            runner.broker()->UninstallAppContainer(kAppContainerSid));
}

TEST(AppContainerTest, DenyOpenEvent) {
  if (base::win::OSInfo::GetInstance()->version() < base::win::VERSION_WIN8)
    return;

  TestRunner runner(JOB_UNPROTECTED, USER_UNPROTECTED, USER_UNPROTECTED);

  const wchar_t capability[] = L"S-1-15-3-12345678-87654321";
  base::win::ScopedHandle handle(CreateTaggedEvent(L"test", capability));
  ASSERT_TRUE(handle.IsValid());

  EXPECT_EQ(SBOX_ALL_OK,
            runner.broker()->InstallAppContainer(kAppContainerSid,
                                                 kAppContainerName));
  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetAppContainer(kAppContainerSid));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_Open f test"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_Open f test"));
  EXPECT_EQ(SBOX_ALL_OK,
            runner.broker()->UninstallAppContainer(kAppContainerSid));
}

TEST(AppContainerTest, NoImpersonation) {
  if (base::win::OSInfo::GetInstance()->version() < base::win::VERSION_WIN8)
    return;

  TestRunner runner(JOB_UNPROTECTED, USER_LIMITED, USER_LIMITED);
  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetAppContainer(kAppContainerSid));
}

TEST(AppContainerTest, WantsImpersonation) {
  if (base::win::OSInfo::GetInstance()->version() < base::win::VERSION_WIN8)
    return;

  TestRunner runner(JOB_UNPROTECTED, USER_UNPROTECTED, USER_NON_ADMIN);
  EXPECT_EQ(SBOX_ERROR_CANNOT_INIT_APPCONTAINER,
            runner.GetPolicy()->SetAppContainer(kAppContainerSid));
}

TEST(AppContainerTest, RequiresImpersonation) {
  if (base::win::OSInfo::GetInstance()->version() < base::win::VERSION_WIN8)
    return;

  TestRunner runner(JOB_UNPROTECTED, USER_RESTRICTED, USER_RESTRICTED);
  EXPECT_EQ(SBOX_ERROR_CANNOT_INIT_APPCONTAINER,
            runner.GetPolicy()->SetAppContainer(kAppContainerSid));
}

}  // namespace sandbox
