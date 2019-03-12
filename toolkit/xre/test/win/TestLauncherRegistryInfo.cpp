/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/LauncherRegistryInfo.h"
#include "mozilla/NativeNt.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "CmdLineAndEnvUtils.h"
#include "nsWindowsHelpers.h"

#include "LauncherRegistryInfo.cpp"

#include <string>

const char kFailFmt[] = "TEST-FAILED | LauncherRegistryInfo | %s | %S\n";

static const wchar_t kRegKeyPath[] = L"SOFTWARE\\" EXPAND_STRING_MACRO(
    MOZ_APP_VENDOR) L"\\" EXPAND_STRING_MACRO(MOZ_APP_BASENAME) L"\\Launcher";
static const wchar_t kBrowserSuffix[] = L"|Browser";
static const wchar_t kLauncherSuffix[] = L"|Launcher";
static const wchar_t kImageSuffix[] = L"|Image";

static std::wstring gBrowserValue;
static std::wstring gLauncherValue;
static std::wstring gImageValue;

static DWORD gMyImageTimestamp;

using QWordResult = mozilla::Result<DWORD64, mozilla::WindowsError>;
using DWordResult = mozilla::Result<DWORD, mozilla::WindowsError>;
using VoidResult = mozilla::Result<mozilla::Ok, mozilla::WindowsError>;

#define RUN_TEST(fn)                                        \
  if ((vr = fn()).isErr()) {                                \
    printf(kFailFmt, #fn, vr.unwrapErr().AsString().get()); \
    return 1;                                               \
  }

static QWordResult GetBrowserTimestamp() {
  DWORD64 qword;
  DWORD size = sizeof(qword);
  LSTATUS status =
      ::RegGetValueW(HKEY_CURRENT_USER, kRegKeyPath, gBrowserValue.c_str(),
                     RRF_RT_QWORD, nullptr, &qword, &size);
  if (status == ERROR_SUCCESS) {
    return qword;
  }

  return mozilla::Err(mozilla::WindowsError::FromWin32Error(status));
}

static VoidResult SetBrowserTimestamp(DWORD64 aTimestamp) {
  LSTATUS status =
      ::RegSetKeyValueW(HKEY_CURRENT_USER, kRegKeyPath, gBrowserValue.c_str(),
                        REG_QWORD, &aTimestamp, sizeof(aTimestamp));
  if (status == ERROR_SUCCESS) {
    return mozilla::Ok();
  }

  return mozilla::Err(mozilla::WindowsError::FromWin32Error(status));
}

static VoidResult DeleteBrowserTimestamp() {
  LSTATUS status = ::RegDeleteKeyValueW(HKEY_CURRENT_USER, kRegKeyPath,
                                        gBrowserValue.c_str());
  if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND) {
    return mozilla::Ok();
  }

  return mozilla::Err(mozilla::WindowsError::FromWin32Error(status));
}

static QWordResult GetLauncherTimestamp() {
  DWORD64 qword;
  DWORD size = sizeof(qword);
  LSTATUS status =
      ::RegGetValueW(HKEY_CURRENT_USER, kRegKeyPath, gLauncherValue.c_str(),
                     RRF_RT_QWORD, nullptr, &qword, &size);
  if (status == ERROR_SUCCESS) {
    return qword;
  }

  return mozilla::Err(mozilla::WindowsError::FromWin32Error(status));
}

static VoidResult SetLauncherTimestamp(DWORD64 aTimestamp) {
  LSTATUS status =
      ::RegSetKeyValueW(HKEY_CURRENT_USER, kRegKeyPath, gLauncherValue.c_str(),
                        REG_QWORD, &aTimestamp, sizeof(aTimestamp));
  if (status == ERROR_SUCCESS) {
    return mozilla::Ok();
  }

  return mozilla::Err(mozilla::WindowsError::FromWin32Error(status));
}

static VoidResult DeleteLauncherTimestamp() {
  LSTATUS status = ::RegDeleteKeyValueW(HKEY_CURRENT_USER, kRegKeyPath,
                                        gLauncherValue.c_str());
  if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND) {
    return mozilla::Ok();
  }

  return mozilla::Err(mozilla::WindowsError::FromWin32Error(status));
}

