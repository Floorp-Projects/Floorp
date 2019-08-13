/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "LauncherRegistryInfo.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/NativeNt.h"

#include <string>

#define EXPAND_STRING_MACRO2(t) t
#define EXPAND_STRING_MACRO(t) EXPAND_STRING_MACRO2(t)

namespace mozilla {

const wchar_t LauncherRegistryInfo::kLauncherSubKeyPath[] =
    L"SOFTWARE\\" EXPAND_STRING_MACRO(MOZ_APP_VENDOR) L"\\" EXPAND_STRING_MACRO(
        MOZ_APP_BASENAME) L"\\Launcher";
const wchar_t LauncherRegistryInfo::kLauncherSuffix[] = L"|Launcher";
const wchar_t LauncherRegistryInfo::kBrowserSuffix[] = L"|Browser";
const wchar_t LauncherRegistryInfo::kImageTimestampSuffix[] = L"|Image";
const wchar_t LauncherRegistryInfo::kTelemetrySuffix[] = L"|Telemetry";

LauncherResult<LauncherRegistryInfo::Disposition> LauncherRegistryInfo::Open() {
  if (!!mRegKey) {
    return Disposition::OpenedExisting;
  }

  DWORD disposition;
  HKEY rawKey;
  LSTATUS result = ::RegCreateKeyExW(
      HKEY_CURRENT_USER, kLauncherSubKeyPath, 0, nullptr,
      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &rawKey, &disposition);
  if (result != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(result);
  }

  mRegKey.own(rawKey);

  switch (disposition) {
    case REG_CREATED_NEW_KEY:
      return Disposition::CreatedNew;
    case REG_OPENED_EXISTING_KEY:
      return Disposition::OpenedExisting;
    default:
      break;
  }

  MOZ_ASSERT_UNREACHABLE("Invalid disposition from RegCreateKeyExW");
  return LAUNCHER_ERROR_GENERIC();
}

LauncherVoidResult LauncherRegistryInfo::ReflectPrefToRegistry(
    const bool aEnable) {
  LauncherResult<EnabledState> curEnabledState = IsEnabled();
  if (curEnabledState.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(curEnabledState);
  }

  bool isCurrentlyEnabled =
      curEnabledState.inspect() != EnabledState::ForceDisabled;
  if (isCurrentlyEnabled == aEnable) {
    // Don't reflect to the registry unless the new enabled state is actually
    // changing with respect to the current enabled state.
    return Ok();
  }

  // Always delete the launcher timestamp
  LauncherResult<bool> clearedLauncherTimestamp =
      ClearStartTimestamp(ProcessType::Launcher);
  MOZ_ASSERT(clearedLauncherTimestamp.isOk());
  if (clearedLauncherTimestamp.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(clearedLauncherTimestamp);
  }

  if (!aEnable) {
    // Set the browser timestamp to 0 to indicate force-disabled
    return WriteStartTimestamp(ProcessType::Browser, Some(0ULL));
  }

  // Otherwise we delete the browser timestamp to start over fresh
  LauncherResult<bool> clearedBrowserTimestamp =
      ClearStartTimestamp(ProcessType::Browser);
  MOZ_ASSERT(clearedBrowserTimestamp.isOk());
  if (clearedBrowserTimestamp.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(clearedBrowserTimestamp);
  }

  return Ok();
}

LauncherVoidResult LauncherRegistryInfo::ReflectTelemetryPrefToRegistry(
    const bool aEnable) {
  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(disposition);
  }

  std::wstring valueName(ResolveTelemetryValueName());

  DWORD value = aEnable ? 1UL : 0UL;
  DWORD len = sizeof(value);
  LSTATUS result =
      ::RegSetValueExW(mRegKey.get(), valueName.c_str(), 0, REG_DWORD,
                       reinterpret_cast<PBYTE>(&value), len);
  if (result != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(result);
  }

  return Ok();
}

