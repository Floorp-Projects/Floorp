/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "LauncherRegistryInfo.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/NativeNt.h"

#include <string>

#define EXPAND_STRING_MACRO2(t) t
#define EXPAND_STRING_MACRO(t) EXPAND_STRING_MACRO2(t)

// This function is copied from Chromium base/time/time_win.cc
// Returns the current value of the performance counter.
static uint64_t QPCNowRaw() {
  LARGE_INTEGER perf_counter_now = {};
  // According to the MSDN documentation for QueryPerformanceCounter(), this
  // will never fail on systems that run XP or later.
  // https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter
  ::QueryPerformanceCounter(&perf_counter_now);
  return perf_counter_now.QuadPart;
}

static mozilla::LauncherResult<DWORD> GetCurrentImageTimestamp() {
  mozilla::nt::PEHeaders headers(::GetModuleHandleW(nullptr));
  if (!headers) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
  }

  DWORD timestamp;
  if (!headers.GetTimeStamp(timestamp)) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_INVALID_DATA);
  }

  return timestamp;
}

template <typename T>
static mozilla::LauncherResult<mozilla::Maybe<T>> ReadRegistryValueData(
    const nsAutoRegKey& key, const std::wstring& name, DWORD expectedType) {
  static_assert(mozilla::IsPod<T>::value,
                "Registry value type must be primitive.");
  T data;
  DWORD dataLen = sizeof(data);
  DWORD type;
  LSTATUS status = ::RegQueryValueExW(key.get(), name.c_str(), nullptr, &type,
                                      reinterpret_cast<PBYTE>(&data), &dataLen);
  if (status == ERROR_FILE_NOT_FOUND) {
    return mozilla::Maybe<T>();
  }

  if (status != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(status);
  }

  if (type != expectedType) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
  }

  return mozilla::Some(data);
}

template <typename T>
static mozilla::LauncherVoidResult WriteRegistryValueData(
    const nsAutoRegKey& key, const std::wstring& name, DWORD type, T data) {
  static_assert(mozilla::IsPod<T>::value,
                "Registry value type must be primitive.");
  LSTATUS status =
      ::RegSetValueExW(key.get(), name.c_str(), 0, type,
                       reinterpret_cast<PBYTE>(&data), sizeof(data));
  if (status != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(status);
  }

  return mozilla::Ok();
}

static mozilla::LauncherResult<bool> DeleteRegistryValueData(
    const nsAutoRegKey& key, const std::wstring& name) {
  LSTATUS status = ::RegDeleteValueW(key, name.c_str());
  if (status == ERROR_FILE_NOT_FOUND) {
    return false;
  }

  if (status != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(status);
  }

  return true;
}

namespace mozilla {

const wchar_t LauncherRegistryInfo::kLauncherSubKeyPath[] =
    L"SOFTWARE\\" EXPAND_STRING_MACRO(MOZ_APP_VENDOR) L"\\" EXPAND_STRING_MACRO(
        MOZ_APP_BASENAME) L"\\Launcher";
const wchar_t LauncherRegistryInfo::kLauncherSuffix[] = L"|Launcher";
const wchar_t LauncherRegistryInfo::kBrowserSuffix[] = L"|Browser";
const wchar_t LauncherRegistryInfo::kImageTimestampSuffix[] = L"|Image";
const wchar_t LauncherRegistryInfo::kTelemetrySuffix[] = L"|Telemetry";

bool LauncherRegistryInfo::sAllowCommit = true;

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
    return curEnabledState.propagateErr();
  }

  bool isCurrentlyEnabled =
      curEnabledState.inspect() != EnabledState::ForceDisabled;
  if (isCurrentlyEnabled == aEnable) {
    // Don't reflect to the registry unless the new enabled state is actually
    // changing with respect to the current enabled state.
    return Ok();
  }

  // Always delete the launcher timestamp
  LauncherResult<bool> clearedLauncherTimestamp = ClearLauncherStartTimestamp();
  MOZ_ASSERT(clearedLauncherTimestamp.isOk());
  if (clearedLauncherTimestamp.isErr()) {
    return clearedLauncherTimestamp.propagateErr();
  }

  // Allow commit when we enable the launcher, otherwise block.
  sAllowCommit = aEnable;

  if (!aEnable) {
    // Set the browser timestamp to 0 to indicate force-disabled
    return WriteBrowserStartTimestamp(0ULL);
  }

  // Otherwise we delete the browser timestamp to start over fresh
  LauncherResult<bool> clearedBrowserTimestamp = ClearBrowserStartTimestamp();
  MOZ_ASSERT(clearedBrowserTimestamp.isOk());
  if (clearedBrowserTimestamp.isErr()) {
    return clearedBrowserTimestamp.propagateErr();
  }

  return Ok();
}

