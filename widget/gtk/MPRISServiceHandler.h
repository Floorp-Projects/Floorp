/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_GTK_MPRIS_SERVICE_HANDLER_H_
#define WIDGET_GTK_MPRIS_SERVICE_HANDLER_H_

#include <gio/gio.h>
#include "mozilla/dom/FetchImageHelper.h"
#include "mozilla/dom/MediaControlKeySource.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsIFile.h"
#include "nsMimeTypes.h"
#include "nsString.h"

#define DBUS_MPRIS_SERVICE_NAME "org.mpris.MediaPlayer2.firefox"
#define DBUS_MPRIS_OBJECT_PATH "/org/mpris/MediaPlayer2"
#define DBUS_MPRIS_INTERFACE "org.mpris.MediaPlayer2"
#define DBUS_MPRIS_PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
#define DBUS_MPRIS_TRACK_PATH "/org/mpris/MediaPlayer2/firefox"

namespace mozilla {
namespace widget {

/**
 * This class implements the "MPRIS" D-Bus Service
 * (https://specifications.freedesktop.org/mpris-spec/2.2),
 * which is used to communicate with the Desktop Environment about the
 * Multimedia playing in Gecko.
 * Note that this interface requires many methods which may not be supported by
 * Gecko, the interface
 * however provides CanXYZ properties for these methods, so the method is
 * defined but won't be executed.
 *
 * Also note that the following defines are for parts that the MPRIS Spec
 * defines optional. The code won't
 * compile with any of the defines set, yet, as those aren't implemented yet and
 * probably never will be of
 * use for gecko. For sake of completeness, they have been added until the
 * decision about their implementation
 * is finally made.
 *
 * The constexpr'ed methods are capabilities of the user agent known at compile
 * time, e.g. we decided at
 * compile time whether we ever want to support closing the user agent via MPRIS
 * (Quit() and CanQuit()).
 *
 * Other properties like CanPlay() might depend on the runtime state (is there
 * media available for playback?)
 * and thus aren't a constexpr but merely a const method.
 */
class MPRISServiceHandler final : public dom::MediaControlKeySource {
  NS_INLINE_DECL_REFCOUNTING(MPRISServiceHandler, override)
 public:
  // Note that this constructor does NOT initialize the MPRIS Service but only
  // this class. The method Open() is responsible for registering and MAY FAIL.

  MPRISServiceHandler();
  bool Open() override;
  void Close() override;
  bool IsOpened() const override;

  // From the EventSource.
  void SetPlaybackState(dom::MediaSessionPlaybackState aState) override;

  // GetPlaybackState returns dom::PlaybackState. GetPlaybackStatus returns this
  // state converted into d-bus variants.
  GVariant* GetPlaybackStatus() const;

  const char* Identity() const;
  const char* DesktopEntry() const;
  bool PressKey(dom::MediaControlKey aKey) const;

  void SetMediaMetadata(const dom::MediaMetadataBase& aMetadata) override;
  GVariant* GetMetadataAsGVariant() const;

  void SetSupportedMediaKeys(const MediaKeysArray& aSupportedKeys) override;

  bool IsMediaKeySupported(dom::MediaControlKey aKey) const;

  void OwnName(GDBusConnection* aConnection);

 private:
  ~MPRISServiceHandler();

  // Note: Registration Ids for the D-Bus start with 1, so a value of 0
  // indicates an error (or an object which wasn't initialized yet)

  // a handle to our bus registration/ownership
  guint mOwnerId = 0;
  // This is for the interface org.mpris.MediaPlayer2
  guint mRootRegistrationId = 0;
  // This is for the interface org.mpris.MediaPlayer2.Player
  guint mPlayerRegistrationId = 0;
  RefPtr<GDBusNodeInfo> mIntrospectionData;
  GDBusConnection* mConnection = nullptr;
  bool mInitialized = false;
  nsAutoCString mIdentity;
  nsAutoCString mDesktopEntry;

  // The image format used in MPRIS is based on the mMimeType here. Although
  // IMAGE_JPEG or IMAGE_BMP are valid types as well but a png image with
  // transparent background will be converted into a jpeg/bmp file with a
  // colored background IMAGE_PNG format seems to be the best choice for now.
  nsCString mMimeType{IMAGE_PNG};

  // A bitmask indicating what keys are enabled
  uint32_t mSupportedKeys = 0;

  class MPRISMetadata : public dom::MediaMetadataBase {
   public:
    MPRISMetadata() = default;
    ~MPRISMetadata() = default;

    void UpdateFromMetadataBase(const dom::MediaMetadataBase& aMetadata) {
      mTitle = aMetadata.mTitle;
      mArtist = aMetadata.mArtist;
      mAlbum = aMetadata.mAlbum;
      mArtwork = aMetadata.mArtwork;
    }
    void Clear() {
      UpdateFromMetadataBase(MediaMetadataBase::EmptyData());
      mArtUrl.Truncate();
    }

    nsCString mArtUrl;
  };
  MPRISMetadata mMPRISMetadata;

  // The saved image file fetched from the URL
  nsCOMPtr<nsIFile> mLocalImageFile;
  nsCOMPtr<nsIFile> mLocalImageFolder;

  UniquePtr<dom::FetchImageHelper> mImageFetcher;
  MozPromiseRequestHolder<dom::ImagePromise> mImageFetchRequest;

  nsString mFetchingUrl;
  nsString mCurrentImageUrl;

  size_t mNextImageIndex = 0;

  // Load the image at index aIndex of the metadta's artwork to MPRIS
  // asynchronously
  void LoadImageAtIndex(const size_t aIndex);
  bool SetImageToDisplay(const char* aImageData, uint32_t aDataSize);

  bool RenewLocalImageFile(const char* aImageData, uint32_t aDataSize);
  bool InitLocalImageFile();
  bool InitLocalImageFolder();
  void RemoveAllLocalImages();
  bool LocalImageFolderExists();

  // Queries nsAppInfo to get the branded browser name and vendor
  void InitIdentity();

  // non-public API, called from events
  void OnNameAcquired(GDBusConnection* aConnection, const gchar* aName);
  void OnNameLost(GDBusConnection* aConnection, const gchar* aName);
  void OnBusAcquired(GDBusConnection* aConnection, const gchar* aName);

  static void OnNameAcquiredStatic(GDBusConnection* aConnection,
                                   const gchar* aName, gpointer aUserData);
  static void OnNameLostStatic(GDBusConnection* aConnection, const gchar* aName,
                               gpointer aUserData);
  static void OnBusAcquiredStatic(GDBusConnection* aConnection,
                                  const gchar* aName, gpointer aUserData);

  void EmitEvent(dom::MediaControlKey aKey) const;

  bool EmitMetadataChanged() const;

  void SetMediaMetadataInternal(const dom::MediaMetadataBase& aMetadata,
                                bool aClearArtUrl = true);

  bool EmitSupportedKeyChanged(dom::MediaControlKey aKey,
                               bool aSupported) const;

  bool EmitPropertiesChangedSignal(GVariant* aParameters) const;

  void ClearMetadata();

  RefPtr<GCancellable> mDBusGetCancellable;

  nsCString mServiceName;
  void SetServiceName(const char* aName);
  const char* GetServiceName();
};

}  // namespace widget
}  // namespace mozilla

#endif  // WIDGET_GTK_MPRIS_SERVICE_HANDLER_H_
