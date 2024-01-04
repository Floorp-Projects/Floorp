/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFClearKeySession.h"

#include <codecvt>
#include <cmath>
#include <Mferror.h>
#include <winerror.h>

#include "WMFClearKeyCDM.h"
#include "WMFClearKeyUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;

static cdm::SessionType ToCdmSessionType(MF_MEDIAKEYSESSION_TYPE aSessionType) {
  switch (aSessionType) {
    case MF_MEDIAKEYSESSION_TYPE_TEMPORARY:
      return cdm::SessionType::kTemporary;
    default:
      MOZ_ASSERT_UNREACHABLE("Only support temporary type for testing");
      return cdm::SessionType::kTemporary;
  }
}

static MF_MEDIAKEY_STATUS ToMFKeyStatus(cdm::KeyStatus aStatus) {
  switch (aStatus) {
    case cdm::KeyStatus::kUsable:
      return MF_MEDIAKEY_STATUS_USABLE;
    case cdm::KeyStatus::kExpired:
      return MF_MEDIAKEY_STATUS_EXPIRED;
    case cdm::KeyStatus::kOutputDownscaled:
      return MF_MEDIAKEY_STATUS_OUTPUT_DOWNSCALED;
    case cdm::KeyStatus::kStatusPending:
      return MF_MEDIAKEY_STATUS_STATUS_PENDING;
    case cdm::KeyStatus::kInternalError:
      return MF_MEDIAKEY_STATUS_INTERNAL_ERROR;
    case cdm::KeyStatus::kReleased:
      return MF_MEDIAKEY_STATUS_RELEASED;
    case cdm::KeyStatus::kOutputRestricted:
      return MF_MEDIAKEY_STATUS_OUTPUT_RESTRICTED;
  }
}

HRESULT WMFClearKeySession::RuntimeClassInitialize(
    MF_MEDIAKEYSESSION_TYPE aSessionType,
    IMFContentDecryptionModuleSessionCallbacks* aCallbacks,
    SessionManagerWrapper* aManager) {
  ENTRY_LOG();
  MOZ_ASSERT(aCallbacks);
  MOZ_ASSERT(aManager);
  mSessionType = ToCdmSessionType(aSessionType);
  mCallbacks = aCallbacks;
  mSessionManager = aManager;
  return S_OK;
}

WMFClearKeySession::~WMFClearKeySession() { ENTRY_LOG(); }

STDMETHODIMP WMFClearKeySession::GenerateRequest(LPCWSTR aInitDataType,
                                                 const BYTE* aInitData,
                                                 DWORD aInitDataSize) {
  if (!mSessionManager) {
    return MF_E_SHUTDOWN;
  }
  ENTRY_LOG_ARGS("init-type=%ls", aInitDataType);
  cdm::InitDataType initType;
  if (wcscmp(aInitDataType, L"cenc") == 0) {
    initType = cdm::InitDataType::kCenc;
  } else if (wcscmp(aInitDataType, L"keyids") == 0) {
    initType = cdm::InitDataType::kKeyIds;
  } else if (wcscmp(aInitDataType, L"webm") == 0) {
    initType = cdm::InitDataType::kWebM;
  } else {
    return MF_NOT_SUPPORTED_ERR;
  }
  RETURN_IF_FAILED(mSessionManager->GenerateRequest(
      initType, aInitData, aInitDataSize, mSessionType, this, mSessionId));
  MOZ_ASSERT(!mSessionId.empty());
  ENTRY_LOG_ARGS("created session, sid=%s", mSessionId.c_str());
  return S_OK;
}

STDMETHODIMP WMFClearKeySession::Load(LPCWSTR session_id, BOOL* loaded) {
  ENTRY_LOG();
  // Per step 5, Load can only be called on persistent session, but we only
  // support temporary session for MF clearkey due to testing reason.
  // https://rawgit.com/w3c/encrypted-media/V1/index.html#dom-mediakeysession-load
  return MF_E_NOT_AVAILABLE;
}

STDMETHODIMP WMFClearKeySession::Update(const BYTE* aResponse,
                                        DWORD aResponseSize) {
  ENTRY_LOG();
  if (!mSessionManager) {
    return MF_E_SHUTDOWN;
  }
  RETURN_IF_FAILED(
      mSessionManager->UpdateSession(mSessionId, aResponse, aResponseSize));
  return S_OK;
}

STDMETHODIMP WMFClearKeySession::Close() {
  ENTRY_LOG();
  // It has been shutdowned and sesssion has been closed.
  if (!mSessionManager) {
    return S_OK;
  }
  RETURN_IF_FAILED(mSessionManager->CloseSession(mSessionId));
  return S_OK;
}

