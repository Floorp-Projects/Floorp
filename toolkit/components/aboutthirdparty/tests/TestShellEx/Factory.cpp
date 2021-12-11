/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Atomics.h"
#include "mozilla/RefPtr.h"

#include <windows.h>
#include <shlobj.h>

already_AddRefed<IExtractIconW> CreateIconExtension();

class ClassFactory final : public IClassFactory {
  mozilla::Atomic<uint32_t> mRefCnt;

  ~ClassFactory() = default;

 public:
  ClassFactory() : mRefCnt(0) {}

  // IUnknown

  STDMETHODIMP QueryInterface(REFIID aRefIID, void** aResult) {
    if (!aResult) {
      return E_INVALIDARG;
    }

    if (aRefIID == IID_IClassFactory) {
      RefPtr ref(static_cast<IClassFactory*>(this));
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

  // IClassFactory

  STDMETHODIMP CreateInstance(IUnknown* aOuter, REFIID aRefIID,
                              void** aResult) {
    if (aOuter) {
      return CLASS_E_NOAGGREGATION;
    }

    RefPtr<IUnknown> instance;
    if (IsEqualCLSID(aRefIID, IID_IExtractIconA) ||
        IsEqualCLSID(aRefIID, IID_IExtractIconW)) {
      instance = CreateIconExtension();
    } else {
      return E_NOINTERFACE;
    }

    return instance ? instance->QueryInterface(aRefIID, aResult)
                    : E_OUTOFMEMORY;
  }

  STDMETHODIMP LockServer(BOOL) { return S_OK; }
};

already_AddRefed<IClassFactory> CreateFactory() {
  return mozilla::MakeAndAddRef<ClassFactory>();
}