LauncherVoidResult LauncherRegistryInfo::ReflectTelemetryPrefToRegistry(
    const bool aEnable) {
  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return disposition.propagateErr();
  }

  return WriteRegistryValueData(mRegKey, ResolveTelemetryValueName(), REG_DWORD,
                                aEnable ? 1UL : 0UL);
}

LauncherResult<LauncherRegistryInfo::ProcessType> LauncherRegistryInfo::Check(
    const ProcessType aDesiredType, const CheckOption aOption) {
  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return disposition.propagateErr();
  }

  LauncherResult<DWORD> ourImageTimestamp = GetCurrentImageTimestamp();
  if (ourImageTimestamp.isErr()) {
    return ourImageTimestamp.propagateErr();
  }

  LauncherResult<Maybe<DWORD>> savedImageTimestamp = GetSavedImageTimestamp();
  if (savedImageTimestamp.isErr()) {
    return savedImageTimestamp.propagateErr();
  }

  // If we don't have a saved timestamp, or we do but it doesn't match with
  // our current timestamp, clear previous values unless we're force-disabled.
  if (savedImageTimestamp.inspect().isNothing() ||
      savedImageTimestamp.inspect().value() != ourImageTimestamp.inspect()) {
    LauncherVoidResult clearResult = ClearStartTimestamps();
    if (clearResult.isErr()) {
      return clearResult.propagateErr();
    }

    LauncherVoidResult writeResult =
        WriteImageTimestamp(ourImageTimestamp.inspect());
    if (writeResult.isErr()) {
      return writeResult.propagateErr();
    }
  }

  // If we're going to be running as the browser process, or there is no
  // existing values to check, just write our timestamp and return.
  if (aDesiredType == ProcessType::Browser) {
    mBrowserTimestampToWrite = Some(QPCNowRaw());
    return ProcessType::Browser;
  }

  if (disposition.inspect() == Disposition::CreatedNew) {
    mLauncherTimestampToWrite = Some(QPCNowRaw());
    return ProcessType::Launcher;
  }

  if (disposition.inspect() != Disposition::OpenedExisting) {
    MOZ_ASSERT_UNREACHABLE("Invalid |disposition|");
    return LAUNCHER_ERROR_GENERIC();
  }

  LauncherResult<Maybe<uint64_t>> lastLauncherTimestampResult =
      GetLauncherStartTimestamp();
  if (lastLauncherTimestampResult.isErr()) {
    return lastLauncherTimestampResult.propagateErr();
  }

  LauncherResult<Maybe<uint64_t>> lastBrowserTimestampResult =
      GetBrowserStartTimestamp();
  if (lastBrowserTimestampResult.isErr()) {
    return lastBrowserTimestampResult.propagateErr();
  }

  const Maybe<uint64_t>& lastLauncherTimestamp =
      lastLauncherTimestampResult.inspect();
  const Maybe<uint64_t>& lastBrowserTimestamp =
      lastBrowserTimestampResult.inspect();

  ProcessType typeToRunAs = aDesiredType;

  if (lastLauncherTimestamp.isSome() != lastBrowserTimestamp.isSome()) {
    // If we have a launcher timestamp but no browser timestamp (or vice versa),
    // that's bad because it is indicating that the browser can't run with
    // the launcher process.
    typeToRunAs = ProcessType::Browser;
  } else if (lastLauncherTimestamp.isSome()) {
    // if we have both timestamps, we want to ensure that the launcher timestamp
    // is earlier than the browser timestamp.
    if (aDesiredType == ProcessType::Launcher) {
      bool areTimestampsOk =
          lastLauncherTimestamp.value() < lastBrowserTimestamp.value();
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

  switch (typeToRunAs) {
    case ProcessType::Browser:
      if (aDesiredType != typeToRunAs) {
        // We were hoping to run as the launcher, but some failure has caused
        // us to run as the browser. Set the browser timestamp to zero as an
        // indicator.
        mBrowserTimestampToWrite = Some(0ULL);
      } else {
        mBrowserTimestampToWrite = Some(QPCNowRaw());
      }
      break;
    case ProcessType::Launcher:
      mLauncherTimestampToWrite = Some(QPCNowRaw());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid |typeToRunAs|");
      return LAUNCHER_ERROR_GENERIC();
  }

  return typeToRunAs;
}

LauncherVoidResult LauncherRegistryInfo::DisableDueToFailure() {
  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return disposition.propagateErr();
  }
  LauncherVoidResult result = WriteBrowserStartTimestamp(0ULL);
  if (result.isOk()) {
    // Block commit when we disable the launcher.  It could be allowed
    // when the image timestamp is updated.
    sAllowCommit = false;
  }
  return result;
}

