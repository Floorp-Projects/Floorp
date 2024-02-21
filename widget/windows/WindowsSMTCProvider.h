/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_WINDOWS_WINDOWSSTMCPROVIDER_H_
#define WIDGET_WINDOWS_WINDOWSSTMCPROVIDER_H_

#ifndef __MINGW32__

#  include <functional>
#  include <Windows.Media.h>
#  include <wrl.h>

#  include "mozilla/dom/FetchImageHelper.h"
#  include "mozilla/dom/MediaController.h"
#  include "mozilla/dom/MediaControlKeySource.h"
#  include "mozilla/UniquePtr.h"

using ISMTC = ABI::Windows::Media::ISystemMediaTransportControls;
using SMTCProperty = ABI::Windows::Media::SystemMediaTransportControlsProperty;
using ISMTCDisplayUpdater =
    ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater;

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Storage::Streams::IDataWriter;
using ABI::Windows::Storage::Streams::IRandomAccessStream;
using ABI::Windows::Storage::Streams::IRandomAccessStreamReference;
using Microsoft::WRL::ComPtr;

class WindowsSMTCProvider final : public mozilla::dom::MediaControlKeySource {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WindowsSMTCProvider, override)

 public:
  WindowsSMTCProvider();

  bool IsOpened() const override;
  bool Open() override;
  void Close() override;

  void SetPlaybackState(
      mozilla::dom::MediaSessionPlaybackState aState) override;

  void SetMediaMetadata(
      const mozilla::dom::MediaMetadataBase& aMetadata) override;

  void SetSupportedMediaKeys(const MediaKeysArray& aSupportedKeys) override;

  void SetPositionState(
      const mozilla::Maybe<mozilla::dom::PositionState>& aState) override;

 private:
  ~WindowsSMTCProvider();
  void UnregisterEvents();
  bool RegisterEvents();

  void OnButtonPressed(mozilla::dom::MediaControlKey aKey) const;
  // Enable the SMTC interface
  bool EnableControl(bool aEnabled) const;
  // Sets the play, pause, next, previous, seekto buttons on the SMTC interface
  // by mSupportedKeys
  bool UpdateButtons();
  bool IsKeySupported(mozilla::dom::MediaControlKey aKey) const;
  bool EnableKey(mozilla::dom::MediaControlKey aKey, bool aEnable) const;

  void OnPositionChangeRequested(double aPosition) const;

  bool InitDisplayAndControls();

  // Sets the Metadata for the currently playing media and sets the playback
  // type to "MUSIC"
  bool SetMusicMetadata(const nsString& aArtist, const nsString& aTitle);

  // Sets one of the artwork to the SMTC interface asynchronously
  void LoadThumbnail(const nsTArray<mozilla::dom::MediaImage>& aArtwork);
  // Stores the image at index aIndex of the mArtwork to the Thumbnail
  // asynchronously
  void LoadImageAtIndex(const size_t aIndex);
  // Stores the raw binary data of an image to mImageStream and set it to the
  // Thumbnail asynchronously
  void LoadImage(const char* aImageData, uint32_t aDataSize);
  // Sets the Thumbnail to the image stored in mImageStream
  bool SetThumbnail(const nsAString& aUrl);
  void ClearThumbnail();

  bool UpdateThumbnail(const nsAString& aUrl);
  void CancelPendingStoreAsyncOperation() const;

  void ClearMetadata();

  bool mInitialized = false;

  // A bit table indicating what keys are enabled
  uint32_t mSupportedKeys = 0;

  ComPtr<ISMTC> mControls;
  ComPtr<ISMTCDisplayUpdater> mDisplay;

  // Use mImageDataWriter to write the binary data of image into mImageStream
  // and refer the image by mImageStreamReference and then set it to the SMTC
  // interface
  ComPtr<IDataWriter> mImageDataWriter;
  ComPtr<IRandomAccessStream> mImageStream;
  ComPtr<IRandomAccessStreamReference> mImageStreamReference;
  ComPtr<IAsyncOperation<unsigned int>> mStoreAsyncOperation;

  // mThumbnailUrl is the url of the current Thumbnail
  // mProcessingUrl is the url that is being processed. The process starts from
  // fetching an image from the url and then storing the fetched image to the
  // mImageStream. If mProcessingUrl is not empty, it means there is an image is
  // in processing
  // mThumbnailUrl and mProcessingUrl won't be set at the same time and they can
  // only be touched on main thread
  nsString mThumbnailUrl;
  nsString mProcessingUrl;

  // mArtwork can only be used in main thread in case of data racing
  CopyableTArray<mozilla::dom::MediaImage> mArtwork;
  size_t mNextImageIndex;

  mozilla::UniquePtr<mozilla::dom::FetchImageHelper> mImageFetcher;
  mozilla::MozPromiseRequestHolder<mozilla::dom::ImagePromise>
      mImageFetchRequest;

  HWND mWindow;  // handle to the invisible window

  // EventRegistrationTokens are used to have a handle on a callback (to remove
  // it again)
  EventRegistrationToken mButtonPressedToken;
  EventRegistrationToken mSeekRegistrationToken;
};

#endif  // __MINGW32__
#endif  // WIDGET_WINDOWS_WINDOWSSTMCPROVIDER_H_
