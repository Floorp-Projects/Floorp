/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <winnetwk.h>

#include "mozilla/FileUtilsWin.h"
#include "mozilla/DebugOnly.h"
#include "nsCRTGlue.h"

#include "gtest/gtest.h"

class DriveMapping {
 public:
  explicit DriveMapping(const nsAString& aRemoteUNCPath);
  ~DriveMapping();

  bool Init();
  bool ChangeDriveLetter();
  wchar_t GetDriveLetter() { return mDriveLetter; }

 private:
  bool DoMapping();
  void Disconnect(wchar_t aDriveLetter);

  wchar_t mDriveLetter;
  nsString mRemoteUNCPath;
};

DriveMapping::DriveMapping(const nsAString& aRemoteUNCPath)
    : mDriveLetter(0), mRemoteUNCPath(aRemoteUNCPath) {}

bool DriveMapping::Init() {
  if (mDriveLetter) {
    return false;
  }
  return DoMapping();
}

bool DriveMapping::DoMapping() {
  wchar_t drvTemplate[] = L" :";
  NETRESOURCEW netRes = {0};
  netRes.dwType = RESOURCETYPE_DISK;
  netRes.lpLocalName = drvTemplate;
  netRes.lpRemoteName =
      reinterpret_cast<wchar_t*>(mRemoteUNCPath.BeginWriting());
  wchar_t driveLetter = L'D';
  DWORD result = NO_ERROR;
  do {
    drvTemplate[0] = driveLetter;
    result = WNetAddConnection2W(&netRes, nullptr, nullptr, CONNECT_TEMPORARY);
  } while (result == ERROR_ALREADY_ASSIGNED && ++driveLetter <= L'Z');
  if (result != NO_ERROR) {
    return false;
  }
  mDriveLetter = driveLetter;
  return true;
}

bool DriveMapping::ChangeDriveLetter() {
  wchar_t prevDriveLetter = mDriveLetter;
  bool result = DoMapping();
  MOZ_RELEASE_ASSERT(mDriveLetter != prevDriveLetter);
  if (result && prevDriveLetter) {
    Disconnect(prevDriveLetter);
  }
  return result;
}

void DriveMapping::Disconnect(wchar_t aDriveLetter) {
  wchar_t drvTemplate[] = {aDriveLetter, L':', L'\0'};
  DWORD result = WNetCancelConnection2W(drvTemplate, 0, TRUE);
  MOZ_RELEASE_ASSERT(result == NO_ERROR);
}

DriveMapping::~DriveMapping() {
  if (mDriveLetter) {
    Disconnect(mDriveLetter);
  }
}

bool DriveToNtPath(const wchar_t aDriveLetter, nsAString& aNtPath) {
  const wchar_t drvTpl[] = {aDriveLetter, L':', L'\0'};
  aNtPath.SetLength(MAX_PATH);
  DWORD pathLen;
  while (true) {
    pathLen = QueryDosDeviceW(
        drvTpl, reinterpret_cast<wchar_t*>(aNtPath.BeginWriting()),
        aNtPath.Length());
    if (pathLen || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      break;
    }
    aNtPath.SetLength(aNtPath.Length() * 2);
  }
  if (!pathLen) {
    return false;
  }
  // aNtPath contains embedded NULLs, so we need to figure out the real length
  // via wcslen.
  aNtPath.SetLength(NS_strlen(aNtPath.BeginReading()));
  return true;
}

bool TestNtPathToDosPath(const wchar_t* aNtPath,
                         const wchar_t* aExpectedDosPath) {
  nsAutoString output;
  bool result = mozilla::NtPathToDosPath(nsDependentString(aNtPath), output);
  return result && output == reinterpret_cast<const nsAString::char_type*>(
                                 aExpectedDosPath);
}

TEST(NtPathToDosPath, Tests)
{
  nsAutoString cDrive;
  ASSERT_TRUE(DriveToNtPath(L'C', cDrive));

  // empty string
  EXPECT_TRUE(TestNtPathToDosPath(L"", L""));

  // non-existent device, must fail
  EXPECT_FALSE(
      TestNtPathToDosPath(L"\\Device\\ThisDeviceDoesNotExist\\Foo", nullptr));

  // base case
  nsAutoString testPath(cDrive);
  testPath.Append(L"\\Program Files");
  EXPECT_TRUE(TestNtPathToDosPath(testPath.get(), L"C:\\Program Files"));

  // short filename
  nsAutoString ntShortName(cDrive);
  ntShortName.Append(L"\\progra~1");
  EXPECT_TRUE(TestNtPathToDosPath(ntShortName.get(), L"C:\\Program Files"));

  // drive letters as symbolic links (NtCreateFile uses these)
  EXPECT_TRUE(TestNtPathToDosPath(L"\\??\\C:\\Foo", L"C:\\Foo"));

  // other symbolic links (should fail)
  EXPECT_FALSE(TestNtPathToDosPath(L"\\??\\MountPointManager", nullptr));

  // socket (should fail)
  EXPECT_FALSE(TestNtPathToDosPath(L"\\Device\\Afd\\Endpoint", nullptr));

  // UNC path (using MUP)
  EXPECT_TRUE(TestNtPathToDosPath(L"\\Device\\Mup\\127.0.0.1\\C$",
                                  L"\\\\127.0.0.1\\C$"));

  // UNC path (using LanmanRedirector)
  EXPECT_TRUE(TestNtPathToDosPath(L"\\Device\\LanmanRedirector\\127.0.0.1\\C$",
                                  L"\\\\127.0.0.1\\C$"));

  DriveMapping drvMapping(NS_LITERAL_STRING("\\\\127.0.0.1\\C$"));
  // Only run these tests if we were able to map; some machines don't have perms
  if (drvMapping.Init()) {
    wchar_t expected[] = L" :\\";
    expected[0] = drvMapping.GetDriveLetter();
    nsAutoString networkPath;
    ASSERT_TRUE(DriveToNtPath(drvMapping.GetDriveLetter(), networkPath));

    networkPath += u"\\";
    EXPECT_TRUE(TestNtPathToDosPath(networkPath.get(), expected));

    // NtPathToDosPath must correctly handle paths whose drive letter mapping
    // has changed. We need to test this because the APIs called by
    // NtPathToDosPath return different info if this has happened.
    ASSERT_TRUE(drvMapping.ChangeDriveLetter());

    expected[0] = drvMapping.GetDriveLetter();
    ASSERT_TRUE(DriveToNtPath(drvMapping.GetDriveLetter(), networkPath));

    networkPath += u"\\";
    EXPECT_TRUE(TestNtPathToDosPath(networkPath.get(), expected));
  }
}
