/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDM_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDM_H

#include <mfidl.h>
#include <unordered_map>
#include <variant>
#include <windows.h>
#include <windows.media.protection.h>
#include <wrl.h>
#include <wrl/client.h>

#include "ClearKeySessionManager.h"
#include "MFCDMExtra.h"
#include "WMFClearKeyUtils.h"
#include "content_decryption_module.h"

namespace mozilla {

class SessionManagerWrapper;
class WMFClearKeySession;

// This our customized MFCDM for supporting clearkey in our testing. It would
// use ClearKeySessionManager via SessionManagerWrapper to perform decryption.
class WMFClearKeyCDM final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModule, IMFGetService, IMFShutdown,
          Microsoft::WRL::FtmBase> {
 public:
  WMFClearKeyCDM() = default;
  ~WMFClearKeyCDM();
  WMFClearKeyCDM(const WMFClearKeyCDM&) = delete;
  WMFClearKeyCDM& operator=(const WMFClearKeyCDM&) = delete;

  HRESULT RuntimeClassInitialize(IPropertyStore* aProperties);

  // IMFContentDecryptionModule
  STDMETHODIMP SetContentEnabler(IMFContentEnabler* aContentEnabler,
                                 IMFAsyncResult* aResult) override;
  STDMETHODIMP GetSuspendNotify(IMFCdmSuspendNotify** aNotify) override;
  STDMETHODIMP SetPMPHostApp(IMFPMPHostApp* aPmpHostApp) override;
  STDMETHODIMP CreateSession(
      MF_MEDIAKEYSESSION_TYPE aSessionType,
      IMFContentDecryptionModuleSessionCallbacks* aCallbacks,
      IMFContentDecryptionModuleSession** aSession) override;
  STDMETHODIMP SetServerCertificate(const BYTE* aCertificate,
                                    DWORD aCertificateSize) override;
  STDMETHODIMP CreateTrustedInput(const BYTE* aContentInitData,
                                  DWORD aContentInitDataSize,
                                  IMFTrustedInput** aTrustedInput) override;
  STDMETHODIMP GetProtectionSystemIds(GUID** aSystemIds,
                                      DWORD* aCount) override;
  // IMFGetService
  STDMETHODIMP GetService(REFGUID aGuidService, REFIID aRiid,
                          LPVOID* aPpvObject) override;

  // IMFShutdown
  STDMETHODIMP Shutdown() override;
  STDMETHODIMP GetShutdownStatus(MFSHUTDOWN_STATUS* aStatus) override;

 private:
  RefPtr<SessionManagerWrapper> mSessionManager;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Media::Protection::IMediaProtectionPMPServer>
      mPMPServer;
};

// In order to reuse existing Gecko clearkey implementation, we need to
// inherit the class `cdm::Host_10`.
// TODO : add a way to assert thread usage. It would be used on MF thread pool
// and the media supervisor thread pool.
class SessionManagerWrapper final : public RefCounted, private cdm::Host_10 {
 public:
  explicit SessionManagerWrapper(WMFClearKeyCDM* aCDM);

  HRESULT GenerateRequest(cdm::InitDataType aInitDataType,
                          const BYTE* aInitData, DWORD aInitDataSize,
                          cdm::SessionType aSessionType,
                          WMFClearKeySession* aSession,
                          std::string& aSessionIdOut);
  HRESULT UpdateSession(const std::string& aSessionId, const BYTE* aResponse,
                        DWORD aResponseSize);
  HRESULT CloseSession(const std::string& aSessionId);
  HRESULT RemoveSession(const std::string& aSessionId);
  HRESULT Decrypt(const cdm::InputBuffer_2& aBuffer,
                  cdm::DecryptedBlock* aDecryptedBlock);

  void Shutdown();
  bool IsShutdown();

