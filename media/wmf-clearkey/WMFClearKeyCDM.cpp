/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFClearKeyCDM.h"

#include <Mferror.h>
#include <mfapi.h>
#include <oleauto.h>
#include <optional>
#include <windows.h>
#include <windows.media.h>

#include "WMFClearKeyTrustedInput.h"
#include "WMFClearKeySession.h"
#include "WMFDecryptedBlock.h"
#include "WMFPMPServer.h"

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

static HRESULT AddPropertyToSet(
    ABI::Windows::Foundation::Collections::IPropertySet* aPropertySet,
    LPCWSTR aName, IInspectable* aInspectable) {
  boolean replaced = false;
  ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable*>>
      map;
  RETURN_IF_FAILED(aPropertySet->QueryInterface(IID_PPV_ARGS(&map)));
  RETURN_IF_FAILED(
      map->Insert(Microsoft::WRL::Wrappers::HStringReference(aName).Get(),
                  aInspectable, &replaced));
  return S_OK;
}

static HRESULT AddStringToPropertySet(
    ABI::Windows::Foundation::Collections::IPropertySet* aPropertySet,
    LPCWSTR aName, LPCWSTR aString) {
  ComPtr<ABI::Windows::Foundation::IPropertyValue> propertyValue;
  ComPtr<ABI::Windows::Foundation::IPropertyValueStatics> propertyValueStatics;
  RETURN_IF_FAILED(ABI::Windows::Foundation::GetActivationFactory(
      Microsoft::WRL::Wrappers::HStringReference(
          RuntimeClass_Windows_Foundation_PropertyValue)
          .Get(),
      &propertyValueStatics));
  RETURN_IF_FAILED(propertyValueStatics->CreateString(
      Microsoft::WRL::Wrappers::HStringReference(aString).Get(),
      &propertyValue));
  RETURN_IF_FAILED(AddPropertyToSet(aPropertySet, aName, propertyValue.Get()));
  return S_OK;
}

static HRESULT AddBoolToPropertySet(
    ABI::Windows::Foundation::Collections::IPropertySet* aPropertySet,
    LPCWSTR aName, BOOL aValue) {
  ComPtr<ABI::Windows::Foundation::IPropertyValue> propertyValue;
  ComPtr<ABI::Windows::Foundation::IPropertyValueStatics> propertyValueStatics;
  RETURN_IF_FAILED(ABI::Windows::Foundation::GetActivationFactory(
      Microsoft::WRL::Wrappers::HStringReference(
          RuntimeClass_Windows_Foundation_PropertyValue)
          .Get(),
      &propertyValueStatics));
  RETURN_IF_FAILED(
      propertyValueStatics->CreateBoolean(!!aValue, &propertyValue));
  RETURN_IF_FAILED(AddPropertyToSet(aPropertySet, aName, propertyValue.Get()));
  return S_OK;
}

MF_MEDIAKEYSESSION_MESSAGETYPE ToMFMessageType(cdm::MessageType aMessageType) {
  switch (aMessageType) {
    case cdm::MessageType::kLicenseRequest:
      return MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST;
    case cdm::MessageType::kLicenseRenewal:
      return MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RENEWAL;
    case cdm::MessageType::kLicenseRelease:
      return MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RELEASE;
    case cdm::MessageType::kIndividualizationRequest:
      return MF_MEDIAKEYSESSION_MESSAGETYPE_INDIVIDUALIZATION_REQUEST;
  }
}