static DWordResult GetImageTimestamp() {
  DWORD dword;
  DWORD size = sizeof(dword);
  LSTATUS status =
      ::RegGetValueW(HKEY_CURRENT_USER, kRegKeyPath, gImageValue.c_str(),
                     RRF_RT_DWORD, nullptr, &dword, &size);
  if (status == ERROR_SUCCESS) {
    return dword;
  }

  return mozilla::Err(mozilla::WindowsError::FromWin32Error(status));
}

static VoidResult SetImageTimestamp(DWORD aTimestamp) {
  LSTATUS status =
      ::RegSetKeyValueW(HKEY_CURRENT_USER, kRegKeyPath, gImageValue.c_str(),
                        REG_DWORD, &aTimestamp, sizeof(aTimestamp));
  if (status == ERROR_SUCCESS) {
    return mozilla::Ok();
  }

  return mozilla::Err(mozilla::WindowsError::FromWin32Error(status));
}

static VoidResult DeleteImageTimestamp() {
  LSTATUS status =
      ::RegDeleteKeyValueW(HKEY_CURRENT_USER, kRegKeyPath, gImageValue.c_str());
  if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND) {
    return mozilla::Ok();
  }

  return mozilla::Err(mozilla::WindowsError::FromWin32Error(status));
}

static VoidResult DeleteAllTimestamps() {
  VoidResult vr = DeleteBrowserTimestamp();
  if (vr.isErr()) {
    return vr;
  }

  vr = DeleteLauncherTimestamp();
  if (vr.isErr()) {
    return vr;
  }

  return DeleteImageTimestamp();
}

static VoidResult SetupEnabledScenario() {
  // Reset the registry state to an enabled state. First, we delete all existing
  // timestamps (if any).
  VoidResult vr = DeleteAllTimestamps();
  if (vr.isErr()) {
    return vr;
  }

  // Now we run Check(Launcher)...
  mozilla::LauncherRegistryInfo info;
  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::ProcessType> result =
      info.Check(mozilla::LauncherRegistryInfo::ProcessType::Launcher);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr().mError);
  }

  if (result.unwrap() != mozilla::LauncherRegistryInfo::ProcessType::Launcher) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  // ...and Check(Browser)
  result = info.Check(mozilla::LauncherRegistryInfo::ProcessType::Browser);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr().mError);
  }

  if (result.unwrap() != mozilla::LauncherRegistryInfo::ProcessType::Browser) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  // By this point we are considered to be fully enabled.

  return mozilla::Ok();
}

static VoidResult TestEmptyRegistry() {
  VoidResult vr = DeleteAllTimestamps();
  if (vr.isErr()) {
    return vr;
  }

  mozilla::LauncherRegistryInfo info;
  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::ProcessType> result =
      info.Check(mozilla::LauncherRegistryInfo::ProcessType::Launcher);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr().mError);
  }

  if (result.unwrap() != mozilla::LauncherRegistryInfo::ProcessType::Launcher) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  // LauncherRegistryInfo should have created Launcher and Image values
  QWordResult launcherTs = GetLauncherTimestamp();
  if (launcherTs.isErr()) {
    return mozilla::Err(launcherTs.unwrapErr());
  }

  DWordResult imageTs = GetImageTimestamp();
  if (imageTs.isErr()) {
    return mozilla::Err(imageTs.unwrapErr());
  }

  if (imageTs.unwrap() != gMyImageTimestamp) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  QWordResult browserTs = GetBrowserTimestamp();
  if (browserTs.isOk()) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  if (browserTs.unwrapErr() !=
      mozilla::WindowsError::FromWin32Error(ERROR_FILE_NOT_FOUND)) {
    return mozilla::Err(browserTs.unwrapErr());
  }

  return mozilla::Ok();
}

