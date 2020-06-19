/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* mingw currently doesn't support windows.media.h, so we disable
 * the whole related class until this is fixed.
 * @TODO: Maybe contact MinGW Team for inclusion?*/
#ifndef __MINGW32__

#  include "WindowsSMTCProvider.h"

#  include <windows.h>
#  include <windows.media.h>
#  include <winsdkver.h>
#  include <wrl.h>

#  include "mozilla/Assertions.h"
#  include "mozilla/Logging.h"
#  include "mozilla/WidgetUtils.h"
#  include "mozilla/WindowsVersion.h"

#  pragma comment(lib, "runtimeobject.lib")

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Media;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace mozilla;

#  ifndef RuntimeClass_Windows_Media_SystemMediaTransportControls
#    define RuntimeClass_Windows_Media_SystemMediaTransportControls \
      L"Windows.Media.SystemMediaTransportControls"
#  endif

#  ifndef RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference
#    define RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference \
      L"Windows.Storage.Streams.RandomAccessStreamReference"
#  endif

#  ifndef ISystemMediaTransportControlsInterop
EXTERN_C const IID IID_ISystemMediaTransportControlsInterop;
MIDL_INTERFACE("ddb0472d-c911-4a1f-86d9-dc3d71a95f5a")
ISystemMediaTransportControlsInterop : public IInspectable {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetForWindow(
      /* [in] */ __RPC__in HWND appWindow,
      /* [in] */ __RPC__in REFIID riid,
      /* [iid_is][retval][out] */
      __RPC__deref_out_opt void** mediaTransportControl) = 0;
};
#  endif /* __ISystemMediaTransportControlsInterop_INTERFACE_DEFINED__ */

extern mozilla::LazyLogModule gMediaControlLog;

#  undef LOG
#  define LOG(msg, ...)                        \
    MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
            ("WindowSMTCProvider=%p, " msg, this, ##__VA_ARGS__))

WindowsSMTCProvider::WindowsSMTCProvider() {
  LOG("Creating an empty and invisible window");

  // In order to create a SMTC-Provider, we need a hWnd, which shall be created
  // dynamically from an invisible window. This leads to the following
  // boilerplate code.
  WNDCLASS wnd{};
  wnd.lpszClassName = L"Firefox-MediaKeys";
  wnd.hInstance = nullptr;
  wnd.lpfnWndProc = DefWindowProc;
  GetLastError();  // Clear the error
  RegisterClass(&wnd);
  MOZ_ASSERT(!GetLastError());

  mWindow = CreateWindowExW(0, L"Firefox-MediaKeys", L"Firefox Media Keys", 0,
                            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, nullptr,
                            nullptr, nullptr, nullptr);
  MOZ_ASSERT(mWindow);
  MOZ_ASSERT(!GetLastError());
}

WindowsSMTCProvider::~WindowsSMTCProvider() {
  // Dispose the window
  MOZ_ASSERT(mWindow);
  if (!DestroyWindow(mWindow)) {
    LOG("Failed to destroy the hidden window. Error Code: %d", GetLastError());
  }
  if (!UnregisterClass(L"Firefox-MediaKeys", nullptr)) {
    // Note that this is logged when the class wasn't even registered.
    LOG("Failed to unregister the class. Error Code: %d", GetLastError());
  }
}

static inline Maybe<mozilla::dom::MediaControlKey> TranslateKeycode(
    SystemMediaTransportControlsButton keycode) {
  switch (keycode) {
    case SystemMediaTransportControlsButton_Play:
      return Some(mozilla::dom::MediaControlKey::Play);
    case SystemMediaTransportControlsButton_Pause:
      return Some(mozilla::dom::MediaControlKey::Pause);
    case SystemMediaTransportControlsButton_Next:
      return Some(mozilla::dom::MediaControlKey::Nexttrack);
    case SystemMediaTransportControlsButton_Previous:
      return Some(mozilla::dom::MediaControlKey::Previoustrack);
    case SystemMediaTransportControlsButton_Stop:
      return Some(mozilla::dom::MediaControlKey::Stop);
    case SystemMediaTransportControlsButton_FastForward:
      return Some(mozilla::dom::MediaControlKey::Seekforward);
    case SystemMediaTransportControlsButton_Rewind:
      return Some(mozilla::dom::MediaControlKey::Seekbackward);
    default:
      return Nothing();  // Not supported Button
  }
}

bool WindowsSMTCProvider::RegisterEvents() {
  MOZ_ASSERT(mControls);
  auto self = RefPtr<WindowsSMTCProvider>(this);
  auto callbackbtnPressed = Callback<
      ITypedEventHandler<SystemMediaTransportControls*,
                         SystemMediaTransportControlsButtonPressedEventArgs*>>(
      [this, self](ISystemMediaTransportControls*,
                   ISystemMediaTransportControlsButtonPressedEventArgs* pArgs)
          -> HRESULT {
        MOZ_ASSERT(pArgs);
        SystemMediaTransportControlsButton btn;

        if (FAILED(pArgs->get_Button(&btn))) {
          LOG("SystemMediaTransportControls: ButtonPressedEvent - Could "
              "not get Button.");
          return S_OK;  // Propagating the error probably wouldn't help.
        }

        Maybe<mozilla::dom::MediaControlKey> keyCode = TranslateKeycode(btn);
        if (keyCode.isSome() && IsOpened()) {
          OnButtonPressed(keyCode.value());
        }
        return S_OK;
      });

  if (FAILED(mControls->add_ButtonPressed(callbackbtnPressed.Get(),
                                          &mButtonPressedToken))) {
    LOG("SystemMediaTransportControls: Failed at "
        "registerEvents().add_ButtonPressed()");
    return false;
  }

  return true;
}

void WindowsSMTCProvider::UnregisterEvents() {
  if (mControls && mButtonPressedToken.value != 0) {
    mControls->remove_ButtonPressed(mButtonPressedToken);
  }
}

bool WindowsSMTCProvider::InitDisplayAndControls() {
  // As Open() might be called multiple times, "cache" the results of the COM
  // API
  if (mControls && mDisplay) {
    return true;
  }
  ComPtr<ISystemMediaTransportControlsInterop> interop;
  HRESULT hr = GetActivationFactory(
      HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControls)
          .Get(),
      interop.GetAddressOf());
  if (FAILED(hr)) {
    LOG("SystemMediaTransportControls: Failed at instantiating the "
        "Interop object");
    return false;
  }
  MOZ_ASSERT(interop);

  if (!mControls && FAILED(interop->GetForWindow(
                        mWindow, IID_PPV_ARGS(mControls.GetAddressOf())))) {
    LOG("SystemMediaTransportControls: Failed at GetForWindow()");
    return false;
  }
  MOZ_ASSERT(mControls);

  if (!mDisplay &&
      FAILED(mControls->get_DisplayUpdater(mDisplay.GetAddressOf()))) {
    LOG("SystemMediaTransportControls: Failed at get_DisplayUpdater()");
  }

  MOZ_ASSERT(mDisplay);
  return true;
}