 private:
  ~SessionManagerWrapper();
  // cdm::Host_10
  void OnInitialized(bool aSuccess) override {}
  void OnResolveKeyStatusPromise(uint32_t aPromiseId,
                                 cdm::KeyStatus aKeyStatus) override {}
  void OnResolveNewSessionPromise(uint32_t aPromiseId, const char* aSessionId,
                                  uint32_t aSessionIdSize) override;
  void OnResolvePromise(uint32_t aPromiseId) override;
  void OnRejectPromise(uint32_t aPromiseId, cdm::Exception aException,
                       uint32_t aSystemCode, const char* aErrorMessage,
                       uint32_t aErrorMessageSize) override;
  void OnSessionMessage(const char* aSessionId, uint32_t aSessionIdSize,
                        cdm::MessageType aMessageType, const char* aMessage,
                        uint32_t aMessageSize) override;
  void OnSessionKeysChange(const char* aSessionId, uint32_t aSessionIdSize,
                           bool aHasAdditionalUsableKey,
                           const cdm::KeyInformation* aKeysInfo,
                           uint32_t aKeysInfoCount) override;
  void OnExpirationChange(const char* aSessionId, uint32_t aSessionIdSize,
                          cdm::Time aNewExpiryTime) override{
      // No need to implement this because the session would never expire in
      // testing.
  };
  void OnSessionClosed(const char* aSessionId,
                       uint32_t aSessionIdSize) override{
      // No need to implement this because session doesn't have close callback
      // or events.
  };
  cdm::FileIO* CreateFileIO(cdm::FileIOClient* aClient) override {
    // We don't support this because we only support temporary session.
    return nullptr;
  }
  void SendPlatformChallenge(const char* aServiceId, uint32_t aServiceIdSize,
                             const char* aChallenge,
                             uint32_t aChallengeSize) override {}
  void EnableOutputProtection(uint32_t aDesiredProtectionMask) override {}
  void QueryOutputProtectionStatus() override{};
  void OnDeferredInitializationDone(cdm::StreamType aStreamType,
                                    cdm::Status aDecoderStatus) override {}
  void RequestStorageId(uint32_t aVersion) override {}
  cdm::Buffer* Allocate(uint32_t aCapacity) override;
  void SetTimer(int64_t aDelayMs, void* aContext) override {}
  cdm::Time GetCurrentWallTime() override { return 0.0; }
  friend class SessionManager;

  Microsoft::WRL::ComPtr<WMFClearKeyCDM> mOwnerCDM;
  RefPtr<ClearKeySessionManager> mSessionManager;
  std::unordered_map<std::string, Microsoft::WRL::ComPtr<WMFClearKeySession>>
      mSessions;

  // This is a RAII helper class to use ClearKeySessionManager::XXXSession
  // methods in a sync style, which is what MFCDM is required.
  // ClearKeySessionManager uses cdm::Host_10's OnResolve/RejectXXX as callback
  // to report whether those function calls relatd with specific promise id
  // succeed or not. As we only do temporary session for ClearKey testing, we
  // don't need to wait to setup the storage so calling those XXXsession
  // functions are actully a sync process. We guarantee that
  // ClearKeySessionManager will use OnResolve/Reject methods to notify us
  // result, right after we calling the session related method.
  // [How to to use this class, not thread-safe]
  //   1. create it on the stack
  //   2. use GetPromiseId() to generate a fake promise id for tracking
  //   3. in cdm::Host_10's callback function, check promise id to know what
  //      result needs to be set
  //   4. check result to see if the session method succeed or not
  class SyncResultChecker final {
   public:
    using ResultType = std::variant<const char*, bool>;
    explicit SyncResultChecker(SessionManagerWrapper& aOwner)
        : mOwner(aOwner), mIdx(sIdx++), mKeySession(nullptr) {
      mOwner.mActiveSyncResultChecker.insert({mIdx, this});
    }
    SyncResultChecker(SessionManagerWrapper& aOwner,
                      WMFClearKeySession* aKeySession)
        : mOwner(aOwner), mIdx(sIdx++), mKeySession(aKeySession) {
      mOwner.mActiveSyncResultChecker.insert({mIdx, this});
    }
    ~SyncResultChecker() { mOwner.mActiveSyncResultChecker.erase(mIdx); }
    uint32_t GetPromiseId() const { return mIdx; }
    const ResultType& GetResult() const { return mResult; }
    WMFClearKeySession* GetKeySession() const { return mKeySession; }

   private:
    // Only allow setting result from these callbacks.
    friend void SessionManagerWrapper::OnResolveNewSessionPromise(uint32_t,
                                                                  const char*,
                                                                  uint32_t);
    friend void SessionManagerWrapper::OnResolvePromise(uint32_t);
    friend void SessionManagerWrapper::OnRejectPromise(uint32_t, cdm::Exception,
                                                       uint32_t, const char*,
                                                       uint32_t);
    void SetResultConstChar(const char* aResult) {
      mResult.emplace<const char*>(aResult);
    }
    void SetResultBool(bool aResult) { mResult.emplace<bool>(aResult); }

    static inline uint32_t sIdx = 0;
    SessionManagerWrapper& mOwner;
    const uint32_t mIdx;
    ResultType mResult;
    WMFClearKeySession* const mKeySession;
  };
  std::unordered_map<uint32_t, SyncResultChecker*> mActiveSyncResultChecker;

  // Protect following members.
  std::mutex mMutex;
  bool mIsShutdown = false;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDM_H