// This test depends on the side effects from having just run TestEmptyRegistry
static VoidResult TestNormal() {
  mozilla::LauncherRegistryInfo info;
  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::ProcessType> result =
      info.Check(mozilla::LauncherRegistryInfo::ProcessType::Browser);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr().mError);
  }

  QWordResult launcherTs = GetLauncherTimestamp();
  if (launcherTs.isErr()) {
    return mozilla::Err(launcherTs.unwrapErr());
  }

  QWordResult browserTs = GetBrowserTimestamp();
  if (browserTs.isErr()) {
    return mozilla::Err(browserTs.unwrapErr());
  }

  if (browserTs.unwrap() < launcherTs.unwrap()) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::EnabledState> enabled =
      info.IsEnabled();
  if (enabled.isErr()) {
    return mozilla::Err(enabled.unwrapErr().mError);
  }

  if (enabled.unwrap() !=
      mozilla::LauncherRegistryInfo::EnabledState::Enabled) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  return mozilla::Ok();
}

// This test depends on the side effects from having just run TestNormal
static VoidResult TestBrowserNoLauncher() {
  VoidResult vr = DeleteLauncherTimestamp();
  if (vr.isErr()) {
    return vr;
  }

  mozilla::LauncherRegistryInfo info;
  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::ProcessType> result =
      info.Check(mozilla::LauncherRegistryInfo::ProcessType::Launcher);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr().mError);
  }

  if (result.unwrap() != mozilla::LauncherRegistryInfo::ProcessType::Browser) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  // Verify that we still don't have a launcher timestamp
  QWordResult launcherTs = GetLauncherTimestamp();
  if (launcherTs.isOk()) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  if (launcherTs.unwrapErr() !=
      mozilla::WindowsError::FromWin32Error(ERROR_FILE_NOT_FOUND)) {
    return mozilla::Err(launcherTs.unwrapErr());
  }

  // Verify that the browser timestamp is now zero
  QWordResult browserTs = GetBrowserTimestamp();
  if (browserTs.isErr()) {
    return mozilla::Err(browserTs.unwrapErr());
  }

  if (browserTs.unwrap() != 0ULL) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::EnabledState> enabled =
      info.IsEnabled();
  if (enabled.isErr()) {
    return mozilla::Err(enabled.unwrapErr().mError);
  }

  if (enabled.unwrap() !=
      mozilla::LauncherRegistryInfo::EnabledState::ForceDisabled) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  return mozilla::Ok();
}

static VoidResult TestLauncherNoBrowser() {
  VoidResult vr = DeleteAllTimestamps();
  if (vr.isErr()) {
    return vr;
  }

  vr = SetLauncherTimestamp(0x77777777);
  if (vr.isErr()) {
    return vr;
  }

  vr = SetImageTimestamp(gMyImageTimestamp);
  if (vr.isErr()) {
    return vr;
  }

  mozilla::LauncherRegistryInfo info;
  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::ProcessType> result =
      info.Check(mozilla::LauncherRegistryInfo::ProcessType::Launcher);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr().mError);
  }

  if (result.unwrap() != mozilla::LauncherRegistryInfo::ProcessType::Browser) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::EnabledState> enabled =
      info.IsEnabled();
  if (enabled.isErr()) {
    return mozilla::Err(enabled.unwrapErr().mError);
  }

  if (enabled.unwrap() !=
      mozilla::LauncherRegistryInfo::EnabledState::FailDisabled) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  return mozilla::Ok();
}

static VoidResult TestBrowserLessThanLauncher() {
  VoidResult vr = SetLauncherTimestamp(0x77777777);
  if (vr.isErr()) {
    return vr;
  }

  vr = SetBrowserTimestamp(0ULL);
  if (vr.isErr()) {
    return vr;
  }

  mozilla::LauncherRegistryInfo info;
  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::ProcessType> result =
      info.Check(mozilla::LauncherRegistryInfo::ProcessType::Launcher);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr().mError);
  }

  if (result.unwrap() != mozilla::LauncherRegistryInfo::ProcessType::Browser) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::EnabledState> enabled =
      info.IsEnabled();
  if (enabled.isErr()) {
    return mozilla::Err(enabled.unwrapErr().mError);
  }

  if (enabled.unwrap() !=
      mozilla::LauncherRegistryInfo::EnabledState::FailDisabled) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  return mozilla::Ok();
}

