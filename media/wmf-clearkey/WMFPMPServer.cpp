/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFPMPServer.h"

#include <mfapi.h>
#include <mferror.h>

#include "WMFClearKeyUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

HRESULT WMFPMPServer::RuntimeClassInitialize(
    ABI::Windows::Foundation::Collections::IPropertySet* aPropertyPmp) {
  ENTRY_LOG();
  mPropertyPmp = aPropertyPmp;
  RETURN_IF_FAILED(MFCreatePMPMediaSession(MFPMPSESSION_IN_PROCESS, nullptr,
                                           &mMediaSession, nullptr));
  RETURN_IF_FAILED(MFGetService(mMediaSession.Get(), MF_PMP_SERVER_CONTEXT,
                                IID_PPV_ARGS(&mPmpServer)));
  RETURN_IF_FAILED(MFGetService(mMediaSession.Get(), MF_PMP_SERVICE,
                                IID_PPV_ARGS(&mPmpHost)));
  return S_OK;
}

STDMETHODIMP WMFPMPServer::GetIids(ULONG* aIidCount, IID** aIids) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFPMPServer::GetRuntimeClassName(
    _COM_Outptr_ HSTRING* aClassName) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFPMPServer::GetTrustLevel(TrustLevel* aTrustLevel) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}
STDMETHODIMP WMFPMPServer::get_Properties(
    ABI::Windows::Foundation::Collections::IPropertySet** aPpProperties) {
  ENTRY_LOG();
  RETURN_IF_FAILED(mPropertyPmp.CopyTo(aPpProperties));
  return S_OK;
}

STDMETHODIMP WMFPMPServer::GetService(REFGUID aGuidService, REFIID aRiid,
                                      LPVOID* aObject) {
  ENTRY_LOG();
  if (!aObject) {
    return E_POINTER;
  }
  if (aGuidService == MF_PMP_SERVER_CONTEXT) {
    RETURN_IF_FAILED(mPmpServer.CopyTo(aRiid, aObject));
  } else if (aGuidService == MF_PMP_SERVICE && aRiid == IID_IMFPMPHost) {
    RETURN_IF_FAILED(mPmpHost.CopyTo(aRiid, aObject));
  } else {
    RETURN_IF_FAILED(MF_E_UNSUPPORTED_SERVICE);
  }
  return S_OK;
}

}  // namespace mozilla