LauncherResult<LauncherRegistryInfo::ProcessType> LauncherRegistryInfo::Check(
    const ProcessType aDesiredType, const CheckOption aOption) {
  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(disposition);
  }

  LauncherResult<DWORD> ourImageTimestamp = GetCurrentImageTimestamp();
  if (ourImageTimestamp.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(ourImageTimestamp);
  }

  LauncherResult<DWORD> savedImageTimestamp = GetSavedImageTimestamp();
  if (savedImageTimestamp.isErr() &&
      savedImageTimestamp.unwrapErr() !=
          WindowsError::FromWin32Error(ERROR_FILE_NOT_FOUND)) {
    return LAUNCHER_ERROR_FROM_RESULT(savedImageTimestamp);
  }

  // If we don't have a saved timestamp, or we do but it doesn't match with
  // our current timestamp, clear previous values.
  if (savedImageTimestamp.isErr() ||
      savedImageTimestamp.inspect() != ourImageTimestamp.inspect()) {
    LauncherVoidResult clearResult = ClearStartTimestamps();
    if (clearResult.isErr()) {
      return LAUNCHER_ERROR_FROM_RESULT(clearResult);
    }

    LauncherVoidResult writeResult =
        WriteImageTimestamp(ourImageTimestamp.inspect());
    if (writeResult.isErr()) {
      return LAUNCHER_ERROR_FROM_RESULT(writeResult);
    }
  }

  ProcessType typeToRunAs = aDesiredType;

  if (disposition.inspect() == Disposition::CreatedNew ||
      aDesiredType == ProcessType::Browser) {
    // No existing values to check, or we're going to be running as the browser
    // process: just write our timestamp and return
    LauncherVoidResult wroteTimestamp = WriteStartTimestamp(typeToRunAs);
    if (wroteTimestamp.isErr()) {
      return LAUNCHER_ERROR_FROM_RESULT(wroteTimestamp);
    }

    return typeToRunAs;
  }

  LauncherResult<uint64_t> lastLauncherTimestamp =
      GetStartTimestamp(ProcessType::Launcher);
  bool haveLauncherTs = lastLauncherTimestamp.isOk();

  LauncherResult<uint64_t> lastBrowserTimestamp =
      GetStartTimestamp(ProcessType::Browser);
  bool haveBrowserTs = lastBrowserTimestamp.isOk();

  if (haveLauncherTs != haveBrowserTs) {
    // If we have a launcher timestamp but no browser timestamp (or vice versa),
    // that's bad because it is indicating that the browser can't run with
    // the launcher process.
    typeToRunAs = ProcessType::Browser;
  } else if (haveLauncherTs) {
    // if we have both timestamps, we want to ensure that the launcher timestamp
    // is earlier than the browser timestamp.
    if (aDesiredType == ProcessType::Launcher) {
      bool areTimestampsOk =
          lastLauncherTimestamp.inspect() < lastBrowserTimestamp.inspect();
      if (!areTimestampsOk) {
        typeToRunAs = ProcessType::Browser;
      }
    }
  } else {
    // If we have neither timestamp, then we should try running as suggested
    // by |aDesiredType|.
    // We shouldn't really have this scenario unless we're going to be running
    // as the launcher process.
    MOZ_ASSERT(typeToRunAs == ProcessType::Launcher);
    // No change to typeToRunAs
  }

  // Debugging setting that forces the desired type regardless of the various
  // tests that have been performed.
  if (aOption == CheckOption::Force) {
    typeToRunAs = aDesiredType;
  }

  LauncherVoidResult wroteTimestamp = Ok();

  if (typeToRunAs == ProcessType::Browser && aDesiredType != typeToRunAs) {
    // We were hoping to run as the launcher, but some failure has caused us
    // to run as the browser. Set the browser timestamp to zero as an indicator.
    wroteTimestamp = WriteStartTimestamp(typeToRunAs, Some(0ULL));
  } else {
    wroteTimestamp = WriteStartTimestamp(typeToRunAs);
  }

  if (wroteTimestamp.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(wroteTimestamp);
  }

  return typeToRunAs;
}

LauncherVoidResult LauncherRegistryInfo::DisableDueToFailure() {
  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(disposition);
  }

  return WriteStartTimestamp(ProcessType::Browser, Some(0ULL));
}

