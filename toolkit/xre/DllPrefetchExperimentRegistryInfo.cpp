/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "DllPrefetchExperimentRegistryInfo.h"

#include "mozilla/Assertions.h"
#include "mozilla/NativeNt.h"
#include "mozilla/ResultExtensions.h"

namespace mozilla {

const wchar_t DllPrefetchExperimentRegistryInfo::kExperimentSubKeyPath[] =
    L"SOFTWARE"
    L"\\" MOZ_APP_VENDOR L"\\" MOZ_APP_BASENAME L"\\DllPrefetchExperiment";

Result<Ok, nsresult> DllPrefetchExperimentRegistryInfo::Open() {
  if (!!mRegKey) {
    return Ok();
  }

  DWORD disposition;
  HKEY rawKey;
  LSTATUS result = ::RegCreateKeyExW(
      HKEY_CURRENT_USER, kExperimentSubKeyPath, 0, nullptr,
      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &rawKey, &disposition);

  if (result != ERROR_SUCCESS) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  mRegKey.own(rawKey);

  if (disposition == REG_CREATED_NEW_KEY ||
      disposition == REG_OPENED_EXISTING_KEY) {
    return Ok();
  }

  MOZ_ASSERT_UNREACHABLE("Invalid disposition from RegCreateKeyExW");
  return Err(NS_ERROR_UNEXPECTED);
}

Result<Ok, nsresult> DllPrefetchExperimentRegistryInfo::ReflectPrefToRegistry(
    int32_t aVal) {
  MOZ_TRY(Open());

  mPrefetchMode = aVal;

  LSTATUS status =
      ::RegSetValueExW(mRegKey.get(), mBinPath.get(), 0, REG_DWORD,
                       reinterpret_cast<PBYTE>(&aVal), sizeof(aVal));

  if (status != ERROR_SUCCESS) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  return Ok();
}

Result<Ok, nsresult> DllPrefetchExperimentRegistryInfo::ReadRegistryValueData(
    DWORD expectedType) {
  MOZ_TRY(Open());

  int32_t data;
  DWORD dataLen = sizeof((ULONG)data);
  DWORD type;
  LSTATUS status =
      ::RegQueryValueExW(mRegKey.get(), mBinPath.get(), nullptr, &type,
                         reinterpret_cast<PBYTE>(&data), &dataLen);

  if (status == ERROR_FILE_NOT_FOUND) {
    // The registry key has not been created, set to default 0
    mPrefetchMode = 0;
    return Ok();
  }

  if (status != ERROR_SUCCESS) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  if (type != expectedType) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  mPrefetchMode = data;
  return Ok();
}

AlteredDllPrefetchMode
DllPrefetchExperimentRegistryInfo::GetAlteredDllPrefetchMode() {
  Result<Ok, nsresult> result = ReadRegistryValueData(REG_DWORD);
  if (!result.isOk()) {
    MOZ_ASSERT(false);
    return AlteredDllPrefetchMode::CurrentPrefetch;
  }

  switch (mPrefetchMode) {
    case 0:
      return AlteredDllPrefetchMode::CurrentPrefetch;
    case 1:
      return AlteredDllPrefetchMode::NoPrefetch;
    case 2:
      return AlteredDllPrefetchMode::OptimizedPrefetch;
    default:
      MOZ_ASSERT(false);
      return AlteredDllPrefetchMode::CurrentPrefetch;
  }
}

}  // namespace mozilla