LauncherVoidResult LauncherRegistryInfo::Commit() {
  if (!sAllowCommit) {
    Abort();
    return Ok();
  }

  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return disposition.propagateErr();
  }

  if (mLauncherTimestampToWrite.isSome()) {
    LauncherVoidResult writeResult =
        WriteLauncherStartTimestamp(mLauncherTimestampToWrite.value());
    if (writeResult.isErr()) {
      return writeResult.propagateErr();
    }
    mLauncherTimestampToWrite = Nothing();
  }

  if (mBrowserTimestampToWrite.isSome()) {
    LauncherVoidResult writeResult =
        WriteBrowserStartTimestamp(mBrowserTimestampToWrite.value());
    if (writeResult.isErr()) {
      return writeResult.propagateErr();
    }
    mBrowserTimestampToWrite = Nothing();
  }

  return Ok();
}

void LauncherRegistryInfo::Abort() {
  mLauncherTimestampToWrite = mBrowserTimestampToWrite = Nothing();
}

LauncherRegistryInfo::EnabledState LauncherRegistryInfo::GetEnabledState(
    const Maybe<uint64_t>& aLauncherTs, const Maybe<uint64_t>& aBrowserTs) {
  if (aBrowserTs.isSome()) {
    if (aLauncherTs.isSome()) {
      if (aLauncherTs.value() < aBrowserTs.value()) {
        // Both timestamps exist and the browser's timestamp is later.
        return EnabledState::Enabled;
      }
    } else if (aBrowserTs.value() == 0ULL) {
      // Only browser's timestamp exists and its value is 0.
      return EnabledState::ForceDisabled;
    }
  } else if (aLauncherTs.isNothing()) {
    // Neither timestamps exist.
    return EnabledState::Enabled;
  }

  // Everything else is FailDisabled.
  return EnabledState::FailDisabled;
}

LauncherResult<LauncherRegistryInfo::EnabledState>
LauncherRegistryInfo::IsEnabled() {
  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return disposition.propagateErr();
  }

  LauncherResult<Maybe<uint64_t>> lastLauncherTimestamp =
      GetLauncherStartTimestamp();
  if (lastLauncherTimestamp.isErr()) {
    return lastLauncherTimestamp.propagateErr();
  }

  LauncherResult<Maybe<uint64_t>> lastBrowserTimestamp =
      GetBrowserStartTimestamp();
  if (lastBrowserTimestamp.isErr()) {
    return lastBrowserTimestamp.propagateErr();
  }

  return GetEnabledState(lastLauncherTimestamp.inspect(),
                         lastBrowserTimestamp.inspect());
}

LauncherResult<bool> LauncherRegistryInfo::IsTelemetryEnabled() {
  LauncherResult<Disposition> disposition = Open();
  if (disposition.isErr()) {
    return disposition.propagateErr();
  }

  LauncherResult<Maybe<DWORD>> result = ReadRegistryValueData<DWORD>(
      mRegKey, ResolveTelemetryValueName(), REG_DWORD);
  if (result.isErr()) {
    return result.propagateErr();
  }

  if (result.inspect().isNothing()) {
    // Value does not exist, treat as false
    return false;
  }

  return result.inspect().value() != 0;
}

