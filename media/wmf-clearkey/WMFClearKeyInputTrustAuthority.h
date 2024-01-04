/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYINPUTTRUSTAUTHORITY_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYINPUTTRUSTAUTHORITY_H

#include <wrl.h>
#include <wrl/client.h>

#include "MFCDMExtra.h"
#include "RefCounted.h"

namespace mozilla {

class SessionManagerWrapper;

// This class is used to provide WMFClearKeyActivate and WMFClearKeyDecryptor.
class WMFClearKeyInputTrustAuthority final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFInputTrustAuthority, IMFShutdown, Microsoft::WRL::FtmBase> {
 public:
  WMFClearKeyInputTrustAuthority() = default;
  ~WMFClearKeyInputTrustAuthority() = default;
  WMFClearKeyInputTrustAuthority(const WMFClearKeyInputTrustAuthority&) =
      delete;
  WMFClearKeyInputTrustAuthority& operator=(
      const WMFClearKeyInputTrustAuthority&) = delete;

  HRESULT RuntimeClassInitialize(UINT32 aStreamId,
                                 SessionManagerWrapper* aSessionManager);

  // IMFInputTrustAuthority
  STDMETHODIMP GetDecrypter(REFIID aRiid, void** aPpv) override;
  STDMETHODIMP RequestAccess(MFPOLICYMANAGER_ACTION aAction,
                             IMFActivate** aContentEnablerActivate) override;
  STDMETHODIMP GetPolicy(MFPOLICYMANAGER_ACTION aAction,
                         IMFOutputPolicy** aPolicy) override;
  STDMETHODIMP BindAccess(
      MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS* aParams) override;
  STDMETHODIMP UpdateAccess(
      MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS* aParams) override;
  STDMETHODIMP Reset() override;

  // IMFShutdown
  STDMETHODIMP GetShutdownStatus(MFSHUTDOWN_STATUS* aStatus) override;
  STDMETHODIMP Shutdown() override;

 private:
  RefPtr<SessionManagerWrapper> mSessionManager;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYINPUTTRUSTAUTHORITY_H
