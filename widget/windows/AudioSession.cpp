/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <atomic>
#include <audiopolicy.h>
#include <windows.h>
#include <mmdeviceapi.h>

#include "mozilla/RefPtr.h"
#include "nsIStringBundle.h"

#include "nsCOMPtr.h"
#include "nsID.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/Mutex.h"
#include "mozilla/WindowsVersion.h"

namespace mozilla {
namespace widget {

/*
 * To take advantage of what Vista+ have to offer with respect to audio,
 * we need to maintain an audio session.  This class wraps IAudioSessionControl
 * and implements IAudioSessionEvents (for callbacks from Windows)
 */
class AudioSession final : public IAudioSessionEvents {
 public:
  static AudioSession* GetSingleton();

  // COM IUnknown
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP QueryInterface(REFIID, void**);
  STDMETHODIMP_(ULONG) Release();

  // IAudioSessionEvents
  STDMETHODIMP OnChannelVolumeChanged(DWORD aChannelCount,
                                      float aChannelVolumeArray[],
                                      DWORD aChangedChannel, LPCGUID aContext);
  STDMETHODIMP OnDisplayNameChanged(LPCWSTR aDisplayName, LPCGUID aContext);
  STDMETHODIMP OnGroupingParamChanged(LPCGUID aGroupingParam, LPCGUID aContext);
  STDMETHODIMP OnIconPathChanged(LPCWSTR aIconPath, LPCGUID aContext);
  STDMETHODIMP OnSessionDisconnected(AudioSessionDisconnectReason aReason);
  STDMETHODIMP OnSimpleVolumeChanged(float aVolume, BOOL aMute,
                                     LPCGUID aContext);
  STDMETHODIMP OnStateChanged(AudioSessionState aState);

  void Start();
  void Stop();
  void StopInternal();
  void InitializeAudioSession();

  nsresult GetSessionData(nsID& aID, nsString& aSessionName,
                          nsString& aIconPath);
  nsresult SetSessionData(const nsID& aID, const nsString& aSessionName,
                          const nsString& aIconPath);

  enum SessionState {
    UNINITIALIZED,              // Has not been initialized yet
    STARTED,                    // Started
    CLONED,                     // SetSessionInfoCalled, Start not called
    FAILED,                     // The audio session failed to start
    STOPPED,                    // Stop called
    AUDIO_SESSION_DISCONNECTED  // Audio session disconnected
  };

  SessionState mState;

 private:
  AudioSession();
  ~AudioSession();
  nsresult CommitAudioSessionData();

 protected:
  RefPtr<IAudioSessionControl> mAudioSessionControl;
  nsString mDisplayName;
  nsString mIconPath;
  nsID mSessionGroupingParameter;
  // Guards the IAudioSessionControl
  mozilla::Mutex mMutex MOZ_UNANNOTATED;

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

static std::atomic<AudioSession*> sService = nullptr;

void StartAudioSession() {
  AudioSession::GetSingleton()->InitializeAudioSession();
  NS_DispatchBackgroundTask(NS_NewRunnableFunction(
      "StartAudioSession",
      []() -> void { AudioSession::GetSingleton()->Start(); }));
}

void StopAudioSession() {
  RefPtr<AudioSession> audioSession;
  AudioSession* temp = sService;
  audioSession.swap(temp);
  sService = nullptr;

  if (audioSession) {
    NS_DispatchBackgroundTask(NS_NewRunnableFunction(
        "StopAudioSession",
        [audioSession]() -> void { audioSession->Stop(); }));
  }
}

AudioSession::AudioSession() : mMutex("AudioSessionControl") {
  mState = UNINITIALIZED;
}

AudioSession::~AudioSession() {}

AudioSession* AudioSession::GetSingleton() {
  if (!sService) {
    RefPtr<AudioSession> service = new AudioSession();
    AudioSession* temp = nullptr;
    service.swap(temp);
    sService = temp;
  }

  // We don't refcount AudioSession on the Gecko side, we hold one single ref
  // as long as the appshell is running.
  return sService;
}

// It appears Windows will use us on a background thread ...
NS_IMPL_ADDREF(AudioSession)
NS_IMPL_RELEASE(AudioSession)

STDMETHODIMP
AudioSession::QueryInterface(REFIID iid, void** ppv) {
  const IID IID_IAudioSessionEvents = __uuidof(IAudioSessionEvents);
  if ((IID_IUnknown == iid) || (IID_IAudioSessionEvents == iid)) {
    *ppv = static_cast<IAudioSessionEvents*>(this);
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

void AudioSession::InitializeAudioSession() {
  // This func must be run on the main thread as
  // nsStringBundle is not thread safe otherwise
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(XRE_IsParentProcess(),
             "Should only get here in a chrome process!");

  if (mState != UNINITIALIZED) return;

  nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  MOZ_ASSERT(bundleService);

  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                              getter_AddRefs(bundle));
  MOZ_ASSERT(bundle);
  bundle->GetStringFromName("brandFullName", mDisplayName);

  wchar_t* buffer;
  mIconPath.GetMutableData(&buffer, MAX_PATH);
  ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);

  [[maybe_unused]] nsresult rv =
      nsID::GenerateUUIDInPlace(mSessionGroupingParameter);
  MOZ_ASSERT(rv == NS_OK);
}

// Once we are started Windows will hold a reference to us through our
// IAudioSessionEvents interface that will keep us alive until the appshell
// calls Stop.
void AudioSession::Start() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mState == UNINITIALIZED || mState == CLONED ||
                 mState == AUDIO_SESSION_DISCONNECTED,
             "State invariants violated");

  const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
  const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
  const IID IID_IAudioSessionManager = __uuidof(IAudioSessionManager);

  HRESULT hr;

  mState = FAILED;

  MOZ_ASSERT(!mDisplayName.IsEmpty() || !mIconPath.IsEmpty(),
             "Should never happen ...");

  RefPtr<IMMDeviceEnumerator> enumerator;
  hr = ::CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                          IID_IMMDeviceEnumerator, getter_AddRefs(enumerator));
  if (FAILED(hr)) return;

  RefPtr<IMMDevice> device;
  hr = enumerator->GetDefaultAudioEndpoint(
      EDataFlow::eRender, ERole::eMultimedia, getter_AddRefs(device));
  if (FAILED(hr)) {
    return;
  }

  RefPtr<IAudioSessionManager> manager;
  hr = device->Activate(IID_IAudioSessionManager, CLSCTX_ALL, nullptr,
                        getter_AddRefs(manager));
  if (FAILED(hr)) {
    return;
  }

  MutexAutoLock lock(mMutex);
  hr = manager->GetAudioSessionControl(&GUID_NULL, 0,
                                       getter_AddRefs(mAudioSessionControl));

  if (FAILED(hr)) {
    return;
  }

  // Increments refcount of 'this'.
  hr = mAudioSessionControl->RegisterAudioSessionNotification(this);
  if (FAILED(hr)) {
    StopInternal();
    return;
  }

  CommitAudioSessionData();
  mState = STARTED;
}

void AudioSession::StopInternal() {
  mMutex.AssertCurrentThreadOwns();

  if (mAudioSessionControl && (mState == STARTED || mState == STOPPED)) {
    // Decrement refcount of 'this'
    mAudioSessionControl->UnregisterAudioSessionNotification(this);
    // Deleting this COM object seems to require the STA / main thread.
    // Audio code may concurrently be running on the main thread and it may
    // block waiting for this to complete, creating deadlock.  So we destroy the
    // object on the main thread instead.
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "ShutdownAudioSession",
        [asc = std::move(mAudioSessionControl)] { /* */ }));
  }
}

void AudioSession::Stop() {
  MOZ_ASSERT(mState == STARTED || mState == UNINITIALIZED ||  // XXXremove this
                 mState == FAILED,
             "State invariants violated");
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  MutexAutoLock lock(mMutex);
  mState = STOPPED;
  StopInternal();
}