bool WindowsSMTCProvider::Open() {
  LOG("Opening Source");
  MOZ_ASSERT(!mInitialized);

  if (!IsWin8Point1OrLater()) {
    LOG("Windows 8.1 or later is required for Media Key Support");
    return false;
  }

  if (!InitDisplayAndControls()) {
    LOG("Failed to initialize the SMTC and its display");
    return false;
  }

  if (!SetControlAttributes(SMTCControlAttributes::EnableAll())) {
    LOG("Failed to set control attributes");
    return false;
  }

  if (!RegisterEvents()) {
    LOG("Failed to register SMTC key-event listener");
    return false;
  }

  mInitialized = true;
  SetPlaybackState(mozilla::dom::MediaSessionPlaybackState::None);
  return mInitialized;
}

bool WindowsSMTCProvider::IsOpened() const { return mInitialized; }

void WindowsSMTCProvider::Close() {
  MediaControlKeySource::Close();
  if (mInitialized) {  // Prevent calling Set methods when init failed
    SetPlaybackState(mozilla::dom::MediaSessionPlaybackState::None);
    SetControlAttributes(SMTCControlAttributes::DisableAll());
    mInitialized = false;
  }

  UnregisterEvents();
}

void WindowsSMTCProvider::SetPlaybackState(
    mozilla::dom::MediaSessionPlaybackState aState) {
  MOZ_ASSERT(mInitialized);
  MediaControlKeySource::SetPlaybackState(aState);

  HRESULT hr;

  // Note: we can't return the status of put_PlaybackStatus, but we can at least
  // assert it.
  switch (aState) {
    case mozilla::dom::MediaSessionPlaybackState::Paused:
      hr = mControls->put_PlaybackStatus(
          ABI::Windows::Media::MediaPlaybackStatus_Paused);
      break;
    case mozilla::dom::MediaSessionPlaybackState::Playing:
      hr = mControls->put_PlaybackStatus(
          ABI::Windows::Media::MediaPlaybackStatus_Playing);
      break;
    case mozilla::dom::MediaSessionPlaybackState::None:
      hr = mControls->put_PlaybackStatus(
          ABI::Windows::Media::MediaPlaybackStatus_Stopped);
      break;
      // MediaPlaybackStatus still supports Closed and Changing, which we don't
      // use (yet)
    default:
      MOZ_ASSERT_UNREACHABLE(
          "Enum Inconsitency between PlaybackState and WindowsSMTCProvider");
      break;
  }

  MOZ_ASSERT(SUCCEEDED(hr));
  Unused << hr;
}

