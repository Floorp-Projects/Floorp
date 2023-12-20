/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <atomic>
#include <audiopolicy.h>
#include <windows.h>
#include <mmdeviceapi.h>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPtr.h"
#include "nsIStringBundle.h"

#include "nsCOMPtr.h"
#include "nsID.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/mscom/AgileReference.h"
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
  AudioSession();

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
  void Stop(bool shouldRestart = false);

  nsresult GetSessionData(nsID& aID, nsString& aSessionName,
                          nsString& aIconPath);
  nsresult SetSessionData(const nsID& aID, const nsString& aSessionName,
                          const nsString& aIconPath);

 private:
  ~AudioSession() = default;

  void StopInternal(const MutexAutoLock& aProofOfLock,
                    bool shouldRestart = false);

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

StaticRefPtr<AudioSession> sService;

void StartAudioSession() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sService);
  sService = new AudioSession();

  // Destroy AudioSession only after any background task threads have been
  // stopped or abandoned.
  ClearOnShutdown(&sService, ShutdownPhase::XPCOMShutdownFinal);

  NS_DispatchBackgroundTask(
      NS_NewCancelableRunnableFunction("StartAudioSession", []() -> void {
        MOZ_ASSERT(AudioSession::GetSingleton(),
                   "AudioSession should outlive background threads");
        AudioSession::GetSingleton()->Start();
      }));
}

void StopAudioSession() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sService);
  NS_DispatchBackgroundTask(
      NS_NewRunnableFunction("StopAudioSession", []() -> void {
        MOZ_ASSERT(AudioSession::GetSingleton(),
                   "AudioSession should outlive background threads");
        AudioSession::GetSingleton()->Stop();
      }));
}

AudioSession* AudioSession::GetSingleton() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
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

AudioSession::AudioSession() : mMutex("AudioSessionControl") {
  // This func must be run on the main thread as
  // nsStringBundle is not thread safe otherwise
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(XRE_IsParentProcess(),
             "Should only get here in a chrome process!");

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

  const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
  const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
  const IID IID_IAudioSessionManager = __uuidof(IAudioSessionManager);

  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(!mAudioSessionControl);
  MOZ_ASSERT(!mDisplayName.IsEmpty() || !mIconPath.IsEmpty(),
             "Should never happen ...");

  auto scopeExit = MakeScopeExit([&] { StopInternal(lock); });

  RefPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr =
      ::CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                         IID_IMMDeviceEnumerator, getter_AddRefs(enumerator));
  if (FAILED(hr)) {
    return;
  }

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

  hr = manager->GetAudioSessionControl(&GUID_NULL, 0,
                                       getter_AddRefs(mAudioSessionControl));

  if (FAILED(hr) || !mAudioSessionControl) {
    return;
  }

  // Increments refcount of 'this'.
  hr = mAudioSessionControl->RegisterAudioSessionNotification(this);
  if (FAILED(hr)) {
    return;
  }

  hr = mAudioSessionControl->SetGroupingParam(
      (LPGUID) & (mSessionGroupingParameter), nullptr);
  if (FAILED(hr)) {
    return;
  }

  hr = mAudioSessionControl->SetDisplayName(mDisplayName.get(), nullptr);
  if (FAILED(hr)) {
    return;
  }

  hr = mAudioSessionControl->SetIconPath(mIconPath.get(), nullptr);
  if (FAILED(hr)) {
    return;
  }

  scopeExit.release();
}

void AudioSession::Stop(bool shouldRestart) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  MutexAutoLock lock(mMutex);
  StopInternal(lock, shouldRestart);
}

void AudioSession::StopInternal(const MutexAutoLock& aProofOfLock,
                                bool shouldRestart) {
  if (!mAudioSessionControl) {
    return;
  }

  // Decrement refcount of 'this'
  mAudioSessionControl->UnregisterAudioSessionNotification(this);

  // Deleting the IAudioSessionControl COM object requires the STA/main thread.
  // Audio code may concurrently be running on the main thread and it may
  // block waiting for this to complete, creating deadlock.  So we destroy the
  // IAudioSessionControl on the main thread instead.  In order to do that, we
  // need to marshall the object to the main thread's apartment with an
  // AgileReference.
  const IID IID_IAudioSessionControl = __uuidof(IAudioSessionControl);
  auto agileAsc = MakeUnique<mozilla::mscom::AgileReference>(
      IID_IAudioSessionControl, mAudioSessionControl);
  mAudioSessionControl = nullptr;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "FreeAudioSession", [agileAsc = std::move(agileAsc),
                           IID_IAudioSessionControl, shouldRestart] {
        RefPtr<IAudioSessionControl> toDelete;
        [[maybe_unused]] HRESULT hr = agileAsc->Resolve(
            IID_IAudioSessionControl, getter_AddRefs(toDelete));
        MOZ_ASSERT(SUCCEEDED(hr));
        // Now release the AgileReference which holds our only reference to the
        // IAudioSessionControl, then maybe restart.
        if (shouldRestart) {
          NS_DispatchBackgroundTask(
              NS_NewCancelableRunnableFunction("RestartAudioSession", [] {
                AudioSession* as = AudioSession::GetSingleton();
                MOZ_ASSERT(as);
                as->Start();
              }));
        }
      }));
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
  CopynsID(aID, mSessionGroupingParameter);
  aSessionName = mDisplayName;
  aIconPath = mIconPath;

  return NS_OK;
}

nsresult AudioSession::SetSessionData(const nsID& aID,
                                      const nsString& aSessionName,
                                      const nsString& aIconPath) {
  MOZ_ASSERT(!XRE_IsParentProcess(),
             "Should never get here in a chrome process!");
  CopynsID(mSessionGroupingParameter, aID);
  mDisplayName = aSessionName;
  mIconPath = aIconPath;
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
  Stop(true /* shouldRestart */);
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
