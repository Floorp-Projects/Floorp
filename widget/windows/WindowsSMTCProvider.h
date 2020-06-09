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

#  include "mozilla\dom\MediaController.h"
#  include "mozilla\dom\MediaControlKeySource.h"
#  include "mozilla\Maybe.h"

using ISMTC = ABI::Windows::Media::ISystemMediaTransportControls;
using SMTCProperty = ABI::Windows::Media::SystemMediaTransportControlsProperty;
using IMSTCDisplayUpdater =
    ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater;

struct SMTCControlAttributes {
  bool mEnabled;
  bool mPlayPauseEnabled;
  bool mNextEnabled;
  bool mPreviousEnabled;

  static constexpr SMTCControlAttributes EnableAll() {
    return {true, true, true, true};
  }
  static constexpr SMTCControlAttributes DisableAll() {
    return {false, false, false, false};
  }
};

class WindowsSMTCProvider final : public mozilla::dom::MediaControlKeySource {
  NS_INLINE_DECL_REFCOUNTING(WindowsSMTCProvider, override)

 public:
  WindowsSMTCProvider();

  bool IsOpened() const override;
  bool Open() override;
  void Close() override;

  // Sets the state of the UI Panel (enabled, can use PlayPause, Next, Previous
  // Buttons)
  bool SetControlAttributes(SMTCControlAttributes aAttributes);

  void SetPlaybackState(
      mozilla::dom::MediaSessionPlaybackState aState) override;

  // Sets the Thumbnail for the currently playing media to the given URL.
  // Note: This method does not call update(), you need to do that manually.
  bool SetThumbnail(const wchar_t* aUrl);

  // Sets the Metadata for the currently playing media and sets the playback
  // type to "MUSIC" Note: This method does not call update(), you need to do
  // that manually.
  bool SetMusicMetadata(mozilla::Maybe<const wchar_t*> aArtist,
                        const wchar_t* aTitle,
                        mozilla::Maybe<const wchar_t*> aAlbumArtist);

  void SetMediaMetadata(
      const mozilla::dom::MediaMetadataBase& aMetadata) override;

  // TODO : modify the virtual control interface based on the supported keys
  void SetSupportedMediaKeys(const MediaKeysArray& aSupportedKeys) override {}

 private:
  ~WindowsSMTCProvider();
  void UnregisterEvents();
  bool RegisterEvents();
  bool InitDisplayAndControls();
  void OnButtonPressed(mozilla::dom::MediaControlKey aKey);

  // This method flushes the changed Media Metadata to the OS.
  bool Update();

  bool mInitialized = false;
  Microsoft::WRL::ComPtr<ISMTC> mControls;
  Microsoft::WRL::ComPtr<IMSTCDisplayUpdater> mDisplay;
  HWND mWindow;  // handle to the invisible window

  // EventRegistrationTokens are used to have a handle on a callback (to remove
  // it again)
  EventRegistrationToken mButtonPressedToken;
};

#endif  // __MINGW32__
#endif  // WIDGET_WINDOWS_WINDOWSSTMCPROVIDER_H_