static VoidResult TestImageTimestampChange() {
  // This should reset the timestamps and then essentially run like
  // TestEmptyRegistry
  VoidResult vr = SetImageTimestamp(0x12345678);
  if (vr.isErr()) {
    return vr;
  }

  vr = SetLauncherTimestamp(1ULL);
  if (vr.isErr()) {
    return vr;
  }

  vr = SetBrowserTimestamp(2ULL);
  if (vr.isErr()) {
    return vr;
  }

  mozilla::LauncherRegistryInfo info;
  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::ProcessType> result =
      info.Check(mozilla::LauncherRegistryInfo::ProcessType::Launcher);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr().mError);
  }

  if (result.unwrap() != mozilla::LauncherRegistryInfo::ProcessType::Launcher) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  QWordResult launcherTs = GetLauncherTimestamp();
  if (launcherTs.isErr()) {
    return mozilla::Err(launcherTs.unwrapErr());
  }

  DWordResult imageTs = GetImageTimestamp();
  if (imageTs.isErr()) {
    return mozilla::Err(imageTs.unwrapErr());
  }

  if (imageTs.unwrap() != gMyImageTimestamp) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  QWordResult browserTs = GetBrowserTimestamp();
  if (browserTs.isOk()) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  if (browserTs.unwrapErr() !=
      mozilla::WindowsError::FromWin32Error(ERROR_FILE_NOT_FOUND)) {
    return mozilla::Err(browserTs.unwrapErr());
  }

  return mozilla::Ok();
}

static VoidResult TestImageTimestampChangeWhenDisabled() {
  VoidResult vr = SetImageTimestamp(0x12345678);
  if (vr.isErr()) {
    return vr;
  }

  vr = DeleteLauncherTimestamp();
  if (vr.isErr()) {
    return vr;
  }

  vr = SetBrowserTimestamp(0ULL);
  if (vr.isErr()) {
    return vr;
  }

  mozilla::LauncherRegistryInfo info;
  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::ProcessType> result =
      info.Check(mozilla::LauncherRegistryInfo::ProcessType::Launcher);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr().mError);
  }

  if (result.unwrap() != mozilla::LauncherRegistryInfo::ProcessType::Browser) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::EnabledState> enabled =
      info.IsEnabled();
  if (enabled.isErr()) {
    return mozilla::Err(enabled.unwrapErr().mError);
  }

  if (enabled.unwrap() !=
      mozilla::LauncherRegistryInfo::EnabledState::ForceDisabled) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  return mozilla::Ok();
}

static VoidResult TestDisableDueToFailure() {
  VoidResult vr = SetupEnabledScenario();
  if (vr.isErr()) {
    return vr;
  }

  // Check that we are indeed enabled.
  mozilla::LauncherRegistryInfo info;
  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::EnabledState> enabled =
      info.IsEnabled();
  if (enabled.isErr()) {
    return mozilla::Err(enabled.unwrapErr().mError);
  }

  if (enabled.unwrap() !=
      mozilla::LauncherRegistryInfo::EnabledState::Enabled) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  // Now call DisableDueToFailure
  mozilla::LauncherVoidResult lvr = info.DisableDueToFailure();
  if (lvr.isErr()) {
    return mozilla::Err(lvr.unwrapErr().mError);
  }

  // We should now be FailDisabled
  enabled = info.IsEnabled();
  if (enabled.isErr()) {
    return mozilla::Err(enabled.unwrapErr().mError);
  }

  if (enabled.unwrap() !=
      mozilla::LauncherRegistryInfo::EnabledState::FailDisabled) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  // If we delete the launcher timestamp, IsEnabled should then return
  // ForceDisabled.
  vr = DeleteLauncherTimestamp();
  if (vr.isErr()) {
    return vr;
  }

  enabled = info.IsEnabled();
  if (enabled.isErr()) {
    return mozilla::Err(enabled.unwrapErr().mError);
  }

  if (enabled.unwrap() !=
      mozilla::LauncherRegistryInfo::EnabledState::ForceDisabled) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  return mozilla::Ok();
}

