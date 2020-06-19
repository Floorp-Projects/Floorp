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

#  include "imgIEncoder.h"
#  include "nsMimeTypes.h"
#  include "mozilla/Assertions.h"
#  include "mozilla/Logging.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/WidgetUtils.h"
#  include "mozilla/WindowsVersion.h"

#  pragma comment(lib, "runtimeobject.lib")

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Storage::Streams;
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

bool WindowsSMTCProvider::IsOpened() const { return mInitialized; }

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

void WindowsSMTCProvider::Close() {
  MediaControlKeySource::Close();
  if (mInitialized) {  // Prevent calling Set methods when init failed
    SetPlaybackState(mozilla::dom::MediaSessionPlaybackState::None);
    SetControlAttributes(SMTCControlAttributes::DisableAll());
    mInitialized = false;
  }

  UnregisterEvents();

  // Cancel the pending image fetch process
  mImageFetchRequest.DisconnectIfExists();
  // Clear the cached image URL in case that image fetching is cancelled
  mImageSrc = EmptyString();
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

void WindowsSMTCProvider::SetMediaMetadata(
    const mozilla::dom::MediaMetadataBase& aMetadata) {
  MOZ_ASSERT(mInitialized);
  SetMusicMetadata(aMetadata.mArtist.get(), aMetadata.mTitle.get(),
                   aMetadata.mAlbum.get());
  RefreshDisplay();
  LoadThumbnail(aMetadata.mArtwork);
}

void WindowsSMTCProvider::UnregisterEvents() {
  if (mControls && mButtonPressedToken.value != 0) {
    mControls->remove_ButtonPressed(mButtonPressedToken);
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

void WindowsSMTCProvider::OnButtonPressed(mozilla::dom::MediaControlKey aKey) {
  for (auto& listener : mListeners) {
    listener->OnKeyPressed(aKey);
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

bool WindowsSMTCProvider::RefreshDisplay() {
  MOZ_ASSERT(mDisplay);
  return SUCCEEDED(mDisplay->Update());
}

bool WindowsSMTCProvider::SetMusicMetadata(const wchar_t* aArtist,
                                           const wchar_t* aTitle,
                                           const wchar_t* aAlbumArtist) {
  MOZ_ASSERT(mDisplay);
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

// The image buffer would be allocated in aStream whose size is aSize and the
// buffer head is aBuffer
static nsresult GetEncodedImageBuffer(imgIContainer* aImage,
                                      const nsACString& aMimeType,
                                      nsIInputStream** aStream, uint32_t* aSize,
                                      char** aBuffer) {
  nsCOMPtr<imgITools> imgTools = do_GetService("@mozilla.org/image/tools;1");
  if (!imgTools) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  nsresult rv = imgTools->EncodeImage(aImage, aMimeType, EmptyString(),
                                      getter_AddRefs(inputStream));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!inputStream) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<imgIEncoder> encoder = do_QueryInterface(inputStream);
  if (!encoder) {
    return NS_ERROR_FAILURE;
  }

  rv = encoder->GetImageBufferUsed(aSize);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = encoder->GetImageBuffer(aBuffer);
  if (NS_FAILED(rv)) {
    return rv;
  }

  encoder.forget(aStream);
  return NS_OK;
}

void WindowsSMTCProvider::LoadThumbnail(
    const nsTArray<mozilla::dom::MediaImage>& aArtwork) {
  MOZ_ASSERT(NS_IsMainThread());

  // We use the first image of aArtwork for now.
  // TODO:
  // 1. Give priorities to different images (e.g., sort by size or format)
  // 2. Fetch the images by the priorities: If fetching for the top-priority
  // image fails, try next one.
  // 3. No need to fetch the default image and do image processing since the the
  // default image is local file and it's trustworthy. For the default image, we
  // can use `CreateFromFile` to create the IRandomAccessStream. We should
  // probably cache it since it could be used very often.

  // Bail out if the URL starts with "file:///" since they cannot be fetched
  // below and may not be trustworthy.
  if (aArtwork.IsEmpty() ||
      aArtwork[0].mSrc.Find(NS_LITERAL_CSTRING("file:///"), false, 0, 0) == 0) {
    LOG("Clear the thumbnail since there is no valid image url");
    mImageFetchRequest.DisconnectIfExists();
    mImageSrc = EmptyString();
    ClearThumbnail();
    RefreshDisplay();
    return;
  }

  // Do nothing if the URL isn't changed. The image of the songs in the same
  // list may be same (e.g., Spotify)
  if (mImageSrc == aArtwork[0].mSrc) {
    LOG("Keep the thumbnail since the image url is same");
    return;
  }

  mImageFetchRequest.DisconnectIfExists();
  mImageSrc = aArtwork[0].mSrc;

  // Clean the out-of-dated thumbnail on the interface
  ClearThumbnail();
  RefreshDisplay();

  mImageFetcher =
      mozilla::MakeUnique<mozilla::dom::FetchImageHelper>(aArtwork[0]);
  RefPtr<WindowsSMTCProvider> self = this;
  mImageFetcher->FetchImage()
      ->Then(
          AbstractThread::MainThread(), __func__,
          [this, self](const nsCOMPtr<imgIContainer>& aImage) {
            LOG("The image is fetched successfully");
            mImageFetchRequest.Complete();

            // Although IMAGE_JPEG or IMAGE_BMP are valid types as well, but a
            // png image with transparent background will be converted into a
            // jpeg/bmp file with a colored background. IMAGE_PNG format seems
            // to be the best choice for now.
            uint32_t size = 0;
            char* src = nullptr;
            // Only used to hold the image data
            nsCOMPtr<nsIInputStream> inputStream;
            nsresult rv =
                GetEncodedImageBuffer(aImage, NS_LITERAL_CSTRING(IMAGE_PNG),
                                      getter_AddRefs(inputStream), &size, &src);
            if (NS_FAILED(rv) || !inputStream || size == 0 || !src) {
              LOG("Failed to get the image buffer info");
              return;
            }

            LoadImage(src, size);
          },
          [this, self](bool) {
            LOG("Failed to fetch image");
            mImageFetchRequest.Complete();
          })
      ->Track(mImageFetchRequest);
}

void WindowsSMTCProvider::LoadImage(const char* aImageData,
                                    uint32_t aDataSize) {
  // 1. Use mImageDataWriter to write the binary data of image into mImageStream
  // 2. Refer the image by mImageStreamReference and then set it to the SMTC
  // In case of the race condition between they are being destroyed and the
  // async operation for image loading, mImageDataWriter, mImageStream, and
  // mImageStreamReference are member variables

  HRESULT hr = ActivateInstance(
      HStringReference(
          RuntimeClass_Windows_Storage_Streams_InMemoryRandomAccessStream)
          .Get(),
      mImageStream.GetAddressOf());
  if (FAILED(hr)) {
    LOG("Failed to make mImageStream refer to an instance of "
        "InMemoryRandomAccessStream");
    return;
  }

  ComPtr<IOutputStream> outputStream;
  hr = mImageStream.As(&outputStream);
  if (FAILED(hr)) {
    LOG("Failed when query IOutputStream interface from mImageStream");
    return;
  }

  ComPtr<IDataWriterFactory> dataWriterFactory;
  hr = GetActivationFactory(
      HStringReference(RuntimeClass_Windows_Storage_Streams_DataWriter).Get(),
      dataWriterFactory.GetAddressOf());
  if (FAILED(hr)) {
    LOG("Failed to get an activation factory for IDataWriterFactory");
    return;
  }

  hr = dataWriterFactory->CreateDataWriter(outputStream.Get(),
                                           mImageDataWriter.GetAddressOf());
  if (FAILED(hr)) {
    LOG("Failed to create mImageDataWriter that writes data to mImageStream");
    return;
  }

  hr = mImageDataWriter->WriteBytes(
      aDataSize, reinterpret_cast<BYTE*>(const_cast<char*>(aImageData)));
  if (FAILED(hr)) {
    LOG("Failed to write data to mImageStream");
    return;
  }

  ComPtr<IAsyncOperation<unsigned int>> storeAsyncOperation;
  hr = mImageDataWriter->StoreAsync(&storeAsyncOperation);
  if (FAILED(hr)) {
    LOG("Failed to commit the written data to mImageStream, asynchronously");
    return;
  }

  // Upon the image is stored in mImageStream, set the image to the SMTC
  // interface
  auto onStoreCompleted = Callback<
      IAsyncOperationCompletedHandler<unsigned int>>(
      [this, self = RefPtr<WindowsSMTCProvider>(this)](
          IAsyncOperation<unsigned int>* aAsyncOp, AsyncStatus aStatus) {
        if (aStatus != AsyncStatus::Completed) {
          LOG("Asynchronous operation is not completed");
          return E_ABORT;
        }

        IAsyncInfo* asyncInfo;
        HRESULT hr = aAsyncOp->QueryInterface(
            IID_IAsyncInfo, reinterpret_cast<void**>(&asyncInfo));
        if (FAILED(hr)) {
          LOG("Failed when query IAsyncInfo from aAsyncOp");
          return hr;
        }

        MOZ_ASSERT(asyncInfo);
        asyncInfo->get_ErrorCode(&hr);
        if (FAILED(hr)) {
          LOG("Failed to get termination status of the asynchronous operation");
          return hr;
        }

        if (!SetThumbnail() || !RefreshDisplay()) {
          LOG("Failed to update thumbnail");
        }

        return S_OK;
      });

  hr = storeAsyncOperation->put_Completed(onStoreCompleted.Get());
  if (FAILED(hr)) {
    LOG("Failed to set callback on completeing the asynchronous operation");
  }
}

bool WindowsSMTCProvider::SetThumbnail() {
  MOZ_ASSERT(mDisplay);
  MOZ_ASSERT(mImageStream);

  ComPtr<IRandomAccessStreamReferenceStatics> streamRefFactory;

  HRESULT hr = GetActivationFactory(
      HStringReference(
          RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference)
          .Get(),
      streamRefFactory.GetAddressOf());
  if (FAILED(hr)) {
    LOG("Failed to get an activation factory for "
        "IRandomAccessStreamReferenceStatics type");
    return false;
  }

  hr = streamRefFactory->CreateFromStream(mImageStream.Get(),
                                          mImageStreamReference.GetAddressOf());
  if (FAILED(hr)) {
    LOG("Failed to create mImageStreamReference from mImageStream");
    return false;
  }

  return SUCCEEDED(mDisplay->put_Thumbnail(mImageStreamReference.Get()));
}

void WindowsSMTCProvider::ClearThumbnail() {
  MOZ_ASSERT(mDisplay);
  HRESULT hr = mDisplay->put_Thumbnail(nullptr);
  MOZ_ASSERT(SUCCEEDED(hr));
  Unused << hr;
}

#endif  // __MINGW32__
