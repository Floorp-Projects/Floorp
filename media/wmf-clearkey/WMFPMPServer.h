/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFPMPSERVER_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFPMPSERVER_H

#include <mfidl.h>
#include <windows.h>
#include <windows.media.protection.h>
#include <wrl.h>

#include "MFCDMExtra.h"

namespace mozilla {

// A mock PMP server in order to allow media engine to create in-process PMP
// server.
class WMFPMPServer final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ABI::Windows::Media::Protection::IMediaProtectionPMPServer,
          IMFGetService, Microsoft::WRL::FtmBase> {
 public:
  WMFPMPServer() = default;
  ~WMFPMPServer() = default;
  WMFPMPServer(const WMFPMPServer&) = delete;
  WMFPMPServer& operator=(const WMFPMPServer&) = delete;

  HRESULT RuntimeClassInitialize(
      ABI::Windows::Foundation::Collections::IPropertySet* aPropertyPmp);

  // IInspectable
  STDMETHODIMP GetIids(ULONG* aIidCount, IID** aIids) override;
  STDMETHODIMP GetRuntimeClassName(HSTRING* aClassName) override;
  STDMETHODIMP GetTrustLevel(TrustLevel* aTrustLevel) override;

  // IMediaProtectionPMPServer
  STDMETHODIMP get_Properties(
      ABI::Windows::Foundation::Collections::IPropertySet** aPpProperties)
      override;

  // IMFGetService
  STDMETHODIMP GetService(REFGUID aGuidService, REFIID aRiid,
                          LPVOID* aObject) override;

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IPropertySet>
      mPropertyPmp;
  Microsoft::WRL::ComPtr<IMFPMPServer> mPmpServer;
  Microsoft::WRL::ComPtr<IMFPMPHost> mPmpHost;
  Microsoft::WRL::ComPtr<IMFMediaSession> mMediaSession;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFPMPSERVER_H
