/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Registry.h"

#include <windows.h>
#include <shlwapi.h>

#include "common.h"
#include "EventLog.h"
#include "UtfConvert.h"

#include "mozilla/Buffer.h"
#include "mozilla/Try.h"

namespace mozilla::default_agent {

using WStringResult = mozilla::WindowsErrorResult<std::wstring>;

static WStringResult MaybePrefixRegistryValueName(
    IsPrefixed isPrefixed, const wchar_t* registryValueNameSuffix) {
  if (isPrefixed == IsPrefixed::Unprefixed) {
    std::wstring registryValueName = registryValueNameSuffix;
    return registryValueName;
  }

  mozilla::UniquePtr<wchar_t[]> installPath = mozilla::GetFullBinaryPath();
  if (!PathRemoveFileSpecW(installPath.get())) {
    HRESULT hr = HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }
  std::wstring registryValueName(installPath.get());
  registryValueName.append(L"|");
  registryValueName.append(registryValueNameSuffix);

  return registryValueName;
}

// Creates a sub key of AGENT_REGKEY_NAME by appending the passed subKey. If
// subKey is null, nothing is appended.
static std::wstring MakeKeyName(const wchar_t* subKey) {
  std::wstring keyName = AGENT_REGKEY_NAME;
  if (subKey) {
    keyName += L"\\";
    keyName += subKey;
  }
  return keyName;
}

MaybeStringResult RegistryGetValueString(
    IsPrefixed isPrefixed, const wchar_t* registryValueName,
    const wchar_t* subKey /* = nullptr */) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  std::wstring keyName = MakeKeyName(subKey);

  // Get the string size
  DWORD wideDataSize = 0;
  LSTATUS ls =
      RegGetValueW(HKEY_CURRENT_USER, keyName.c_str(), valueName.c_str(),
                   RRF_RT_REG_SZ, nullptr, nullptr, &wideDataSize);
  if (ls == ERROR_FILE_NOT_FOUND) {
    return mozilla::Maybe<std::string>(mozilla::Nothing());
  } else if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  // Convert bytes to characters. The extra character should be unnecessary, but
  // addresses the possible rounding problem inherent with integer division.
  DWORD charCount = (wideDataSize / sizeof(wchar_t)) + 1;

  // Read the data from the registry into a wide string
  mozilla::Buffer<wchar_t> wideData(charCount);
  ls = RegGetValueW(HKEY_CURRENT_USER, keyName.c_str(), valueName.c_str(),
                    RRF_RT_REG_SZ, nullptr, wideData.Elements(), &wideDataSize);
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  // Convert to narrow string and return.
  std::string narrowData;
  MOZ_TRY_VAR(narrowData, Utf16ToUtf8(wideData.Elements()));

  return mozilla::Some(narrowData);
}

VoidResult RegistrySetValueString(IsPrefixed isPrefixed,
                                  const wchar_t* registryValueName,
                                  const char* newValue,
                                  const wchar_t* subKey /* = nullptr */) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  std::wstring keyName = MakeKeyName(subKey);

  // Convert the value from a narrow string to a wide string
  std::wstring wideValue;
  MOZ_TRY_VAR(wideValue, Utf8ToUtf16(newValue));

  // Store the value
  LSTATUS ls = RegSetKeyValueW(HKEY_CURRENT_USER, keyName.c_str(),
                               valueName.c_str(), REG_SZ, wideValue.c_str(),
                               (wideValue.size() + 1) * sizeof(wchar_t));
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Ok();
}

MaybeBoolResult RegistryGetValueBool(IsPrefixed isPrefixed,
                                     const wchar_t* registryValueName,
                                     const wchar_t* subKey /* = nullptr */) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  std::wstring keyName = MakeKeyName(subKey);

  // Read the integer value from the registry
  DWORD value;
  DWORD valueSize = sizeof(DWORD);
  LSTATUS ls =
      RegGetValueW(HKEY_CURRENT_USER, keyName.c_str(), valueName.c_str(),
                   RRF_RT_REG_DWORD, nullptr, &value, &valueSize);
  if (ls == ERROR_FILE_NOT_FOUND) {
    return mozilla::Maybe<bool>(mozilla::Nothing());
  }
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Some(value != 0);
}