bool WindowsSMTCProvider::SetControlAttributes(
    SMTCControlAttributes aAttributes) {
  MOZ_ASSERT(mControls);

  if (FAILED(mControls->put_IsEnabled(aAttributes.mEnabled))) {
    return false;
  }
  if (FAILED(mControls->put_IsPauseEnabled(aAttributes.mPlayPauseEnabled))) {
    return false;
  }
  if (FAILED(mControls->put_IsPlayEnabled(aAttributes.mPlayPauseEnabled))) {
    return false;
  }
  if (FAILED(mControls->put_IsNextEnabled(aAttributes.mNextEnabled))) {
    return false;
  }
  if (FAILED(mControls->put_IsPreviousEnabled(aAttributes.mPreviousEnabled))) {
    return false;
  }

  return true;
}

bool WindowsSMTCProvider::SetThumbnail(const wchar_t* aUrl) {
  MOZ_ASSERT(mInitialized);
  ComPtr<IActivationFactory> streamRefFactory;
  HRESULT hr = GetActivationFactory(
      HStringReference(
          RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference)
          .Get(),
      &streamRefFactory);
  if (FAILED(hr)) {
    LOG("SystemMediaTransportControls: Failed at "
        "setThumbnail.GetActivationFactory(&StreamRefFactory)");
    return false;
  }

  ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStreamReferenceStatics>
      streamRefStatic;
  hr = streamRefFactory.As(&streamRefStatic);
  if (FAILED(hr)) {
    LOG("SystemMediaTransportControls: Failed at "
        "setThumbnail.StreamRefFactory.As()");
    return false;
  }

  ComPtr<IUriRuntimeClass> uri;
  ComPtr<IUriRuntimeClassFactory> uriFactory;
  hr = GetActivationFactory(
      HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(), &uriFactory);
  if (FAILED(hr)) {
    LOG("SystemMediaTransportControls: Failed at "
        "setThumbnail.GetActivationFactory(&uriFactory)");
    return false;
  }

  hr = uriFactory->CreateUri(HStringReference(aUrl).Get(), uri.GetAddressOf());
  if (FAILED(hr)) {
    LOG("SystemMediaTransportControls: Failed at setThumbnail.CreateUri()");
    return false;
  }

  ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStreamReference>
      thumbnail;
  // When Thumbnail would be a local file: CreateFromFile(file.Get(),
  // &streamRef);
  hr = streamRefStatic->CreateFromUri(uri.Get(), thumbnail.GetAddressOf());
  if (FAILED(hr)) {
    LOG("SystemMediaTransportControls: Failed at "
        "setThumbnail.CreateFromUri()");
    return false;
  }

  return SUCCEEDED(mDisplay->put_Thumbnail(thumbnail.Get()));
}

bool WindowsSMTCProvider::SetMusicMetadata(const wchar_t* aArtist,
                                           const wchar_t* aTitle,
                                           const wchar_t* aAlbumArtist) {
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(aArtist);
  MOZ_ASSERT(aTitle);
  MOZ_ASSERT(aAlbumArtist);
  ComPtr<IMusicDisplayProperties> musicProps;

  HRESULT hr = mDisplay->put_Type(MediaPlaybackType::MediaPlaybackType_Music);
  MOZ_ASSERT(SUCCEEDED(hr));
  Unused << hr;
  hr = mDisplay->get_MusicProperties(musicProps.GetAddressOf());
  if (FAILED(hr)) {
    LOG("Failed to get music properties");
    return false;
  }

  hr = musicProps->put_Artist(HStringReference(aArtist).Get());
  if (FAILED(hr)) {
    LOG("Failed to set the music's artist");
    return false;
  }

  hr = musicProps->put_Title(HStringReference(aTitle).Get());
  if (FAILED(hr)) {
    LOG("Failed to set the music's title");
    return false;
  }

  hr = musicProps->put_AlbumArtist(HStringReference(aAlbumArtist).Get());
  if (FAILED(hr)) {
    LOG("Failed to set the music's album");
    return false;
  }

  return true;
}

void WindowsSMTCProvider::SetMediaMetadata(
    const mozilla::dom::MediaMetadataBase& aMetadata) {
  SetMusicMetadata(aMetadata.mArtist.get(), aMetadata.mTitle.get(),
                   aMetadata.mAlbum.get());
  Update();
}

void WindowsSMTCProvider::OnButtonPressed(mozilla::dom::MediaControlKey aKey) {
  for (auto& listener : mListeners) {
    listener->OnKeyPressed(aKey);
  }
}

bool WindowsSMTCProvider::Update() {
  MOZ_ASSERT(mInitialized);
  return SUCCEEDED(mDisplay->Update());
}

#endif  // __MINGW32__
