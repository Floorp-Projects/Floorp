/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <filesystem>
#include <string>

#include "mozilla/WinHeaderOnlyUtils.h"

#include "NotificationFactory.h"

using namespace std::filesystem;

static path processDllPath = {};

// Populate the path to this DLL.
bool PopulateDllPath(HINSTANCE dllInstance) {
  std::vector<wchar_t> path(MAX_PATH, 0);
  DWORD charsWritten =
      GetModuleFileNameW(dllInstance, path.data(), path.size());

  // GetModuleFileNameW returns the count of characters written including null
  // when truncated, excluding null otherwise. Therefore the count will always
  // be less than the buffer size when not truncated.
  while (charsWritten == path.size()) {
    path.resize(path.size() * 2, 0);
    charsWritten = GetModuleFileNameW(dllInstance, path.data(), path.size());
  }

  if (charsWritten == 0) {
    return false;
  }

  processDllPath = path.data();
  return true;
}

// Our activator's CLSID is generated once either during install or at runtime
// by the application generating the notification so that notifications work
// with parallel installs and portable/development builds. When a COM object is
// requested we verify the CLSID's InprocServer registry entry matches this
// DLL's path.
bool CheckRuntimeClsid(REFCLSID rclsid) {
  // MSIX Notification COM Server registration is isolated to the package and is
  // identical across installs/channels.
  if (mozilla::HasPackageIdentity()) {
    // Keep synchronized with `python\mozbuild\mozbuild\repackaging\msix.py`.
    constexpr CLSID MOZ_INOTIFICATIONACTIVATION_CLSID = {
        0x916f9b5d,
        0xb5b2,
        0x4d36,
        {0xb0, 0x47, 0x03, 0xc7, 0xa5, 0x2f, 0x81, 0xc8}};

    return IsEqualCLSID(rclsid, MOZ_INOTIFICATIONACTIVATION_CLSID);
  }

  std::wstring clsid_str;
  {
    wchar_t* raw_clsid_str;
    if (SUCCEEDED(StringFromCLSID(rclsid, &raw_clsid_str))) {
      clsid_str += raw_clsid_str;
      CoTaskMemFree(raw_clsid_str);
    } else {
      return false;
    }
  }

  std::wstring key = L"CLSID\\";
  key += clsid_str;
  key += L"\\InprocServer32";

  DWORD bufferLen = 0;
  LSTATUS status = RegGetValueW(HKEY_CLASSES_ROOT, key.c_str(), L"",
                                RRF_RT_REG_SZ, nullptr, nullptr, &bufferLen);
  if (status != ERROR_SUCCESS) {
    return false;
  }

  std::vector<wchar_t> clsidDllPathBuffer(bufferLen / sizeof(wchar_t));
  // Sanity assignment in case the buffer length found was not an integer
  // multiple of `sizeof(wchar_t)`.
  bufferLen = clsidDllPathBuffer.size() * sizeof(wchar_t);

  status = RegGetValueW(HKEY_CLASSES_ROOT, key.c_str(), L"", RRF_RT_REG_SZ,
                        nullptr, clsidDllPathBuffer.data(), &bufferLen);
  if (status != ERROR_SUCCESS) {
    return false;
  }

  path clsidDllPath = clsidDllPathBuffer.data();
  return equivalent(processDllPath, clsidDllPath);
}

extern "C" {
HRESULT STDMETHODCALLTYPE DllGetClassObject(REFCLSID rclsid, REFIID riid,
                                            LPVOID* ppv) {
  if (!ppv) {
    return E_INVALIDARG;
  }
  *ppv = nullptr;

  if (!CheckRuntimeClsid(rclsid)) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }

  using namespace Microsoft::WRL;
  ComPtr<NotificationFactory> factory =
      Make<NotificationFactory, const GUID&, const path&>(
          rclsid, processDllPath.parent_path());

  switch (factory->QueryInterface(riid, ppv)) {
    case S_OK:
      return S_OK;
    case E_NOINTERFACE:
      return CLASS_E_CLASSNOTAVAILABLE;
    default:
      return E_UNEXPECTED;
  }
}

BOOL STDMETHODCALLTYPE DllMain(HINSTANCE hinstDLL, DWORD fdwReason,
                               LPVOID lpReserved) {
  if (fdwReason == DLL_PROCESS_ATTACH) {
    if (!PopulateDllPath(hinstDLL)) {
      return FALSE;
    }
  }
  return TRUE;
}
}
