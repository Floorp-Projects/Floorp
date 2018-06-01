/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>

#include "mozilla/RefPtr.h"
#include "nsIStringBundle.h"
#include "nsIUUIDGenerator.h"
#include "nsIXULAppInfo.h"

//#include "AudioSession.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/WindowsVersion.h"

#include <objbase.h>

namespace mozilla {
namespace widget {

/*
 * To take advantage of what Vista+ have to offer with respect to audio,
 * we need to maintain an audio session.  This class wraps IAudioSessionControl
 * and implements IAudioSessionEvents (for callbacks from Windows)
 */
class AudioSession final : public IAudioSessionEvents {
private:
  AudioSession();
  ~AudioSession();
public:
  static AudioSession* GetSingleton();

  // COM IUnknown
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP QueryInterface(REFIID, void**);
  STDMETHODIMP_(ULONG) Release();

  // IAudioSessionEvents
  STDMETHODIMP OnChannelVolumeChanged(DWORD aChannelCount,
                                      float aChannelVolumeArray[],
                                      DWORD aChangedChannel,
                                      LPCGUID aContext);
  STDMETHODIMP OnDisplayNameChanged(LPCWSTR aDisplayName, LPCGUID aContext);
  STDMETHODIMP OnGroupingParamChanged(LPCGUID aGroupingParam, LPCGUID aContext);
  STDMETHODIMP OnIconPathChanged(LPCWSTR aIconPath, LPCGUID aContext);
  STDMETHODIMP OnSessionDisconnected(AudioSessionDisconnectReason aReason);
private:
  nsresult OnSessionDisconnectedInternal();
  nsresult CommitAudioSessionData();

public:
  STDMETHODIMP OnSimpleVolumeChanged(float aVolume,
                                     BOOL aMute,
                                     LPCGUID aContext);
  STDMETHODIMP OnStateChanged(AudioSessionState aState);

  nsresult Start();
  nsresult Stop();
  void StopInternal();

  nsresult GetSessionData(nsID& aID,
                          nsString& aSessionName,
                          nsString& aIconPath);

  nsresult SetSessionData(const nsID& aID,
                          const nsString& aSessionName,
                          const nsString& aIconPath);

