/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Atomics.h"
#include "mozilla/RefPtr.h"
#include "Resource.h"

#include <windows.h>
#include <shlobj.h>

#include <string>

extern std::wstring gDllPath;
extern GUID CLSID_TestShellEx;

class IconExtension final : public IPersistFile,
                            public IExtractIconA,
                            public IExtractIconW {
  mozilla::Atomic<uint32_t> mRefCnt;

  ~IconExtension() = default;

 public:
  IconExtension() : mRefCnt(0) {}

  // IUnknown

  STDMETHODIMP QueryInterface(REFIID aRefIID, void** aResult) {
    if (!aResult) {
      return E_INVALIDARG;
    }

    if (aRefIID == IID_IPersist) {
      RefPtr ref(static_cast<IPersist*>(this));
      ref.forget(aResult);
      return S_OK;
    } else if (aRefIID == IID_IPersistFile) {
      RefPtr ref(static_cast<IPersistFile*>(this));
      ref.forget(aResult);
      return S_OK;
    } else if (aRefIID == IID_IExtractIconA) {
      RefPtr ref(static_cast<IExtractIconA*>(this));
      ref.forget(aResult);
      return S_OK;
    } else if (aRefIID == IID_IExtractIconW) {
      RefPtr ref(static_cast<IExtractIconW*>(this));
      ref.forget(aResult);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() { return ++mRefCnt; }

  STDMETHODIMP_(ULONG) Release() {
    ULONG result = --mRefCnt;
    if (!result) {
      delete this;
    }
    return result;
  }

  // IPersist

  STDMETHODIMP GetClassID(CLSID* aClassID) {
    *aClassID = CLSID_TestShellEx;
    return S_OK;
  }

  // IPersistFile

  STDMETHODIMP GetCurFile(LPOLESTR*) { return E_NOTIMPL; }
  STDMETHODIMP IsDirty() { return S_FALSE; }
  STDMETHODIMP Load(LPCOLESTR, DWORD) { return S_OK; }
  STDMETHODIMP Save(LPCOLESTR, BOOL) { return E_NOTIMPL; }
  STDMETHODIMP SaveCompleted(LPCOLESTR) { return E_NOTIMPL; }

  // IExtractIconA

  STDMETHODIMP Extract(PCSTR, UINT, HICON*, HICON*, UINT) { return E_NOTIMPL; }
  STDMETHODIMP GetIconLocation(UINT, PSTR, UINT, int*, UINT*) {
    return E_NOTIMPL;
  }

  // IExtractIconW

  STDMETHODIMP Extract(PCWSTR, UINT, HICON*, HICON*, UINT) { return S_FALSE; }

  STDMETHODIMP GetIconLocation(UINT, PWSTR aIconFile, UINT aCchMax, int* aIndex,
                               UINT* aOutFlags) {
    if (aCchMax <= gDllPath.size()) {
      return E_NOT_SUFFICIENT_BUFFER;
    }

    gDllPath.copy(aIconFile, gDllPath.size());
    aIconFile[gDllPath.size()] = 0;
    *aOutFlags = GIL_DONTCACHE;
    *aIndex = -IDI_ICON1;
    return S_OK;
  }
};

already_AddRefed<IExtractIconW> CreateIconExtension() {
  return mozilla::MakeAndAddRef<IconExtension>();
}