void CopynsID(nsID& lhs, const nsID& rhs) {
  lhs.m0 = rhs.m0;
  lhs.m1 = rhs.m1;
  lhs.m2 = rhs.m2;
  for (int i = 0; i < 8; i++) {
    lhs.m3[i] = rhs.m3[i];
  }
}

nsresult AudioSession::GetSessionData(nsID& aID, nsString& aSessionName,
                                      nsString& aIconPath) {
  MOZ_ASSERT(mState == FAILED || mState == STARTED || mState == CLONED,
             "State invariants violated");

  CopynsID(aID, mSessionGroupingParameter);
  aSessionName = mDisplayName;
  aIconPath = mIconPath;

  if (mState == FAILED) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult AudioSession::SetSessionData(const nsID& aID,
                                      const nsString& aSessionName,
                                      const nsString& aIconPath) {
  MOZ_ASSERT(mState == UNINITIALIZED, "State invariants violated");
  MOZ_ASSERT(!XRE_IsParentProcess(),
             "Should never get here in a chrome process!");
  mState = CLONED;

  CopynsID(mSessionGroupingParameter, aID);
  mDisplayName = aSessionName;
  mIconPath = aIconPath;
  return NS_OK;
}

nsresult AudioSession::CommitAudioSessionData() {
  mMutex.AssertCurrentThreadOwns();

  if (!mAudioSessionControl) {
    // Stop() was called before we had a chance to do this.
    return NS_OK;
  }

  HRESULT hr = mAudioSessionControl->SetGroupingParam(
      (LPGUID) & (mSessionGroupingParameter), nullptr);
  if (FAILED(hr)) {
    StopInternal();
    return NS_ERROR_FAILURE;
  }

  hr = mAudioSessionControl->SetDisplayName(mDisplayName.get(), nullptr);
  if (FAILED(hr)) {
    StopInternal();
    return NS_ERROR_FAILURE;
  }

  hr = mAudioSessionControl->SetIconPath(mIconPath.get(), nullptr);
  if (FAILED(hr)) {
    StopInternal();
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

STDMETHODIMP
AudioSession::OnChannelVolumeChanged(DWORD aChannelCount,
                                     float aChannelVolumeArray[],
                                     DWORD aChangedChannel, LPCGUID aContext) {
  return S_OK;  // NOOP
}

STDMETHODIMP
AudioSession::OnDisplayNameChanged(LPCWSTR aDisplayName, LPCGUID aContext) {
  return S_OK;  // NOOP
}

STDMETHODIMP
AudioSession::OnGroupingParamChanged(LPCGUID aGroupingParam, LPCGUID aContext) {
  return S_OK;  // NOOP
}

STDMETHODIMP
AudioSession::OnIconPathChanged(LPCWSTR aIconPath, LPCGUID aContext) {
  return S_OK;  // NOOP
}

STDMETHODIMP
AudioSession::OnSessionDisconnected(AudioSessionDisconnectReason aReason) {
  {
    MutexAutoLock lock(mMutex);
    if (!mAudioSessionControl) return S_OK;
    mAudioSessionControl->UnregisterAudioSessionNotification(this);
    // Deleting this COM object seems to require the STA / main thread.
    // Audio code may concurrently be running on the main thread and it may
    // block waiting for this to complete, creating deadlock.  So we destroy the
    // object on the main thread instead.
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "FreeAudioSession", [asc = std::move(mAudioSessionControl)] { /* */ }));
    mState = AUDIO_SESSION_DISCONNECTED;
  }
  Start();  // If it fails there's not much we can do.
  return S_OK;
}

STDMETHODIMP
AudioSession::OnSimpleVolumeChanged(float aVolume, BOOL aMute,
                                    LPCGUID aContext) {
  return S_OK;  // NOOP
}

STDMETHODIMP
AudioSession::OnStateChanged(AudioSessionState aState) {
  return S_OK;  // NOOP
}

}  // namespace widget
}  // namespace mozilla
