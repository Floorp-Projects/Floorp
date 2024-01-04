/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCONTENTENABLER_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCONTENTENABLER_H

#include <wrl.h>
#include <wrl/client.h>

#include "MFCDMExtra.h"

namespace mozilla {

// This class is used to return correct enable type.
class WMFClearKeyContentEnabler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentEnabler, Microsoft::WRL::FtmBase> {
 public:
  WMFClearKeyContentEnabler() = default;
  ~WMFClearKeyContentEnabler() = default;
  WMFClearKeyContentEnabler(const WMFClearKeyContentEnabler&) = delete;
  WMFClearKeyContentEnabler& operator=(const WMFClearKeyContentEnabler&) =
      delete;

  HRESULT RuntimeClassInitialize();

  // IMFContentEnabler
  STDMETHODIMP AutomaticEnable() override;
  STDMETHODIMP Cancel() override;
  STDMETHODIMP GetEnableData(BYTE** aData, DWORD* aDataSize) override;
  STDMETHODIMP GetEnableType(GUID* aType) override;
  STDMETHODIMP GetEnableURL(LPWSTR* aUrl, DWORD* aUrlSize,
                            MF_URL_TRUST_STATUS* aTrustStatus) override;
  STDMETHODIMP IsAutomaticSupported(BOOL* automatic) override;
  STDMETHODIMP MonitorEnable() override;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCONTENTENABLER_H