STDMETHODIMP WMFClearKeySession::Remove() {
  ENTRY_LOG();
  // It has been shutdowned and sesssion has been removed.
  if (!mSessionManager) {
    return S_OK;
  }
  RETURN_IF_FAILED(mSessionManager->RemoveSession(mSessionId));
  return S_OK;
}

STDMETHODIMP WMFClearKeySession::GetSessionId(LPWSTR* aSessionId) {
  ENTRY_LOG();
  if (!mSessionManager) {
    return S_OK;
  }
  if (mSessionId.empty()) {
    *aSessionId = (LPWSTR)CoTaskMemAlloc(sizeof(wchar_t));
    if (*aSessionId != NULL) {
      **aSessionId = L'\0';
      return S_OK;
    } else {
      return E_OUTOFMEMORY;
    }
  }

  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  std::wstring wideStr = converter.from_bytes(mSessionId);
  *aSessionId =
      (LPWSTR)CoTaskMemAlloc((wideStr.length() + 1) * sizeof(wchar_t));
  if (*aSessionId != NULL) {
    wcscpy_s(*aSessionId, wideStr.length() + 1, wideStr.c_str());
    return S_OK;
  } else {
    return E_OUTOFMEMORY;
  }
}

STDMETHODIMP WMFClearKeySession::GetExpiration(double* expiration) {
  ENTRY_LOG();
  // This is used for testing, never expires.
  *expiration = std::nan("1");
  return S_OK;
}

STDMETHODIMP WMFClearKeySession::GetKeyStatuses(MFMediaKeyStatus** aKeyStatuses,
                                                UINT* aKeyStatusesCount) {
  ENTRY_LOG();
  *aKeyStatuses = nullptr;
  *aKeyStatusesCount = 0;

  const auto keyStatusCount = mKeyInfo.size();
  if (mSessionId.empty() || keyStatusCount == 0) {
    ENTRY_LOG_ARGS("No session ID or no key info!");
    return S_OK;
  }

  MFMediaKeyStatus* keyStatusArray = nullptr;
  keyStatusArray = static_cast<MFMediaKeyStatus*>(
      CoTaskMemAlloc(keyStatusCount * sizeof(MFMediaKeyStatus)));
  if (!keyStatusArray) {
    ENTRY_LOG_ARGS("OOM when alloacting keyStatusArray!");
    return E_OUTOFMEMORY;
  }
  ZeroMemory(keyStatusArray, keyStatusCount * sizeof(MFMediaKeyStatus));
  for (UINT idx = 0; idx < keyStatusCount; idx++) {
    keyStatusArray[idx].cbKeyId = mKeyInfo[idx].mKeyId.size();
    keyStatusArray[idx].pbKeyId =
        static_cast<BYTE*>(CoTaskMemAlloc(sizeof(mKeyInfo[idx].mKeyId.size())));
    if (keyStatusArray[idx].pbKeyId == nullptr) {
      ENTRY_LOG_ARGS("OOM when alloacting keyStatusArray's pbKeyId!");
      return E_OUTOFMEMORY;
    }

    keyStatusArray[idx].eMediaKeyStatus =
        ToMFKeyStatus(mKeyInfo[idx].mKeyStatus);
    memcpy(keyStatusArray[idx].pbKeyId, mKeyInfo[idx].mKeyId.data(),
           mKeyInfo[idx].mKeyId.size());
  }

  *aKeyStatuses = keyStatusArray;
  *aKeyStatusesCount = keyStatusCount;
  ENTRY_LOG_ARGS("return key status!");
  return S_OK;
}

void WMFClearKeySession::OnKeyMessage(
    MF_MEDIAKEYSESSION_MESSAGETYPE aMessageType, const BYTE* aMessage,
    DWORD aMessageSize) {
  ENTRY_LOG_ARGS("aMessageSize=%lu", aMessageSize);
  if (!mSessionManager) {
    return;
  }
  mCallbacks->KeyMessage(aMessageType, aMessage, aMessageSize, nullptr);
}

void WMFClearKeySession::OnKeyStatusChanged(
    const cdm::KeyInformation* aKeysInfo, uint32_t aKeysInfoCount) {
  ENTRY_LOG_ARGS("aKeysInfoCount=%u", aKeysInfoCount);
  if (!mSessionManager) {
    return;
  }
  mKeyInfo.clear();
  for (uint32_t idx = 0; idx < aKeysInfoCount; idx++) {
    const cdm::KeyInformation& key = aKeysInfo[idx];
    mKeyInfo.push_back(KeyInformation{key.key_id, key.key_id_size, key.status});
    ENTRY_LOG_ARGS("idx=%u, keySize=%u, status=%u", idx, key.key_id_size,
                   key.status);
  }
  mCallbacks->KeyStatusChanged();
}

void WMFClearKeySession::Shutdown() {
  ENTRY_LOG();
  mCallbacks = nullptr;
  mSessionManager = nullptr;
}

}  // namespace mozilla
