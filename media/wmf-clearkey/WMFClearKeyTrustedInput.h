/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYTRUSTEDINPUT_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYTRUSTEDINPUT_H

#include <wrl.h>
#include <wrl/client.h>

#include "MFCDMExtra.h"
#include "RefCounted.h"

namespace mozilla {

class SessionManagerWrapper;

// This class is used to provide WMFClearKeyInputTrustAuthority.
class WMFClearKeyTrustedInput final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          IMFTrustedInput, Microsoft::WRL::FtmBase> {
 public:
  WMFClearKeyTrustedInput() = default;
  ~WMFClearKeyTrustedInput() = default;
  WMFClearKeyTrustedInput(const WMFClearKeyTrustedInput&) = delete;
  WMFClearKeyTrustedInput& operator=(const WMFClearKeyTrustedInput&) = delete;

  HRESULT RuntimeClassInitialize(SessionManagerWrapper* aSessionManager);

  // IMFTrustedInput
  STDMETHODIMP GetInputTrustAuthority(DWORD aStreamId, REFIID aRiid,
                                      IUnknown** aAuthority) override;

 private:
  RefPtr<SessionManagerWrapper> mSessionManager;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYTRUSTEDINPUT_H
