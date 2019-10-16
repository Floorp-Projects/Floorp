/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LauncherRegistryInfo_h
#define mozilla_LauncherRegistryInfo_h

#include "mozilla/Maybe.h"
#include "mozilla/WinHeaderOnlyUtils.h"
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

  enum class EnabledState {
    Enabled,
    FailDisabled,
    ForceDisabled,
  };

  enum class CheckOption {
    Default,
    Force,
  };

  LauncherRegistryInfo() : mBinPath(GetFullBinaryPath().get()) {}
  ~LauncherRegistryInfo() { Abort(); }

  LauncherVoidResult ReflectPrefToRegistry(const bool aEnable);
  LauncherResult<EnabledState> IsEnabled();
  LauncherResult<bool> IsTelemetryEnabled();
  LauncherVoidResult ReflectTelemetryPrefToRegistry(const bool aEnable);
  LauncherResult<ProcessType> Check(
      const ProcessType aDesiredType,
      const CheckOption aOption = CheckOption::Default);
  LauncherVoidResult DisableDueToFailure();
  LauncherVoidResult Commit();
  void Abort();

 private:
  enum class Disposition { CreatedNew, OpenedExisting };

 private:
  // This flag is to prevent the disabled state from being accidentally
  // re-enabled by another instance.
  static bool sAllowCommit;

  static EnabledState GetEnabledState(const Maybe<uint64_t>& aLauncherTs,
                                      const Maybe<uint64_t>& aBrowserTs);

  LauncherResult<Disposition> Open();
  LauncherVoidResult WriteLauncherStartTimestamp(uint64_t aValue);
  LauncherVoidResult WriteBrowserStartTimestamp(uint64_t aValue);
  LauncherVoidResult WriteImageTimestamp(DWORD aTimestamp);
  LauncherResult<bool> ClearLauncherStartTimestamp();
  LauncherResult<bool> ClearBrowserStartTimestamp();
  LauncherVoidResult ClearStartTimestamps();
  LauncherResult<Maybe<DWORD>> GetSavedImageTimestamp();
  LauncherResult<Maybe<uint64_t>> GetLauncherStartTimestamp();
  LauncherResult<Maybe<uint64_t>> GetBrowserStartTimestamp();

  const std::wstring& ResolveLauncherValueName();
  const std::wstring& ResolveBrowserValueName();
  const std::wstring& ResolveImageTimestampValueName();
  const std::wstring& ResolveTelemetryValueName();

 private:
  Maybe<uint64_t> mLauncherTimestampToWrite;
  Maybe<uint64_t> mBrowserTimestampToWrite;

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