namespace mozilla {

HRESULT WMFClearKeyCDM::RuntimeClassInitialize(IPropertyStore* aProperties) {
  ENTRY_LOG();
  // A workaround in order to create an in-process PMP server regardless of the
  // system's HWDRM capability. As accoriding to Microsoft, only PlayReady is
  // supported for the in-process PMP so we pretend ourselves as a PlayReady
  // CDM for the PMP server.
  ComPtr<ABI::Windows::Foundation::Collections::IPropertySet> propertyPmp;
  RETURN_IF_FAILED(Windows::Foundation::ActivateInstance(
      Microsoft::WRL::Wrappers::HStringReference(
          RuntimeClass_Windows_Foundation_Collections_PropertySet)
          .Get(),
      &propertyPmp));
  RETURN_IF_FAILED(AddStringToPropertySet(
      propertyPmp.Get(), L"Windows.Media.Protection.MediaProtectionSystemId",
      PLAYREADY_GUID_MEDIA_PROTECTION_SYSTEM_ID_STRING));
  RETURN_IF_FAILED(AddBoolToPropertySet(
      propertyPmp.Get(), L"Windows.Media.Protection.UseHardwareProtectionLayer",
      TRUE));
  RETURN_IF_FAILED((MakeAndInitialize<
                    WMFPMPServer,
                    ABI::Windows::Media::Protection::IMediaProtectionPMPServer>(
      &mPMPServer, propertyPmp.Get())));

  mSessionManager = new SessionManagerWrapper(this);
  return S_OK;
}

WMFClearKeyCDM::~WMFClearKeyCDM() { ENTRY_LOG(); }

STDMETHODIMP WMFClearKeyCDM::SetContentEnabler(
    IMFContentEnabler* aContentEnabler, IMFAsyncResult* aResult) {
  ENTRY_LOG();
  if (!aContentEnabler || !aResult) {
    return E_INVALIDARG;
  }
  // Invoke the callback immediately but will determine whether the keyid exists
  // or not in the decryptor's ProcessOutput().
  RETURN_IF_FAILED(MFInvokeCallback(aResult));
  return S_OK;
}

STDMETHODIMP WMFClearKeyCDM::SetPMPHostApp(IMFPMPHostApp* aPmpHostApp) {
  ENTRY_LOG();
  // Simply return S_OK and ignore IMFPMPHostApp, which is only used for the
  // out-of-process PMP.
  return S_OK;
}

STDMETHODIMP WMFClearKeyCDM::CreateSession(
    MF_MEDIAKEYSESSION_TYPE aSessionType,
    IMFContentDecryptionModuleSessionCallbacks* aCallbacks,
    IMFContentDecryptionModuleSession** aSession) {
  ENTRY_LOG();
  RETURN_IF_FAILED(
      (MakeAndInitialize<WMFClearKeySession, IMFContentDecryptionModuleSession>(
          aSession, aSessionType, aCallbacks, mSessionManager)));
  return S_OK;
}

STDMETHODIMP WMFClearKeyCDM::CreateTrustedInput(
    const BYTE* aContentInitData, DWORD aContentInitDataSize,
    IMFTrustedInput** aTrustedInput) {
  ENTRY_LOG();
  ComPtr<IMFTrustedInput> trustedInput;
  RETURN_IF_FAILED((MakeAndInitialize<WMFClearKeyTrustedInput, IMFTrustedInput>(
      &trustedInput, mSessionManager)));
  *aTrustedInput = trustedInput.Detach();
  return S_OK;
}

STDMETHODIMP WMFClearKeyCDM::GetProtectionSystemIds(GUID** aSystemIds,
                                                    DWORD* aCount) {
  ENTRY_LOG();
  GUID* systemId = static_cast<GUID*>(CoTaskMemAlloc(sizeof(GUID)));
  if (!systemId) {
    return E_OUTOFMEMORY;
  }
  *systemId = CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID;
  *aSystemIds = systemId;
  *aCount = 1;
  return S_OK;
}

STDMETHODIMP WMFClearKeyCDM::GetService(REFGUID aGuidService, REFIID aRiid,
                                        LPVOID* aPpvObject) {
  ENTRY_LOG();
  if (MF_CONTENTDECRYPTIONMODULE_SERVICE != aGuidService) {
    ENTRY_LOG_ARGS("unsupported guid!");
    return MF_E_UNSUPPORTED_SERVICE;
  }
  if (!mPMPServer) {
    ENTRY_LOG_ARGS("no PMP server!");
    return MF_INVALID_STATE_ERR;
  }
  if (aRiid == ABI::Windows::Media::Protection::IID_IMediaProtectionPMPServer) {
    RETURN_IF_FAILED(mPMPServer.CopyTo(aRiid, aPpvObject));
  } else {
    ComPtr<IMFGetService> getService;
    RETURN_IF_FAILED(mPMPServer.As(&getService));
    RETURN_IF_FAILED(getService->GetService(MF_PMP_SERVICE, aRiid, aPpvObject));
  }
  return S_OK;
}

STDMETHODIMP WMFClearKeyCDM::GetSuspendNotify(IMFCdmSuspendNotify** aNotify) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyCDM::SetServerCertificate(const BYTE* aCertificate,
                                                  DWORD aCertificateSize) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyCDM::Shutdown() {
  ENTRY_LOG();
  mSessionManager->Shutdown();
  return S_OK;
}

STDMETHODIMP WMFClearKeyCDM::GetShutdownStatus(MFSHUTDOWN_STATUS* aStatus) {
  ENTRY_LOG();
  // https://learn.microsoft.com/en-us/windows/win32/api/mfidl/nf-mfidl-imfshutdown-getshutdownstatus#return-value
  if (mSessionManager->IsShutdown()) {
    return MF_E_INVALIDREQUEST;
  }
  return S_OK;
}

// SessionManagerWrapper

SessionManagerWrapper::SessionManagerWrapper(WMFClearKeyCDM* aCDM)
    : mOwnerCDM(aCDM), mSessionManager(new ClearKeySessionManager(this)) {
  // For testing, we don't care about these.
  mSessionManager->Init(false /* aDistinctiveIdentifierAllowed */,
                        false /* aPersistentStateAllowed*/);
}

SessionManagerWrapper::~SessionManagerWrapper() { ENTRY_LOG(); }

// Callback methods
void SessionManagerWrapper::OnResolveNewSessionPromise(
    uint32_t aPromiseId, const char* aSessionId, uint32_t aSessionIdSize) {
  if (auto rv = mActiveSyncResultChecker.find(aPromiseId);
      rv != mActiveSyncResultChecker.end()) {
    LOG("Generated request (promise-id=%u, sessionId=%s)", aPromiseId,
        aSessionId);
    rv->second->SetResultConstChar(aSessionId);
    std::string sessionId{aSessionId};
    MOZ_ASSERT(mSessions.find(sessionId) == mSessions.end());
    mSessions[sessionId] = rv->second->GetKeySession();
  }
}

void SessionManagerWrapper::OnResolvePromise(uint32_t aPromiseId) {
  if (auto rv = mActiveSyncResultChecker.find(aPromiseId);
      rv != mActiveSyncResultChecker.end()) {
    LOG("Resolved promise (promise-id=%u)", aPromiseId);
    rv->second->SetResultBool(true);
  }
}

void SessionManagerWrapper::OnRejectPromise(uint32_t aPromiseId,
                                            cdm::Exception aException,
                                            uint32_t aSystemCode,
                                            const char* aErrorMessage,
                                            uint32_t aErrorMessageSize) {
  if (auto rv = mActiveSyncResultChecker.find(aPromiseId);
      rv != mActiveSyncResultChecker.end()) {
    LOG("Rejected promise (promise-id=%u)", aPromiseId);
    rv->second->SetResultBool(false);
  }
}

HRESULT SessionManagerWrapper::GenerateRequest(cdm::InitDataType aInitDataType,
                                               const BYTE* aInitData,
                                               DWORD aInitDataSize,
                                               cdm::SessionType aSessionType,
                                               WMFClearKeySession* aSession,
                                               std::string& aSessionIdOut) {
  ENTRY_LOG();
  MOZ_DIAGNOSTIC_ASSERT(aSessionType == cdm::SessionType::kTemporary);
  SyncResultChecker checker(*this, aSession);
  mSessionManager->CreateSession(checker.GetPromiseId(), aInitDataType,
                                 aInitData, aInitDataSize, aSessionType);
  auto* rv = std::get_if<const char*>(&checker.GetResult());
  if (!rv) {
    LOG("Failed to generate request, no session Id!");
    return E_FAIL;
  }
  aSessionIdOut = std::string(*rv);
  return S_OK;
}

HRESULT SessionManagerWrapper::UpdateSession(const std::string& aSessionId,
                                             const BYTE* aResponse,
                                             DWORD aResponseSize) {
  ENTRY_LOG();
  SyncResultChecker checker(*this);
  mSessionManager->UpdateSession(checker.GetPromiseId(), aSessionId.c_str(),
                                 aSessionId.size(), aResponse, aResponseSize);
  auto* rv = std::get_if<bool>(&checker.GetResult());
  return rv && *rv ? S_OK : E_FAIL;
}

HRESULT SessionManagerWrapper::CloseSession(const std::string& aSessionId) {
  ENTRY_LOG();
  SyncResultChecker checker(*this);
  mSessionManager->CloseSession(checker.GetPromiseId(), aSessionId.c_str(),
                                aSessionId.size());
  auto* rv = std::get_if<bool>(&checker.GetResult());
  return rv && *rv ? S_OK : E_FAIL;
}

HRESULT SessionManagerWrapper::RemoveSession(const std::string& aSessionId) {
  ENTRY_LOG();
  SyncResultChecker checker(*this);
  MOZ_ASSERT(mSessions.find(aSessionId) != mSessions.end());
  mSessions.erase(aSessionId);
  mSessionManager->RemoveSession(checker.GetPromiseId(), aSessionId.c_str(),
                                 aSessionId.size());
  auto* rv = std::get_if<bool>(&checker.GetResult());
  return rv && *rv ? S_OK : E_FAIL;
}

HRESULT SessionManagerWrapper::Decrypt(const cdm::InputBuffer_2& aBuffer,
                                       cdm::DecryptedBlock* aDecryptedBlock) {
  ENTRY_LOG();
  // From MF thread pool.
  std::lock_guard<std::mutex> lock(mMutex);
  if (mIsShutdown) {
    return MF_E_SHUTDOWN;
  }
  auto rv = mSessionManager->Decrypt(aBuffer, aDecryptedBlock);
  return rv == cdm::kSuccess ? S_OK : E_FAIL;
}

void SessionManagerWrapper::OnSessionMessage(const char* aSessionId,
                                             uint32_t aSessionIdSize,
                                             cdm::MessageType aMessageType,
                                             const char* aMessage,
                                             uint32_t aMessageSize) {
  ENTRY_LOG_ARGS("sessionId=%s, sz=%u", aSessionId, aSessionIdSize);
  std::string sessionId(aSessionId);
  MOZ_ASSERT(mSessions.find(sessionId) != mSessions.end());
  mSessions[sessionId]->OnKeyMessage(ToMFMessageType(aMessageType),
                                     reinterpret_cast<const BYTE*>(aMessage),
                                     aMessageSize);
}

void SessionManagerWrapper::OnSessionKeysChange(
    const char* aSessionId, uint32_t aSessionIdSize,
    bool aHasAdditionalUsableKey, const cdm::KeyInformation* aKeysInfo,
    uint32_t aKeysInfoCount) {
  ENTRY_LOG_ARGS("sessionId=%s, sz=%u", aSessionId, aSessionIdSize);
  std::string sessionId(aSessionId);
  MOZ_ASSERT(mSessions.find(sessionId) != mSessions.end());
  mSessions[sessionId]->OnKeyStatusChanged(aKeysInfo, aKeysInfoCount);
}

// `ClearKeySessionManager::Decrypt` will call this method to allocate buffer
// for decryted data.
cdm::Buffer* SessionManagerWrapper::Allocate(uint32_t aCapacity) {
  ENTRY_LOG_ARGS("capacity=%u", aCapacity);
  return new WMFDecryptedBuffer(aCapacity);
}

void SessionManagerWrapper::Shutdown() {
  ENTRY_LOG();
  std::lock_guard<std::mutex> lock(mMutex);
  if (mIsShutdown) {
    return;
  }
  mOwnerCDM = nullptr;
  for (const auto& session : mSessions) {
    session.second->Shutdown();
  }
  mSessions.clear();
  mSessionManager = nullptr;
  mIsShutdown = true;
}

bool SessionManagerWrapper::IsShutdown() {
  std::lock_guard<std::mutex> lock(mMutex);
  return mIsShutdown;
}

}  // namespace mozilla
