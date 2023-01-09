/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationFactory.h"

HRESULT STDMETHODCALLTYPE NotificationFactory::CreateInstance(
    IUnknown* pUnkOuter, REFIID riid, void** ppvObject) {
  if (pUnkOuter != nullptr) {
    return CLASS_E_NOAGGREGATION;
  }

  if (!ppvObject) {
    return E_INVALIDARG;
  }
  *ppvObject = nullptr;

  using namespace Microsoft::WRL;
  ComPtr<NotificationCallback> callback =
      Make<NotificationCallback, const GUID&, const path&>(notificationGuid,
                                                           installDir);

  switch (callback->QueryInterface(riid, ppvObject)) {
    case S_OK:
      return S_OK;
    case E_NOINTERFACE:
      return E_NOINTERFACE;
    default:
      return E_UNEXPECTED;
  }
}