VoidResult RegistrySetValueBool(IsPrefixed isPrefixed,
                                const wchar_t* registryValueName, bool newValue,
                                const wchar_t* subKey /* = nullptr */) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  std::wstring keyName = MakeKeyName(subKey);

  // Write the value to the registry
  DWORD value = newValue ? 1 : 0;
  LSTATUS ls =
      RegSetKeyValueW(HKEY_CURRENT_USER, keyName.c_str(), valueName.c_str(),
                      REG_DWORD, &value, sizeof(DWORD));
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Ok();
}

MaybeQwordResult RegistryGetValueQword(IsPrefixed isPrefixed,
                                       const wchar_t* registryValueName,
                                       const wchar_t* subKey /* = nullptr */) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  std::wstring keyName = MakeKeyName(subKey);

  // Read the integer value from the registry
  ULONGLONG value;
  DWORD valueSize = sizeof(ULONGLONG);
  LSTATUS ls =
      RegGetValueW(HKEY_CURRENT_USER, keyName.c_str(), valueName.c_str(),
                   RRF_RT_REG_QWORD, nullptr, &value, &valueSize);
  if (ls == ERROR_FILE_NOT_FOUND) {
    return mozilla::Maybe<ULONGLONG>(mozilla::Nothing());
  }
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Some(value);
}

VoidResult RegistrySetValueQword(IsPrefixed isPrefixed,
                                 const wchar_t* registryValueName,
                                 ULONGLONG newValue,
                                 const wchar_t* subKey /* = nullptr */) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  std::wstring keyName = MakeKeyName(subKey);

  // Write the value to the registry
  LSTATUS ls =
      RegSetKeyValueW(HKEY_CURRENT_USER, keyName.c_str(), valueName.c_str(),
                      REG_QWORD, &newValue, sizeof(ULONGLONG));
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Ok();
}

MaybeDwordResult RegistryGetValueDword(IsPrefixed isPrefixed,
                                       const wchar_t* registryValueName,
                                       const wchar_t* subKey /* = nullptr */) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  std::wstring keyName = MakeKeyName(subKey);

  // Read the integer value from the registry
  uint32_t value;
  DWORD valueSize = sizeof(uint32_t);
  LSTATUS ls =
      RegGetValueW(HKEY_CURRENT_USER, keyName.c_str(), valueName.c_str(),
                   RRF_RT_DWORD, nullptr, &value, &valueSize);
  if (ls == ERROR_FILE_NOT_FOUND) {
    return mozilla::Maybe<uint32_t>(mozilla::Nothing());
  }
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Some(value);
}

VoidResult RegistrySetValueDword(IsPrefixed isPrefixed,
                                 const wchar_t* registryValueName,
                                 uint32_t newValue,
                                 const wchar_t* subKey /* = nullptr */) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  std::wstring keyName = MakeKeyName(subKey);

  // Write the value to the registry
  LSTATUS ls =
      RegSetKeyValueW(HKEY_CURRENT_USER, keyName.c_str(), valueName.c_str(),
                      REG_DWORD, &newValue, sizeof(uint32_t));
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Ok();
}

VoidResult RegistryDeleteValue(IsPrefixed isPrefixed,
                               const wchar_t* registryValueName,
                               const wchar_t* subKey /* = nullptr */) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  std::wstring keyName = MakeKeyName(subKey);

  LSTATUS ls =
      RegDeleteKeyValueW(HKEY_CURRENT_USER, keyName.c_str(), valueName.c_str());
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Ok();
}

}  // namespace mozilla::default_agent
