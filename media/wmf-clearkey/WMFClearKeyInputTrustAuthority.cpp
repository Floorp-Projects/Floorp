/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFClearKeyInputTrustAuthority.h"

#include <mfapi.h>
#include <mferror.h>

#include "WMFClearKeyActivate.h"
#include "WMFClearKeyCDM.h"
// #include "WMFClearKeyDecryptor.h"
#include "WMFClearKeyUtils.h"
#include "WMFClearKeyOutputPolicy.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

HRESULT WMFClearKeyInputTrustAuthority::RuntimeClassInitialize(
    UINT32 aStreamId, SessionManagerWrapper* aSessionManager) {
  ENTRY_LOG();
  mSessionManager = aSessionManager;
  return S_OK;
}

STDMETHODIMP WMFClearKeyInputTrustAuthority::GetDecrypter(REFIID aRiid,
                                                          void** aPpv) {
  ENTRY_LOG();
  ComPtr<IMFTransform> decryptor;
  // TODO : As currently we are still not able to make decryption working in the
  // media foundation pipeline, we will finish the implementation for
  // WMFClearKeyDecryptor later.
  // RETURN_IF_FAILED((MakeAndInitialize<WMFClearKeyDecryptor, IMFTransform>(
  //     &decryptor, mSessionManager)));
  RETURN_IF_FAILED(decryptor.CopyTo(aRiid, aPpv));
  return S_OK;
}

STDMETHODIMP WMFClearKeyInputTrustAuthority::RequestAccess(
    MFPOLICYMANAGER_ACTION aAction, IMFActivate** aContentEnablerActivate) {
  ENTRY_LOG_ARGS("aAction=%d", aAction);
  // The ITA only allows the PLAY, EXTRACT and NO actions
  // NOTE: Topology created only on the basis of EXTRACT or NO action will NOT
  // decrypt content.
  if (PEACTION_EXTRACT == aAction || PEACTION_NO == aAction) {
    return S_OK;
  }
  if (PEACTION_PLAY != aAction) {
    ENTRY_LOG_ARGS("Unsupported action");
    return MF_E_ITA_UNSUPPORTED_ACTION;
  }
  ComPtr<IMFActivate> activate;
  RETURN_IF_FAILED(MakeAndInitialize<WMFClearKeyActivate>(&activate));
  *aContentEnablerActivate = activate.Detach();
  return S_OK;
}

STDMETHODIMP WMFClearKeyInputTrustAuthority::GetPolicy(
    MFPOLICYMANAGER_ACTION aAction, IMFOutputPolicy** aPolicy) {
  ENTRY_LOG();
  // For testing purpose, we don't need to set the output policy/
  *aPolicy = nullptr;
  return S_OK;
}

STDMETHODIMP WMFClearKeyInputTrustAuthority::BindAccess(
    MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS* aParams) {
  ENTRY_LOG();
  if (aParams == nullptr || aParams->dwVer != 0) {
    return E_INVALIDARG;
  }
  for (DWORD i = 0; i < aParams->cActions; ++i) {
    MFPOLICYMANAGER_ACTION action = aParams->rgOutputActions[i].Action;
    if (action != PEACTION_PLAY && action != PEACTION_EXTRACT &&
        action != PEACTION_NO) {
      ENTRY_LOG_ARGS("Unexpected action!");
      return MF_E_UNEXPECTED;
    }
  }
  return S_OK;
}

STDMETHODIMP WMFClearKeyInputTrustAuthority::UpdateAccess(
    MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS* aParams) {
  ENTRY_LOG();
  return BindAccess(aParams);
}

STDMETHODIMP WMFClearKeyInputTrustAuthority::Reset() {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyInputTrustAuthority::GetShutdownStatus(
    MFSHUTDOWN_STATUS* aStatus) {
  ENTRY_LOG();
  // https://learn.microsoft.com/en-us/windows/win32/api/mfidl/nf-mfidl-imfshutdown-getshutdownstatus#return-value
  if (mSessionManager->IsShutdown()) {
    return MF_E_INVALIDREQUEST;
  }
  return S_OK;
}

STDMETHODIMP WMFClearKeyInputTrustAuthority::Shutdown() {
  ENTRY_LOG();
  if (mSessionManager->IsShutdown()) {
    return MF_E_SHUTDOWN;
  }
  mSessionManager->Shutdown();
  return S_OK;
}

}  // namespace mozilla
