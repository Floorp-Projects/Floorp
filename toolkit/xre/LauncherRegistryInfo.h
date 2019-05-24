/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LauncherRegistryInfo_h
#define mozilla_LauncherRegistryInfo_h

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/LauncherResult.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "nsWindowsHelpers.h"

#include <string>

/**
 * We use std::wstring here because this code must be usable within both the
 * launcher process and Gecko itself.
 */

namespace mozilla {

class LauncherRegistryInfo final {
 public:
  enum class ProcessType { Launcher, Browser };

  enum class EnabledState : uint32_t {
    Enabled = 0,
    FailDisabled = 1,
    ForceDisabled = 2,
  };

  enum class CheckOption {
    Default,
    Force,
  };

  LauncherRegistryInfo() : mBinPath(GetFullBinaryPath().get()) {}

  LauncherVoidResult ReflectPrefToRegistry(const bool aEnable);
  LauncherResult<EnabledState> IsEnabled();
  LauncherResult<bool> IsTelemetryEnabled();
  LauncherVoidResult ReflectTelemetryPrefToRegistry(const bool aEnable);
  LauncherResult<ProcessType> Check(const ProcessType aDesiredType,
                                    const CheckOption aOption =
                                      CheckOption::Default);
  LauncherVoidResult DisableDueToFailure();

 private:
  enum class Disposition { CreatedNew, OpenedExisting };

 private:
  LauncherResult<Disposition> Open();
  LauncherVoidResult WriteStartTimestamp(
      ProcessType aProcessType, const Maybe<uint64_t>& aValue = Nothing());
  LauncherResult<DWORD> GetCurrentImageTimestamp();
  LauncherVoidResult WriteImageTimestamp(DWORD aTimestamp);
  LauncherResult<bool> ClearStartTimestamp(ProcessType aProcessType);
  LauncherVoidResult ClearStartTimestamps();
  LauncherResult<DWORD> GetSavedImageTimestamp();
  LauncherResult<uint64_t> GetStartTimestamp(ProcessType aProcessType);
  LauncherResult<bool> GetTelemetrySetting();

  LauncherResult<std::wstring> ResolveValueName(ProcessType aProcessType);
  std::wstring ResolveImageTimestampValueName();
  std::wstring ResolveTelemetryValueName();

 private:
  nsAutoRegKey mRegKey;
  std::wstring mBinPath;
  std::wstring mImageValueName;
  std::wstring mBrowserValueName;
  std::wstring mLauncherValueName;
  std::wstring mTelemetryValueName;

  static const wchar_t kLauncherSubKeyPath[];
  static const wchar_t kLauncherSuffix[];
  static const wchar_t kBrowserSuffix[];
  static const wchar_t kImageTimestampSuffix[];
  static const wchar_t kTelemetrySuffix[];
};

}  // namespace mozilla

#endif  // mozilla_LauncherRegistryInfo_h
