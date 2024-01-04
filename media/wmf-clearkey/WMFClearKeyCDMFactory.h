/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDMFACTORY_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDMFACTORY_H

#include <mfidl.h>
#include <windows.h>
#include <wrl.h>

#include "MFCDMExtra.h"

namespace mozilla {

// This class is used to return WMFClearKeyCDMAccess and answer what kind of key
// system is supported.
class WMFClearKeyCDMFactory final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          Microsoft::WRL::CloakedIid<IMFContentDecryptionModuleFactory>,
          Microsoft::WRL::FtmBase> {
  InspectableClass(L"org.w3.clearkey", BaseTrust);

 public:
  WMFClearKeyCDMFactory() = default;
  ~WMFClearKeyCDMFactory() = default;
  WMFClearKeyCDMFactory(const WMFClearKeyCDMFactory&) = delete;
  WMFClearKeyCDMFactory& operator=(const WMFClearKeyCDMFactory&) = delete;

  HRESULT RuntimeClassInitialize() { return S_OK; }

  // IMFContentDecryptionModuleFactory
  STDMETHODIMP_(BOOL)
  IsTypeSupported(LPCWSTR aKeySystem, LPCWSTR aContentType) override;

  STDMETHODIMP CreateContentDecryptionModuleAccess(
      LPCWSTR aKeySystem, IPropertyStore** aConfigurations,
      DWORD aNumConfigurations,
      IMFContentDecryptionModuleAccess** aCdmAccess) override;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDMFACTORY_H