static VoidResult TestPrefReflection() {
  // Reset the registry to a known good state.
  VoidResult vr = SetupEnabledScenario();
  if (vr.isErr()) {
    return vr;
  }

  // Let's see what happens when we flip the pref to OFF.
  mozilla::LauncherRegistryInfo info;
  mozilla::LauncherVoidResult reflectOk = info.ReflectPrefToRegistry(false);
  if (reflectOk.isErr()) {
    return mozilla::Err(reflectOk.unwrapErr().mError);
  }

  // Launcher timestamp should be non-existent.
  QWordResult launcherTs = GetLauncherTimestamp();
  if (launcherTs.isOk()) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  if (launcherTs.unwrapErr() !=
      mozilla::WindowsError::FromWin32Error(ERROR_FILE_NOT_FOUND)) {
    return mozilla::Err(launcherTs.unwrapErr());
  }

  // Browser timestamp should be zero
  QWordResult browserTs = GetBrowserTimestamp();
  if (browserTs.isErr()) {
    return mozilla::Err(browserTs.unwrapErr());
  }

  if (browserTs.unwrap() != 0ULL) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  // IsEnabled should give us ForceDisabled
  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::EnabledState> enabled =
      info.IsEnabled();
  if (enabled.isErr()) {
    return mozilla::Err(enabled.unwrapErr().mError);
  }

  if (enabled.unwrap() !=
      mozilla::LauncherRegistryInfo::EnabledState::ForceDisabled) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  // Now test to see what happens when the pref is set to ON.
  reflectOk = info.ReflectPrefToRegistry(true);
  if (reflectOk.isErr()) {
    return mozilla::Err(reflectOk.unwrapErr().mError);
  }

  // Launcher and browser timestamps should be non-existent.
  launcherTs = GetLauncherTimestamp();
  if (launcherTs.isOk()) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  if (launcherTs.unwrapErr() !=
      mozilla::WindowsError::FromWin32Error(ERROR_FILE_NOT_FOUND)) {
    return mozilla::Err(launcherTs.unwrapErr());
  }

  browserTs = GetBrowserTimestamp();
  if (browserTs.isOk()) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_UNEXPECTED));
  }

  if (browserTs.unwrapErr() !=
      mozilla::WindowsError::FromWin32Error(ERROR_FILE_NOT_FOUND)) {
    return mozilla::Err(browserTs.unwrapErr());
  }

  // IsEnabled should give us Enabled.
  enabled = info.IsEnabled();
  if (enabled.isErr()) {
    return mozilla::Err(enabled.unwrapErr().mError);
  }

  if (enabled.unwrap() !=
      mozilla::LauncherRegistryInfo::EnabledState::Enabled) {
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
  }

  return mozilla::Ok();
}

int main(int argc, char* argv[]) {
  auto fullPath = mozilla::GetFullBinaryPath();
  if (!fullPath) {
    return 1;
  }

  // Global setup for all tests
  gBrowserValue = fullPath.get();
  gBrowserValue += kBrowserSuffix;

  gLauncherValue = fullPath.get();
  gLauncherValue += kLauncherSuffix;

  gImageValue = fullPath.get();
  gImageValue += kImageSuffix;

  mozilla::nt::PEHeaders myHeaders(::GetModuleHandleW(nullptr));
  if (!myHeaders) {
    return 1;
  }

  if (!myHeaders.GetTimeStamp(gMyImageTimestamp)) {
    return 1;
  }

  auto onExit = mozilla::MakeScopeExit(
      []() { mozilla::Unused << DeleteAllTimestamps(); });

  VoidResult vr = mozilla::Ok();

  RUN_TEST(TestEmptyRegistry);
  RUN_TEST(TestNormal);
  RUN_TEST(TestBrowserNoLauncher);
  RUN_TEST(TestLauncherNoBrowser);
  RUN_TEST(TestBrowserLessThanLauncher);
  RUN_TEST(TestImageTimestampChange);
  RUN_TEST(TestImageTimestampChangeWhenDisabled);
  RUN_TEST(TestDisableDueToFailure);
  RUN_TEST(TestPrefReflection);

  return 0;
}
