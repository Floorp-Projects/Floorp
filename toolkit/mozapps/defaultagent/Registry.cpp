/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Registry.h"

#include <windows.h>
#include <shlwapi.h>

#include "common.h"

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

MaybeStringResult RegistryGetValueString(IsPrefixed isPrefixed,
                                         const wchar_t* registryValueName) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  // Get the string size
  DWORD wideDataSize = 0;
  LSTATUS ls =
      RegGetValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME, valueName.c_str(),
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
  mozilla::UniquePtr<wchar_t[]> wideData =
      mozilla::MakeUnique<wchar_t[]>(charCount);
  ls = RegGetValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME, valueName.c_str(),
                    RRF_RT_REG_SZ, nullptr, wideData.get(), &wideDataSize);
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  // Convert to narrow string and return.
  int narrowLen = WideCharToMultiByte(CP_UTF8, 0, wideData.get(), -1, nullptr,
                                      0, nullptr, nullptr);
  if (narrowLen == 0) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }
  mozilla::UniquePtr<char[]> narrowOldValue =
      mozilla::MakeUnique<char[]>(narrowLen);
  int charsWritten =
      WideCharToMultiByte(CP_UTF8, 0, wideData.get(), -1, narrowOldValue.get(),
                          narrowLen, nullptr, nullptr);
  if (charsWritten == 0) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }
  return mozilla::Some(std::string(narrowOldValue.get()));
}

mozilla::WindowsErrorResult<mozilla::Ok> RegistrySetValueString(
    IsPrefixed isPrefixed, const wchar_t* registryValueName,
    const char* newValue) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  // Convert the value from a narrow string to a wide string
  int wideLen = MultiByteToWideChar(CP_UTF8, 0, newValue, -1, nullptr, 0);
  if (wideLen == 0) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }
  mozilla::UniquePtr<wchar_t[]> wideValue =
      mozilla::MakeUnique<wchar_t[]>(wideLen);
  int charsWritten =
      MultiByteToWideChar(CP_UTF8, 0, newValue, -1, wideValue.get(), wideLen);
  if (charsWritten == 0) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  // Store the value
  LSTATUS ls =
      RegSetKeyValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME, valueName.c_str(),
                      REG_SZ, wideValue.get(), wideLen * sizeof(wchar_t));
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Ok();
}

MaybeBoolResult RegistryGetValueBool(IsPrefixed isPrefixed,
                                     const wchar_t* registryValueName) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  // Read the integer value from the registry
  DWORD value;
  DWORD valueSize = sizeof(DWORD);
  LSTATUS ls =
      RegGetValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME, valueName.c_str(),
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

mozilla::WindowsErrorResult<mozilla::Ok> RegistrySetValueBool(
    IsPrefixed isPrefixed, const wchar_t* registryValueName, bool newValue) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  // Write the value to the registry
  DWORD value = newValue ? 1 : 0;
  LSTATUS ls =
      RegSetKeyValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME, valueName.c_str(),
                      REG_DWORD, &value, sizeof(DWORD));
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Ok();
}

MaybeQwordResult RegistryGetValueQword(IsPrefixed isPrefixed,
                                       const wchar_t* registryValueName) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  // Read the integer value from the registry
  ULONGLONG value;
  DWORD valueSize = sizeof(ULONGLONG);
  LSTATUS ls =
      RegGetValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME, valueName.c_str(),
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

mozilla::WindowsErrorResult<mozilla::Ok> RegistrySetValueQword(
    IsPrefixed isPrefixed, const wchar_t* registryValueName,
    ULONGLONG newValue) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  // Write the value to the registry
  LSTATUS ls =
      RegSetKeyValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME, valueName.c_str(),
                      REG_QWORD, &newValue, sizeof(ULONGLONG));
  if (ls != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Ok();
}

mozilla::WindowsErrorResult<mozilla::Ok> RegistryDeleteValue(
    IsPrefixed isPrefixed, const wchar_t* registryValueName) {
  // Get the full registry value name
  WStringResult registryValueNameResult =
      MaybePrefixRegistryValueName(isPrefixed, registryValueName);
  if (registryValueNameResult.isErr()) {
    return mozilla::Err(registryValueNameResult.unwrapErr());
  }
  std::wstring valueName = registryValueNameResult.unwrap();

  LSTATUS ls = RegDeleteKeyValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME,
                                  valueName.c_str());
  if (ls == ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(ls);
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return mozilla::Ok();
}
