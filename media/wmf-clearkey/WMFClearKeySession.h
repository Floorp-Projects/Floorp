/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYSESSION_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYSESSION_H

#include <mfidl.h>
#include <windows.h>
#include <wrl.h>

#include "content_decryption_module.h"
#include "MFCDMExtra.h"
#include "RefCounted.h"
#include "WMFClearKeyUtils.h"

namespace mozilla {

class SessionManagerWrapper;

// This is the implementation for EME API's MediaKeySession.
// TODO : add a way to assert thread usage. It would only be used on the media
// supervisor thread pool.
class WMFClearKeySession final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModuleSession, Microsoft::WRL::FtmBase> {
 public:
  WMFClearKeySession() = default;
  ~WMFClearKeySession();
  WMFClearKeySession(const WMFClearKeySession&) = delete;
  WMFClearKeySession& operator=(const WMFClearKeySession&) = delete;

  HRESULT RuntimeClassInitialize(
      MF_MEDIAKEYSESSION_TYPE aSessionType,
      IMFContentDecryptionModuleSessionCallbacks* aCallbacks,
      SessionManagerWrapper* aManager);

  void Shutdown();

  // IMFContentDecryptionModuleSession
  STDMETHODIMP GenerateRequest(LPCWSTR init_data_type,

                               const BYTE* init_data,
                               DWORD init_data_size) override;
  STDMETHODIMP Load(LPCWSTR session_id, BOOL* loaded) override;
  STDMETHODIMP Update(const BYTE* aResponse, DWORD aResponseSize) override;
  STDMETHODIMP Close() override;
  STDMETHODIMP Remove() override;
  STDMETHODIMP GetSessionId(LPWSTR* aSessionId) override;
  STDMETHODIMP GetExpiration(double* aExpiration) override;
  STDMETHODIMP GetKeyStatuses(MFMediaKeyStatus** aKeyStatuses,
                              UINT* aKeyStatusesCount) override;

  void OnKeyMessage(MF_MEDIAKEYSESSION_MESSAGETYPE aMessageType,
                    const BYTE* aMessage, DWORD aMessageSize);
  void OnKeyStatusChanged(const cdm::KeyInformation* aKeysInfo,
                          uint32_t aKeysInfoCount);

 private:
  cdm::SessionType mSessionType;
  std::string mSessionId;
  Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSessionCallbacks> mCallbacks;
  RefPtr<SessionManagerWrapper> mSessionManager;

  struct KeyInformation {
    KeyInformation(const uint8_t* aKeyId, uint32_t aKeyIdSize,
                   cdm::KeyStatus aKeyStatus)
        : mKeyId(aKeyId, aKeyId + aKeyIdSize), mKeyStatus(aKeyStatus) {}
    std::vector<uint8_t> mKeyId;
    cdm::KeyStatus mKeyStatus;
  };
  std::vector<KeyInformation> mKeyInfo;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYSESSION_H
