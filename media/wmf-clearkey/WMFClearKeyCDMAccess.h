/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDMACCESS_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDMACCESS_H

#include <mfidl.h>
#include <windows.h>
#include <wrl.h>

#include "MFCDMExtra.h"

namespace mozilla {

// This class is used to create WMFClearKeyCDM.
class WMFClearKeyCDMAccess final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModuleAccess, Microsoft::WRL::FtmBase> {
 public:
  WMFClearKeyCDMAccess() = default;
  ~WMFClearKeyCDMAccess() = default;
  WMFClearKeyCDMAccess(const WMFClearKeyCDMAccess&) = delete;
  WMFClearKeyCDMAccess& operator=(const WMFClearKeyCDMAccess&) = delete;

  HRESULT RuntimeClassInitialize() { return S_OK; };

  // IMFContentDecryptionModuleAccess
  STDMETHODIMP CreateContentDecryptionModule(
      IPropertyStore* aProperties, IMFContentDecryptionModule** aCdm) override;
  STDMETHODIMP GetConfiguration(IPropertyStore** aConfig) override;
  STDMETHODIMP GetKeySystem(LPWSTR* aKeySystem) override;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDMACCESS_H
