/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>

#include "nsIStringBundle.h"
#include "nsIUUIDGenerator.h"
#include "nsIXULAppInfo.h"

//#include "AudioSession.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"

#include <objbase.h>

namespace mozilla {
namespace widget {

/* 
 * To take advantage of what Vista+ have to offer with respect to audio,
 * we need to maintain an audio session.  This class wraps IAudioSessionControl
 * and implements IAudioSessionEvents (for callbacks from Windows)
 */
class AudioSession MOZ_FINAL : public IAudioSessionEvents {
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
    FAILED, // The autdio session failed to start
    STOPPED, // Stop called
    AUDIO_SESSION_DISCONNECTED // Audio session disconnected
  };
protected:
  nsRefPtr<IAudioSessionControl> mAudioSessionControl;
  nsString mDisplayName;
  nsString mIconPath;
  nsID mSessionGroupingParameter;
  SessionState mState;

  nsAutoRefCnt mRefCnt;
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

AudioSession* AudioSession::sService = NULL;

AudioSession::AudioSession()
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
    nsRefPtr<AudioSession> service = new AudioSession();
    service.forget(&AudioSession::sService);
  }

  // We don't refcount AudioSession on the Gecko side, we hold one single ref
  // as long as the appshell is running.
  return AudioSession::sService;
}

// It appears Windows will use us on a background thread ...
NS_IMPL_THREADSAFE_ADDREF(AudioSession)
NS_IMPL_THREADSAFE_RELEASE(AudioSession)

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
  NS_ABORT_IF_FALSE(mState == UNINITIALIZED || 
                    mState == CLONED ||
                    mState == AUDIO_SESSION_DISCONNECTED,
                    "State invariants violated");

  const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
  const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
  const IID IID_IAudioSessionManager = __uuidof(IAudioSessionManager);

  HRESULT hr;

  if (FAILED(::CoInitialize(NULL)))
    return NS_ERROR_FAILURE;

  if (mState == UNINITIALIZED) {
    mState = FAILED;

    // XXXkhuey implement this for content processes
    if (XRE_GetProcessType() == GeckoProcessType_Content)
      return NS_ERROR_FAILURE;

    NS_ABORT_IF_FALSE(XRE_GetProcessType() == GeckoProcessType_Default,
                      "Should only get here in a chrome process!");

    nsCOMPtr<nsIStringBundleService> bundleService = 
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    NS_ENSURE_TRUE(bundleService, NS_ERROR_FAILURE);

    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                                getter_AddRefs(bundle));
    NS_ENSURE_TRUE(bundle, NS_ERROR_FAILURE);

    bundle->GetStringFromName(NS_LITERAL_STRING("brandFullName").get(),
                              getter_Copies(mDisplayName));

    PRUnichar *buffer;
    mIconPath.GetMutableData(&buffer, MAX_PATH);

    // XXXkhuey we should provide a way for a xulrunner app to specify an icon
    // that's not in the product binary.
    ::GetModuleFileNameW(NULL, buffer, MAX_PATH);

    nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1");
    NS_ASSERTION(uuidgen, "No UUID-Generator?!?");

    uuidgen->GenerateUUIDInPlace(&mSessionGroupingParameter);
  }

  mState = FAILED;

  NS_ABORT_IF_FALSE(!mDisplayName.IsEmpty() || !mIconPath.IsEmpty(),
                    "Should never happen ...");

  nsRefPtr<IMMDeviceEnumerator> enumerator;
  hr = ::CoCreateInstance(CLSID_MMDeviceEnumerator,
                          NULL,
                          CLSCTX_ALL,
                          IID_IMMDeviceEnumerator,
                          getter_AddRefs(enumerator));
  if (FAILED(hr))
    return NS_ERROR_NOT_AVAILABLE;

  nsRefPtr<IMMDevice> device;
  hr = enumerator->GetDefaultAudioEndpoint(EDataFlow::eRender,
                                           ERole::eMultimedia,
                                           getter_AddRefs(device));
  if (FAILED(hr)) {
    if (hr == E_NOTFOUND)
      return NS_ERROR_NOT_AVAILABLE;
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<IAudioSessionManager> manager;
  hr = device->Activate(IID_IAudioSessionManager,
                        CLSCTX_ALL,
                        NULL,
                        getter_AddRefs(manager));
  if (FAILED(hr))
    return NS_ERROR_FAILURE;

  hr = manager->GetAudioSessionControl(NULL,
                                       FALSE,
                                       getter_AddRefs(mAudioSessionControl));
  if (FAILED(hr))
    return NS_ERROR_FAILURE;

  hr = mAudioSessionControl->SetGroupingParam((LPCGUID)&mSessionGroupingParameter,
                                              NULL);
  if (FAILED(hr)) {
    StopInternal();
    return NS_ERROR_FAILURE;
  }

  hr = mAudioSessionControl->SetDisplayName(mDisplayName.get(), NULL);
  if (FAILED(hr)) {
    StopInternal();
    return NS_ERROR_FAILURE;
  }

  hr = mAudioSessionControl->SetIconPath(mIconPath.get(), NULL);
  if (FAILED(hr)) {
    StopInternal();
    return NS_ERROR_FAILURE;
  }

  hr = mAudioSessionControl->RegisterAudioSessionNotification(this);
  if (FAILED(hr)) {
    StopInternal();
    return NS_ERROR_FAILURE;
  }

  mState = STARTED;

  return NS_OK;
}

void
AudioSession::StopInternal()
{
  static const nsID blankId = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0} };

  if (mAudioSessionControl) {
    mAudioSessionControl->SetGroupingParam((LPCGUID)&blankId, NULL);
    mAudioSessionControl->UnregisterAudioSessionNotification(this);
    mAudioSessionControl = nsnull;
  }
}

nsresult
AudioSession::Stop()
{
  NS_ABORT_IF_FALSE(mState == STARTED ||
                    mState == UNINITIALIZED || // XXXremove this
                    mState == FAILED,
                    "State invariants violated");
  mState = STOPPED;

  nsRefPtr<AudioSession> kungFuDeathGrip;
  kungFuDeathGrip.swap(sService);

  if (XRE_GetProcessType() != GeckoProcessType_Content)
    StopInternal();

  // At this point kungFuDeathGrip should be the only reference to AudioSession

  ::CoUninitialize();

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
  NS_ABORT_IF_FALSE(mState == FAILED ||
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
  NS_ABORT_IF_FALSE(mState == UNINITIALIZED,
                    "State invariants violated");
  NS_ABORT_IF_FALSE(XRE_GetProcessType() != GeckoProcessType_Default,
                    "Should never get here in a chrome process!");
  mState = CLONED;

  CopynsID(mSessionGroupingParameter, aID);
  mDisplayName = aSessionName;
  mIconPath = aIconPath;
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
    NS_NewRunnableMethod(this, &AudioSession::OnSessionDisconnectedInternal);
  NS_DispatchToMainThread(runnable);
  return S_OK;
}

nsresult
AudioSession::OnSessionDisconnectedInternal()
{
  if (!mAudioSessionControl)
    return NS_OK;

  mAudioSessionControl->UnregisterAudioSessionNotification(this);
  mAudioSessionControl = nsnull;

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
