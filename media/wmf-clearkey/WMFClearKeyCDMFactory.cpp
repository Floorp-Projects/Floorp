/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFClearKeyCDMFactory.h"

#include <Mferror.h>

#include "WMFClearKeyCDMAccess.h"
#include "WMFClearKeyUtils.h"

namespace mozilla {

using Microsoft::WRL::MakeAndInitialize;

ActivatableClass(WMFClearKeyCDMFactory);

STDMETHODIMP_(BOOL)
WMFClearKeyCDMFactory::IsTypeSupported(_In_ LPCWSTR aKeySystem,
                                       _In_opt_ LPCWSTR aContentType) {
  // For testing, always return support. Maybe we can block support for certain
  // type if needed?
  return true;
}

STDMETHODIMP
WMFClearKeyCDMFactory::CreateContentDecryptionModuleAccess(
    LPCWSTR aKeySystem, IPropertyStore** aConfigurations,
    DWORD aNumConfigurations, IMFContentDecryptionModuleAccess** aCdmAccess) {
  ENTRY_LOG();
  if (aKeySystem == nullptr || aKeySystem[0] == L'\0') {
    ENTRY_LOG_ARGS("Key system is null or empty");
    return MF_TYPE_ERR;
  }

  if (aNumConfigurations == 0) {
    ENTRY_LOG_ARGS("No available configration");
    return MF_TYPE_ERR;
  }

  if (!IsTypeSupported(aKeySystem, nullptr)) {
    ENTRY_LOG_ARGS("Not supported type");
    return MF_NOT_SUPPORTED_ERR;
  }

  RETURN_IF_FAILED((
      MakeAndInitialize<WMFClearKeyCDMAccess, IMFContentDecryptionModuleAccess>(
          aCdmAccess)));
  ENTRY_LOG_ARGS("Created CDM access!");
  return S_OK;
}

#undef LOG

}  // namespace mozilla
