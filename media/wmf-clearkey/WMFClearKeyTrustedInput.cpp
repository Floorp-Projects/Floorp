/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFClearKeyTrustedInput.h"

#include "WMFClearKeyCDM.h"
#include "WMFClearKeyInputTrustAuthority.h"
#include "WMFClearKeyUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

HRESULT WMFClearKeyTrustedInput::RuntimeClassInitialize(
    SessionManagerWrapper* aSessionManager) {
  ENTRY_LOG();
  mSessionManager = aSessionManager;
  return S_OK;
}

// IMFTrustedInput
STDMETHODIMP WMFClearKeyTrustedInput::GetInputTrustAuthority(
    DWORD aStreamId, REFIID aRiid, IUnknown** aAuthority) {
  ENTRY_LOG();
  ComPtr<IMFInputTrustAuthority> ita;
  RETURN_IF_FAILED((
      MakeAndInitialize<WMFClearKeyInputTrustAuthority, IMFInputTrustAuthority>(
          &ita, aStreamId, mSessionManager)));
  *aAuthority = ita.Detach();
  return S_OK;
}

}  // namespace mozilla