  enum SessionState {
    UNINITIALIZED, // Has not been initialized yet
    STARTED, // Started
    CLONED, // SetSessionInfoCalled, Start not called
    FAILED, // The audio session failed to start
    STOPPED, // Stop called
    AUDIO_SESSION_DISCONNECTED // Audio session disconnected
  };
protected:
  RefPtr<IAudioSessionControl> mAudioSessionControl;
  nsString mDisplayName;
  nsString mIconPath;
  nsID mSessionGroupingParameter;
  SessionState mState;
  // Guards the IAudioSessionControl
  mozilla::Mutex mMutex;

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  static AudioSession* sService;
};

nsresult
StartAudioSession()
{
  return AudioSession::GetSingleton()->Start();
}

nsresult
StopAudioSession()
{
  return AudioSession::GetSingleton()->Stop();
}

nsresult
GetAudioSessionData(nsID& aID,
                    nsString& aSessionName,
                    nsString& aIconPath)
{
  return AudioSession::GetSingleton()->GetSessionData(aID,
                                                      aSessionName,
                                                      aIconPath);
}

nsresult
RecvAudioSessionData(const nsID& aID,
                     const nsString& aSessionName,
                     const nsString& aIconPath)
{
  return AudioSession::GetSingleton()->SetSessionData(aID,
                                                      aSessionName,
                                                      aIconPath);
}

AudioSession* AudioSession::sService = nullptr;

AudioSession::AudioSession() :
  mMutex("AudioSessionControl")
{
  mState = UNINITIALIZED;
}

AudioSession::~AudioSession()
{

}

AudioSession*
AudioSession::GetSingleton()
{
  if (!(AudioSession::sService)) {
    RefPtr<AudioSession> service = new AudioSession();
    service.forget(&AudioSession::sService);
  }

  // We don't refcount AudioSession on the Gecko side, we hold one single ref
  // as long as the appshell is running.
  return AudioSession::sService;
}

// It appears Windows will use us on a background thread ...
NS_IMPL_ADDREF(AudioSession)
NS_IMPL_RELEASE(AudioSession)

STDMETHODIMP
AudioSession::QueryInterface(REFIID iid, void **ppv)
{
  const IID IID_IAudioSessionEvents = __uuidof(IAudioSessionEvents);
  if ((IID_IUnknown == iid) ||
      (IID_IAudioSessionEvents == iid)) {
    *ppv = static_cast<IAudioSessionEvents*>(this);
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// Once we are started Windows will hold a reference to us through our
// IAudioSessionEvents interface that will keep us alive until the appshell
// calls Stop.
nsresult
AudioSession::Start()
{
  MOZ_ASSERT(mState == UNINITIALIZED ||
             mState == CLONED ||
             mState == AUDIO_SESSION_DISCONNECTED,
             "State invariants violated");

  const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
  const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
  const IID IID_IAudioSessionManager = __uuidof(IAudioSessionManager);

  HRESULT hr;

  // There's a matching CoUninit in Stop() for this tied to a state of
  // UNINITIALIZED.
  hr = CoInitialize(nullptr);
  MOZ_ASSERT(SUCCEEDED(hr), "CoInitialize failure in audio session control, unexpected");

  if (mState == UNINITIALIZED) {
    mState = FAILED;

    // Content processes should be CLONED
    if (XRE_IsContentProcess()) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT(XRE_IsParentProcess(),
               "Should only get here in a chrome process!");

    nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    NS_ENSURE_TRUE(bundleService, NS_ERROR_FAILURE);
    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                                getter_AddRefs(bundle));
    NS_ENSURE_TRUE(bundle, NS_ERROR_FAILURE);

    bundle->GetStringFromName("brandFullName", mDisplayName);

    wchar_t *buffer;
    mIconPath.GetMutableData(&buffer, MAX_PATH);
    ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);

    nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1");
    NS_ENSURE_TRUE(uuidgen, NS_ERROR_FAILURE);
    uuidgen->GenerateUUIDInPlace(&mSessionGroupingParameter);
  }

  mState = FAILED;

  MOZ_ASSERT(!mDisplayName.IsEmpty() || !mIconPath.IsEmpty(),
             "Should never happen ...");

  RefPtr<IMMDeviceEnumerator> enumerator;
  hr = ::CoCreateInstance(CLSID_MMDeviceEnumerator,
                          nullptr,
                          CLSCTX_ALL,
                          IID_IMMDeviceEnumerator,
                          getter_AddRefs(enumerator));
  if (FAILED(hr))
    return NS_ERROR_NOT_AVAILABLE;

  RefPtr<IMMDevice> device;
  hr = enumerator->GetDefaultAudioEndpoint(EDataFlow::eRender,
                                           ERole::eMultimedia,
                                           getter_AddRefs(device));
  if (FAILED(hr)) {
    if (hr == E_NOTFOUND)
      return NS_ERROR_NOT_AVAILABLE;
    return NS_ERROR_FAILURE;
  }

  RefPtr<IAudioSessionManager> manager;
  hr = device->Activate(IID_IAudioSessionManager,
                        CLSCTX_ALL,
                        nullptr,
                        getter_AddRefs(manager));
  if (FAILED(hr)) {
    return NS_ERROR_FAILURE;
  }

  MutexAutoLock lock(mMutex);
  hr = manager->GetAudioSessionControl(&GUID_NULL,
                                       0,
                                       getter_AddRefs(mAudioSessionControl));

  if (FAILED(hr)) {
    return NS_ERROR_FAILURE;
  }

  // Increments refcount of 'this'.
  hr = mAudioSessionControl->RegisterAudioSessionNotification(this);
  if (FAILED(hr)) {
    StopInternal();
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod("AudioSession::CommitAudioSessionData",
                      this, &AudioSession::CommitAudioSessionData);
  NS_DispatchToMainThread(runnable);

  mState = STARTED;

  return NS_OK;
}

void
SpawnASCReleaseThread(RefPtr<IAudioSessionControl>&& aASC)
{
  // Fake moving to the other thread by circumventing the ref count.
  // (RefPtrs don't play well with C++11 lambdas and we don't want to use
  // XPCOM here.)
  IAudioSessionControl* rawPtr = nullptr;
  aASC.forget(&rawPtr);
  MOZ_ASSERT(rawPtr);
  PRThread* thread =
    PR_CreateThread(PR_USER_THREAD,
                    [](void* aRawPtr) { static_cast<IAudioSessionControl*>(aRawPtr)->Release(); },
                    rawPtr, PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_UNJOINABLE_THREAD, 0);
  if (!thread) {
    // We can't make a thread so just destroy the IAudioSessionControl here.
    rawPtr->Release();
  }
}


void
AudioSession::StopInternal()
{
  mMutex.AssertCurrentThreadOwns();

  if (mAudioSessionControl &&
      (mState == STARTED || mState == STOPPED)) {
    // Decrement refcount of 'this'
    mAudioSessionControl->UnregisterAudioSessionNotification(this);
  }

  // Win7 is the only Windows version supported before Win8.
  if (mAudioSessionControl && !IsWin8OrLater()) {
    // bug 1419488: Avoid hanging due to Win7 race condition when destroying
    // AudioSessionControl.  We do that by Moving the AudioSessionControl
    // to a worker thread (that we never 'join') for destruction.
    SpawnASCReleaseThread(std::move(mAudioSessionControl));
  } else {
    mAudioSessionControl = nullptr;
  }
}

nsresult
AudioSession::Stop()
{
  MOZ_ASSERT(mState == STARTED ||
             mState == UNINITIALIZED || // XXXremove this
             mState == FAILED,
             "State invariants violated");
  SessionState state = mState;
  mState = STOPPED;

  {
    RefPtr<AudioSession> kungFuDeathGrip;
    kungFuDeathGrip.swap(sService);

    MutexAutoLock lock(mMutex);
    StopInternal();
  }

  if (state != UNINITIALIZED) {
    ::CoUninitialize();
  }
  return NS_OK;
}

void CopynsID(nsID& lhs, const nsID& rhs)
{
  lhs.m0 = rhs.m0;
  lhs.m1 = rhs.m1;
  lhs.m2 = rhs.m2;
  for (int i = 0; i < 8; i++ ) {
    lhs.m3[i] = rhs.m3[i];
  }
}

nsresult
AudioSession::GetSessionData(nsID& aID,
                             nsString& aSessionName,
                             nsString& aIconPath)
{
  MOZ_ASSERT(mState == FAILED ||
             mState == STARTED ||
             mState == CLONED,
             "State invariants violated");

  CopynsID(aID, mSessionGroupingParameter);
  aSessionName = mDisplayName;
  aIconPath = mIconPath;

  if (mState == FAILED)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
AudioSession::SetSessionData(const nsID& aID,
                             const nsString& aSessionName,
                             const nsString& aIconPath)
{
  MOZ_ASSERT(mState == UNINITIALIZED,
             "State invariants violated");
  MOZ_ASSERT(!XRE_IsParentProcess(),
             "Should never get here in a chrome process!");
  mState = CLONED;

  CopynsID(mSessionGroupingParameter, aID);
  mDisplayName = aSessionName;
  mIconPath = aIconPath;
  return NS_OK;
}

nsresult
AudioSession::CommitAudioSessionData()
{
  MutexAutoLock lock(mMutex);

  if (!mAudioSessionControl) {
    // Stop() was called before we had a chance to do this.
    return NS_OK;
  }

  HRESULT hr =
    mAudioSessionControl->SetGroupingParam((LPGUID)&mSessionGroupingParameter,
                                           nullptr);
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
                                     DWORD aChangedChannel,
                                     LPCGUID aContext)
{
  return S_OK; // NOOP
}

STDMETHODIMP
AudioSession::OnDisplayNameChanged(LPCWSTR aDisplayName,
                                   LPCGUID aContext)
{
  return S_OK; // NOOP
}

STDMETHODIMP
AudioSession::OnGroupingParamChanged(LPCGUID aGroupingParam,
                                     LPCGUID aContext)
{
  return S_OK; // NOOP
}

STDMETHODIMP
AudioSession::OnIconPathChanged(LPCWSTR aIconPath,
                                LPCGUID aContext)
{
  return S_OK; // NOOP
}

STDMETHODIMP
AudioSession::OnSessionDisconnected(AudioSessionDisconnectReason aReason)
{
  // Run our code asynchronously.  Per MSDN we can't do anything interesting
  // in this callback.
  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod("widget::AudioSession::OnSessionDisconnectedInternal",
                      this, &AudioSession::OnSessionDisconnectedInternal);
  NS_DispatchToMainThread(runnable);
  return S_OK;
}

nsresult
AudioSession::OnSessionDisconnectedInternal()
{
  // When successful, UnregisterAudioSessionNotification will decrement the
  // refcount of 'this'.  Start will re-increment it.  In the interim,
  // we'll need to reference ourselves.
  RefPtr<AudioSession> kungFuDeathGrip(this);

  {
    // We need to release the mutex before we call Start().
    MutexAutoLock lock(mMutex);

    if (!mAudioSessionControl)
      return NS_OK;

    mAudioSessionControl->UnregisterAudioSessionNotification(this);
    mAudioSessionControl = nullptr;
  }

  mState = AUDIO_SESSION_DISCONNECTED;
  CoUninitialize();
  Start(); // If it fails there's not much we can do.
  return NS_OK;
}

STDMETHODIMP
AudioSession::OnSimpleVolumeChanged(float aVolume,
                                    BOOL aMute,
                                    LPCGUID aContext)
{
  return S_OK; // NOOP
}

STDMETHODIMP
AudioSession::OnStateChanged(AudioSessionState aState)
{
  return S_OK; // NOOP
}

} // namespace widget
} // namespace mozilla
