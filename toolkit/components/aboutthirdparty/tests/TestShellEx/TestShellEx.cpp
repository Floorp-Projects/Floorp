/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefPtr.h"
#include "RegUtils.h"

#include <windows.h>
#include <objbase.h>

already_AddRefed<IClassFactory> CreateFactory();

// {10A9521E-0205-4CC7-93A1-62F30A9A54B3}
GUID CLSID_TestShellEx = {
    0x10a9521e, 0x205, 0x4cc7, {0x93, 0xa1, 0x62, 0xf3, 0xa, 0x9a, 0x54, 0xb3}};
wchar_t kFriendlyName[] = L"Minimum Shell Extension for Firefox testing";

std::wstring gDllPath;

BOOL APIENTRY DllMain(HMODULE aModule, DWORD aReason, LPVOID) {
  wchar_t buf[MAX_PATH];
  switch (aReason) {
    case DLL_PROCESS_ATTACH:
      if (!::GetModuleFileNameW(aModule, buf, ARRAYSIZE(buf))) {
        return FALSE;
      }
      gDllPath = buf;
      break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
      break;
  }
  return TRUE;
}

STDAPI DllGetClassObject(REFCLSID aClsid, REFIID aIid, void** aResult) {
  if (!IsEqualCLSID(aClsid, CLSID_TestShellEx) ||
      !IsEqualCLSID(aIid, IID_IClassFactory)) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }

  RefPtr<IClassFactory> factory = CreateFactory();
  return factory ? factory->QueryInterface(aIid, aResult) : E_OUTOFMEMORY;
}

STDAPI DllCanUnloadNow() { return S_OK; }

// These functions enable us to run "regsvr32 [/u] TestShellEx.dll" manually.
// (No admin privilege is needed because all access is under HKCU.)
// We don't use these functions in the mochitest, but having these makes easier
// to test the module manually.
STDAPI DllRegisterServer() {
  ComRegisterer reg(CLSID_TestShellEx, kFriendlyName);
  if (!reg.RegisterObject(L"Apartment") || !reg.RegisterExtensions()) {
    return E_ACCESSDENIED;
  }
  return S_OK;
}
STDAPI DllUnregisterServer() {
  ComRegisterer reg(CLSID_TestShellEx, kFriendlyName);
  if (!reg.UnregisterAll()) {
    return E_ACCESSDENIED;
  }
  return S_OK;
}
