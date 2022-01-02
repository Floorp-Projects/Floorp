/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/UniquePtr.h"
#include "RegUtils.h"

#include <windows.h>
#include <strsafe.h>

extern std::wstring gDllPath;

const wchar_t kClsIdPrefix[] = L"CLSID\\";
const wchar_t* kExtensionSubkeys[] = {
    L".zzz\\shellex\\IconHandler",
};

bool RegKey::SetStringInternal(const wchar_t* aValueName,
                               const wchar_t* aValueData,
                               DWORD aValueDataLength) {
  if (!mKey) {
    return false;
  }

  return ::RegSetValueExW(mKey, aValueName, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(aValueData),
                          aValueDataLength) == ERROR_SUCCESS;
}

RegKey::RegKey(HKEY root, const wchar_t* aSubkey) : mKey(nullptr) {
  ::RegCreateKeyExW(root, aSubkey, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr,
                    &mKey, nullptr);
}

RegKey::~RegKey() {
  if (mKey) {
    ::RegCloseKey(mKey);
  }
}

bool RegKey::SetString(const wchar_t* aValueName, const wchar_t* aValueData) {
  return SetStringInternal(
      aValueName, aValueData,
      aValueData
          ? static_cast<DWORD>((wcslen(aValueData) + 1) * sizeof(wchar_t))
          : 0);
}

bool RegKey::SetString(const wchar_t* aValueName,
                       const std::wstring& aValueData) {
  return SetStringInternal(
      aValueName, aValueData.c_str(),
      static_cast<DWORD>((aValueData.size() + 1) * sizeof(wchar_t)));
}

std::wstring RegKey::GetString(const wchar_t* aValueName) {
  DWORD len = 0;
  LSTATUS status = ::RegGetValueW(mKey, aValueName, nullptr, RRF_RT_REG_SZ,
                                  nullptr, nullptr, &len);

  mozilla::UniquePtr<uint8_t[]> buf = mozilla::MakeUnique<uint8_t[]>(len);
  status = ::RegGetValueW(mKey, aValueName, nullptr, RRF_RT_REG_SZ, nullptr,
                          buf.get(), &len);
  if (status != ERROR_SUCCESS) {
    return L"";
  }

  return reinterpret_cast<wchar_t*>(buf.get());
}

ComRegisterer::ComRegisterer(const GUID& aClsId, const wchar_t* aFriendlyName)
    : mClassRoot(HKEY_CURRENT_USER, L"Software\\Classes"),
      mFriendlyName(aFriendlyName) {
  wchar_t guidStr[64];
  HRESULT hr = ::StringCbPrintfW(
      guidStr, sizeof(guidStr),
      L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", aClsId.Data1,
      aClsId.Data2, aClsId.Data3, aClsId.Data4[0], aClsId.Data4[1],
      aClsId.Data4[2], aClsId.Data4[3], aClsId.Data4[4], aClsId.Data4[5],
      aClsId.Data4[6], aClsId.Data4[7]);
  if (FAILED(hr)) {
    return;
  }

  mClsId = guidStr;
}

bool ComRegisterer::UnregisterAll() {
  bool isOk = true;
  LSTATUS ls;

  for (const wchar_t* subkey : kExtensionSubkeys) {
    RegKey root(mClassRoot, subkey);

    std::wstring currentHandler = root.GetString(nullptr);
    if (currentHandler != mClsId) {
      // If another extension is registered, don't overwrite it.
      continue;
    }

    // Set an empty string instead of deleting the key.
    if (!root.SetString(nullptr)) {
      isOk = false;
    }
  }

  std::wstring subkey(kClsIdPrefix);
  subkey += mClsId;
  ls = ::RegDeleteTreeW(mClassRoot, subkey.c_str());
  if (ls != ERROR_SUCCESS && ls != ERROR_FILE_NOT_FOUND) {
    isOk = false;
  }

  return isOk;
}

bool ComRegisterer::RegisterObject(const wchar_t* aThreadModel) {
  std::wstring subkey(kClsIdPrefix);
  subkey += mClsId;

  RegKey root(mClassRoot, subkey.c_str());
  if (!root || !root.SetString(nullptr, mFriendlyName)) {
    return false;
  }

  RegKey inproc(root, L"InprocServer32");
  return inproc && inproc.SetString(nullptr, gDllPath) &&
         inproc.SetString(L"ThreadingModel", aThreadModel);
}

bool ComRegisterer::RegisterExtensions() {
  for (const wchar_t* subkey : kExtensionSubkeys) {
    RegKey root(mClassRoot, subkey);
    std::wstring currentHandler = root.GetString(nullptr);
    if (!currentHandler.empty()) {
      // If another extension is registered, don't overwrite it.
      continue;
    }

    if (!root.SetString(nullptr, mClsId)) {
      return false;
    }
  }

  return true;
}