const std::wstring& LauncherRegistryInfo::ResolveLauncherValueName() {
  if (mLauncherValueName.empty()) {
    mLauncherValueName.assign(mBinPath);
    mLauncherValueName.append(kLauncherSuffix,
                              ArrayLength(kLauncherSuffix) - 1);
  }

  return mLauncherValueName;
}

const std::wstring& LauncherRegistryInfo::ResolveBrowserValueName() {
  if (mBrowserValueName.empty()) {
    mBrowserValueName.assign(mBinPath);
    mBrowserValueName.append(kBrowserSuffix, ArrayLength(kBrowserSuffix) - 1);
  }

  return mBrowserValueName;
}

const std::wstring& LauncherRegistryInfo::ResolveImageTimestampValueName() {
  if (mImageValueName.empty()) {
    mImageValueName.assign(mBinPath);
    mImageValueName.append(kImageTimestampSuffix,
                           ArrayLength(kImageTimestampSuffix) - 1);
  }

  return mImageValueName;
}

const std::wstring& LauncherRegistryInfo::ResolveTelemetryValueName() {
  if (mTelemetryValueName.empty()) {
    mTelemetryValueName.assign(mBinPath);
    mTelemetryValueName.append(kTelemetrySuffix,
                               ArrayLength(kTelemetrySuffix) - 1);
  }

  return mTelemetryValueName;
}

LauncherVoidResult LauncherRegistryInfo::WriteLauncherStartTimestamp(
    uint64_t aValue) {
  return WriteRegistryValueData(mRegKey, ResolveLauncherValueName(), REG_QWORD,
                                aValue);
}

LauncherVoidResult LauncherRegistryInfo::WriteBrowserStartTimestamp(
    uint64_t aValue) {
  return WriteRegistryValueData(mRegKey, ResolveBrowserValueName(), REG_QWORD,
                                aValue);
}

LauncherVoidResult LauncherRegistryInfo::WriteImageTimestamp(DWORD aTimestamp) {
  return WriteRegistryValueData(mRegKey, ResolveImageTimestampValueName(),
                                REG_DWORD, aTimestamp);
}

LauncherResult<bool> LauncherRegistryInfo::ClearLauncherStartTimestamp() {
  return DeleteRegistryValueData(mRegKey, ResolveLauncherValueName());
}

LauncherResult<bool> LauncherRegistryInfo::ClearBrowserStartTimestamp() {
  return DeleteRegistryValueData(mRegKey, ResolveBrowserValueName());
}

LauncherVoidResult LauncherRegistryInfo::ClearStartTimestamps() {
  LauncherResult<EnabledState> enabled = IsEnabled();
  if (enabled.isOk() && enabled.inspect() == EnabledState::ForceDisabled) {
    // We don't clear anything when we're force disabled - we need to maintain
    // the current registry state in this case.
    return Ok();
  }

  LauncherResult<bool> clearedLauncherTimestamp = ClearLauncherStartTimestamp();
  if (clearedLauncherTimestamp.isErr()) {
    return clearedLauncherTimestamp.propagateErr();
  }

  LauncherResult<bool> clearedBrowserTimestamp = ClearBrowserStartTimestamp();
  if (clearedBrowserTimestamp.isErr()) {
    return clearedBrowserTimestamp.propagateErr();
  }

  // Reset both timestamps to align with registry deletion
  mLauncherTimestampToWrite = mBrowserTimestampToWrite = Nothing();

  // Disablement is gone.  Let's allow commit.
  sAllowCommit = true;

  return Ok();
}

LauncherResult<Maybe<DWORD>> LauncherRegistryInfo::GetSavedImageTimestamp() {
  return ReadRegistryValueData<DWORD>(mRegKey, ResolveImageTimestampValueName(),
                                      REG_DWORD);
}

LauncherResult<Maybe<uint64_t>>
LauncherRegistryInfo::GetLauncherStartTimestamp() {
  return ReadRegistryValueData<uint64_t>(mRegKey, ResolveLauncherValueName(),
                                         REG_QWORD);
}

LauncherResult<Maybe<uint64_t>>
LauncherRegistryInfo::GetBrowserStartTimestamp() {
  return ReadRegistryValueData<uint64_t>(mRegKey, ResolveBrowserValueName(),
                                         REG_QWORD);
}

}  // namespace mozilla