LauncherResult<LauncherRegistryInfo::EnabledState>
LauncherRegistryInfo::IsEnabled() {
  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(disposition);
  }

  LauncherResult<uint64_t> launcherTimestamp =
      GetStartTimestamp(ProcessType::Launcher);

  LauncherResult<uint64_t> browserTimestamp =
      GetStartTimestamp(ProcessType::Browser);

  // In this function, we'll explictly search for the ForceDisabled and
  // Enabled conditions. Everything else is FailDisabled.

  bool isBrowserTimestampZero =
      browserTimestamp.isOk() && browserTimestamp.inspect() == 0ULL;

  if (isBrowserTimestampZero && launcherTimestamp.isErr()) {
    return EnabledState::ForceDisabled;
  }

  if (launcherTimestamp.isOk() && browserTimestamp.isOk() &&
      launcherTimestamp.inspect() < browserTimestamp.inspect()) {
    return EnabledState::Enabled;
  }

  if (launcherTimestamp.isErr() && browserTimestamp.isErr()) {
    return EnabledState::Enabled;
  }

  return EnabledState::FailDisabled;
}

LauncherResult<bool> LauncherRegistryInfo::IsTelemetryEnabled() {
  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(disposition);
  }

  return GetTelemetrySetting();
}

LauncherResult<std::wstring> LauncherRegistryInfo::ResolveValueName(
    LauncherRegistryInfo::ProcessType aProcessType) {
  if (aProcessType == ProcessType::Launcher) {
    if (mLauncherValueName.empty()) {
      mLauncherValueName.assign(mBinPath);
      mLauncherValueName.append(kLauncherSuffix,
                                ArrayLength(kLauncherSuffix) - 1);
    }

    return mLauncherValueName;
  } else if (aProcessType == ProcessType::Browser) {
    if (mBrowserValueName.empty()) {
      mBrowserValueName.assign(mBinPath);
      mBrowserValueName.append(kBrowserSuffix, ArrayLength(kBrowserSuffix) - 1);
    }

    return mBrowserValueName;
  }

  return LAUNCHER_ERROR_GENERIC();
}

std::wstring LauncherRegistryInfo::ResolveImageTimestampValueName() {
  if (mImageValueName.empty()) {
    mImageValueName.assign(mBinPath);
    mImageValueName.append(kImageTimestampSuffix,
                           ArrayLength(kImageTimestampSuffix) - 1);
  }

  return mImageValueName;
}

std::wstring LauncherRegistryInfo::ResolveTelemetryValueName() {
  if (mTelemetryValueName.empty()) {
    mTelemetryValueName.assign(mBinPath);
    mTelemetryValueName.append(kTelemetrySuffix,
                               ArrayLength(kTelemetrySuffix) - 1);
  }

  return mTelemetryValueName;
}

LauncherVoidResult LauncherRegistryInfo::WriteStartTimestamp(
    LauncherRegistryInfo::ProcessType aProcessType,
    const Maybe<uint64_t>& aValue) {
  LauncherResult<std::wstring> name = ResolveValueName(aProcessType);
  if (name.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(name);
  }

  ULARGE_INTEGER timestamp;
  if (aValue.isSome()) {
    timestamp.QuadPart = aValue.value();
  } else {
    // We need to use QPC here because millisecond granularity is too coarse
    // to properly measure the times involved.
    if (!::QueryPerformanceCounter(
            reinterpret_cast<LARGE_INTEGER*>(&timestamp))) {
      return LAUNCHER_ERROR_FROM_LAST();
    }
  }

  DWORD len = sizeof(timestamp);
  LSTATUS result =
      ::RegSetValueExW(mRegKey.get(), name.inspect().c_str(), 0, REG_QWORD,
                       reinterpret_cast<PBYTE>(&timestamp), len);
  if (result != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(result);
  }

  return Ok();
}

LauncherResult<DWORD> LauncherRegistryInfo::GetCurrentImageTimestamp() {
  nt::PEHeaders headers(::GetModuleHandleW(nullptr));
  if (!headers) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
  }

  DWORD timestamp;
  if (!headers.GetTimeStamp(timestamp)) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_INVALID_DATA);
  }

  return timestamp;
}

