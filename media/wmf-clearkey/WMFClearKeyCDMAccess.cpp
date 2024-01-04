/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFClearKeyCDMAccess.h"

#include <Mferror.h>
#include <oleauto.h>

#include "WMFClearKeyCDM.h"
#include "WMFClearKeyUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

STDMETHODIMP WMFClearKeyCDMAccess::CreateContentDecryptionModule(
    IPropertyStore* aProperties, IMFContentDecryptionModule** aCdm) {
  ENTRY_LOG();
  if (!aProperties) {
    ENTRY_LOG_ARGS("Null properties!");
    return MF_E_UNEXPECTED;
  }

  *aCdm = nullptr;
  ComPtr<IMFContentDecryptionModule> cdm;
  RETURN_IF_FAILED(
      (MakeAndInitialize<WMFClearKeyCDM, IMFContentDecryptionModule>(
          &cdm, aProperties)));
  *aCdm = cdm.Detach();
  ENTRY_LOG_ARGS("Created clearkey CDM!");
  return S_OK;
}

STDMETHODIMP WMFClearKeyCDMAccess::GetConfiguration(IPropertyStore** aConfig) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyCDMAccess::GetKeySystem(LPWSTR* aKeySystem) {
  ENTRY_LOG();
  *aKeySystem = (LPWSTR)CoTaskMemAlloc((wcslen(kCLEARKEY_SYSTEM_NAME) + 1) *
                                       sizeof(wchar_t));
  if (*aKeySystem == NULL) {
    return E_OUTOFMEMORY;
  }
  wcscpy_s(*aKeySystem, wcslen(kCLEARKEY_SYSTEM_NAME) + 1,
           kCLEARKEY_SYSTEM_NAME);
  return S_OK;
}

}  // namespace mozilla
