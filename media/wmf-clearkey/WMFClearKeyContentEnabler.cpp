/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFClearKeyContentEnabler.h"

#include <mfapi.h>
#include <mferror.h>

#include "WMFClearKeyUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;

HRESULT WMFClearKeyContentEnabler::RuntimeClassInitialize() { return S_OK; }

STDMETHODIMP WMFClearKeyContentEnabler::AutomaticEnable() {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyContentEnabler::Cancel() {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyContentEnabler::GetEnableData(BYTE** aData,
                                                      DWORD* aDataSize) {
  ENTRY_LOG();
  // Does not support this method of content enabling with EME.
  return MF_E_NOT_AVAILABLE;
}

STDMETHODIMP WMFClearKeyContentEnabler::GetEnableURL(
    LPWSTR* aUrl, DWORD* aUrlSize, MF_URL_TRUST_STATUS* aTrustStatus) {
  ENTRY_LOG();
  // Does not support this method of content enabling with EME.
  return MF_E_NOT_AVAILABLE;
}

STDMETHODIMP WMFClearKeyContentEnabler::IsAutomaticSupported(BOOL* aAutomatic) {
  ENTRY_LOG();
  if (!aAutomatic) {
    return E_INVALIDARG;
  }
  *aAutomatic = FALSE;
  return S_OK;
}

STDMETHODIMP WMFClearKeyContentEnabler::MonitorEnable() {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyContentEnabler::GetEnableType(GUID* aType) {
  LOG("WMFClearKeyContentEnabler::GetEnableType");
  if (!aType) {
    return E_INVALIDARG;
  }
  *aType = MEDIA_FOUNDATION_CLEARKEY_GUID_CONTENT_ENABLER_TYPE;
  return S_OK;
}

}  // namespace mozilla