LauncherVoidResult LauncherRegistryInfo::WriteImageTimestamp(DWORD aTimestamp) {
  std::wstring imageTimestampValueName = ResolveImageTimestampValueName();

  DWORD len = sizeof(aTimestamp);
  LSTATUS result =
      ::RegSetValueExW(mRegKey.get(), imageTimestampValueName.c_str(), 0,
                       REG_DWORD, reinterpret_cast<PBYTE>(&aTimestamp), len);
  if (result != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(result);
  }

  return Ok();
}

LauncherResult<bool> LauncherRegistryInfo::ClearStartTimestamp(
    LauncherRegistryInfo::ProcessType aProcessType) {
  LauncherResult<std::wstring> timestampName = ResolveValueName(aProcessType);
  if (timestampName.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(timestampName);
  }

  LSTATUS result =
      ::RegDeleteValueW(mRegKey.get(), timestampName.inspect().c_str());
  if (result == ERROR_SUCCESS) {
    return true;
  }

  if (result == ERROR_FILE_NOT_FOUND) {
    return false;
  }

  return LAUNCHER_ERROR_FROM_WIN32(result);
}

LauncherVoidResult LauncherRegistryInfo::ClearStartTimestamps() {
  LauncherResult<EnabledState> enabled = IsEnabled();
  if (enabled.isOk() && enabled.inspect() == EnabledState::ForceDisabled) {
    // We don't clear anything when we're force disabled - we need to maintain
    // the current registry state in this case.
    return Ok();
  }

  LauncherResult<bool> clearedLauncherTimestamp =
      ClearStartTimestamp(ProcessType::Launcher);
  if (clearedLauncherTimestamp.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(clearedLauncherTimestamp);
  }

  LauncherResult<bool> clearedBrowserTimestamp =
      ClearStartTimestamp(ProcessType::Browser);
  if (clearedBrowserTimestamp.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(clearedBrowserTimestamp);
  }

  return Ok();
}

LauncherResult<DWORD> LauncherRegistryInfo::GetSavedImageTimestamp() {
  std::wstring imageTimestampValueName = ResolveImageTimestampValueName();

  DWORD value;
  DWORD valueLen = sizeof(value);
  DWORD type;
  LSTATUS result = ::RegQueryValueExW(
      mRegKey.get(), imageTimestampValueName.c_str(), nullptr, &type,
      reinterpret_cast<PBYTE>(&value), &valueLen);
  // NB: If the value does not exist, result == ERROR_FILE_NOT_FOUND
  if (result != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(result);
  }

  if (type != REG_DWORD) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
  }

  return value;
}

LauncherResult<bool> LauncherRegistryInfo::GetTelemetrySetting() {
  std::wstring telemetryValueName = ResolveTelemetryValueName();

  DWORD value;
  DWORD valueLen = sizeof(value);
  DWORD type;
  LSTATUS result =
      ::RegQueryValueExW(mRegKey.get(), telemetryValueName.c_str(), nullptr,
                         &type, reinterpret_cast<PBYTE>(&value), &valueLen);
  if (result == ERROR_FILE_NOT_FOUND) {
    // Value does not exist, treat as false
    return false;
  }

  if (result != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(result);
  }

  if (type != REG_DWORD) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
  }

  return value != 0;
}

LauncherResult<uint64_t> LauncherRegistryInfo::GetStartTimestamp(
    LauncherRegistryInfo::ProcessType aProcessType) {
  LauncherResult<std::wstring> name = ResolveValueName(aProcessType);
  if (name.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(name);
  }

  uint64_t value;
  DWORD valueLen = sizeof(value);
  DWORD type;
  LSTATUS result =
      ::RegQueryValueExW(mRegKey.get(), name.inspect().c_str(), nullptr, &type,
                         reinterpret_cast<PBYTE>(&value), &valueLen);
  // NB: If the value does not exist, result == ERROR_FILE_NOT_FOUND
  if (result != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(result);
  }

  if (type != REG_QWORD) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
  }

  return value;
}

}  // namespace mozilla
