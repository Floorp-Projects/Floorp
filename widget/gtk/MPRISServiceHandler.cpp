/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MPRISServiceHandler.h"

#include <stdint.h>
#include <inttypes.h>
#include <unordered_map>

#include "MPRISInterfaceDescription.h"
#include "mozilla/dom/MediaControlUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Sprintf.h"
#include "nsIXULAppInfo.h"
#include "nsIOutputStream.h"
#include "nsServiceManagerUtils.h"

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MPRISServiceHandler=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace widget {

// A global counter tracking the total images saved in the system and it will be
// used to form a unique image file name.
static uint32_t gImageNumber = 0;

static inline Maybe<mozilla::dom::MediaControlKey> GetMediaControlKey(
    const gchar* aMethodName) {
  const std::unordered_map<std::string, mozilla::dom::MediaControlKey> map = {
      {"Raise", mozilla::dom::MediaControlKey::Focus},
      {"Next", mozilla::dom::MediaControlKey::Nexttrack},
      {"Previous", mozilla::dom::MediaControlKey::Previoustrack},
      {"Pause", mozilla::dom::MediaControlKey::Pause},
      {"PlayPause", mozilla::dom::MediaControlKey::Playpause},
      {"Stop", mozilla::dom::MediaControlKey::Stop},
      {"Play", mozilla::dom::MediaControlKey::Play}};

  auto it = map.find(aMethodName);
  return (it == map.end() ? Nothing() : Some(it->second));
}

static void HandleMethodCall(GDBusConnection* aConnection, const gchar* aSender,
                             const gchar* aObjectPath,
                             const gchar* aInterfaceName,
                             const gchar* aMethodName, GVariant* aParameters,
                             GDBusMethodInvocation* aInvocation,
                             gpointer aUserData) {
  MOZ_ASSERT(aUserData);
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<mozilla::dom::MediaControlKey> key = GetMediaControlKey(aMethodName);
  if (key.isNothing()) {
    g_dbus_method_invocation_return_error(
        aInvocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED,
        "Method %s.%s.%s not supported", aObjectPath, aInterfaceName,
        aMethodName);
    return;
  }

  MPRISServiceHandler* handler = static_cast<MPRISServiceHandler*>(aUserData);
  handler->PressKey(key.value());
}

enum class Property : uint8_t {
  eIdentity,
  eHasTrackList,
  eCanRaise,
  eCanQuit,
  eSupportedUriSchemes,
  eSupportedMimeTypes,
  eCanGoNext,
  eCanGoPrevious,
  eCanPlay,
  eCanPause,
  eCanSeek,
  eCanControl,
  eGetPlaybackStatus,
  eGetMetadata,
};

static inline Maybe<Property> GetProperty(const gchar* aPropertyName) {
  const std::unordered_map<std::string, Property> map = {
      // org.mpris.MediaPlayer2 properties
      {"Identity", Property::eIdentity},
      {"HasTrackList", Property::eHasTrackList},
      {"CanRaise", Property::eCanRaise},
      {"CanQuit", Property::eCanQuit},
      {"SupportedUriSchemes", Property::eSupportedUriSchemes},
      {"SupportedMimeTypes", Property::eSupportedMimeTypes},
      // org.mpris.MediaPlayer2.Player properties
      {"CanGoNext", Property::eCanGoNext},
      {"CanGoPrevious", Property::eCanGoPrevious},
      {"CanPlay", Property::eCanPlay},
      {"CanPause", Property::eCanPause},
      {"CanSeek", Property::eCanSeek},
      {"CanControl", Property::eCanControl},
      {"PlaybackStatus", Property::eGetPlaybackStatus},
      {"Metadata", Property::eGetMetadata}};

  auto it = map.find(aPropertyName);
  return (it == map.end() ? Nothing() : Some(it->second));
}

static GVariant* HandleGetProperty(GDBusConnection* aConnection,
                                   const gchar* aSender,
                                   const gchar* aObjectPath,
                                   const gchar* aInterfaceName,
                                   const gchar* aPropertyName, GError** aError,
                                   gpointer aUserData) {
  MOZ_ASSERT(aUserData);
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<Property> property = GetProperty(aPropertyName);
  if (property.isNothing()) {
    g_set_error(aError, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED,
                "%s.%s %s is not supported", aObjectPath, aInterfaceName,
                aPropertyName);
    return nullptr;
  }

  MPRISServiceHandler* handler = static_cast<MPRISServiceHandler*>(aUserData);
  switch (property.value()) {
    case Property::eSupportedUriSchemes:
    case Property::eSupportedMimeTypes:
      // No plan to implement OpenUri for now
      return g_variant_new_strv(nullptr, 0);
    case Property::eGetPlaybackStatus:
      return handler->GetPlaybackStatus();
    case Property::eGetMetadata:
      return handler->GetMetadataAsGVariant();
    case Property::eIdentity:
      return g_variant_new_string(handler->Identity());
    case Property::eHasTrackList:
    case Property::eCanQuit:
    case Property::eCanSeek:
      return g_variant_new_boolean(false);
    // Play/Pause would be blocked if CanControl is false
    case Property::eCanControl:
    case Property::eCanRaise:
    case Property::eCanGoNext:
    case Property::eCanGoPrevious:
    case Property::eCanPlay:
    case Property::eCanPause:
      return g_variant_new_boolean(true);
  }

  MOZ_ASSERT_UNREACHABLE("Switch statement is incomplete");
  return nullptr;
}

static gboolean HandleSetProperty(GDBusConnection* aConnection,
                                  const gchar* aSender,
                                  const gchar* aObjectPath,
                                  const gchar* aInterfaceName,
                                  const gchar* aPropertyName, GVariant* aValue,
                                  GError** aError, gpointer aUserData) {
  MOZ_ASSERT(aUserData);
  MOZ_ASSERT(NS_IsMainThread());
  g_set_error(aError, G_IO_ERROR, G_IO_ERROR_FAILED,
              "%s:%s setting is not supported", aInterfaceName, aPropertyName);
  return false;
}

static const GDBusInterfaceVTable gInterfaceVTable = {
    HandleMethodCall, HandleGetProperty, HandleSetProperty};

void MPRISServiceHandler::OnNameAcquiredStatic(GDBusConnection* aConnection,
                                               const gchar* aName,
                                               gpointer aUserData) {
  MOZ_ASSERT(aUserData);
  static_cast<MPRISServiceHandler*>(aUserData)->OnNameAcquired(aConnection,
                                                               aName);
}

void MPRISServiceHandler::OnNameLostStatic(GDBusConnection* aConnection,
                                           const gchar* aName,
                                           gpointer aUserData) {
  MOZ_ASSERT(aUserData);
  static_cast<MPRISServiceHandler*>(aUserData)->OnNameLost(aConnection, aName);
}

void MPRISServiceHandler::OnBusAcquiredStatic(GDBusConnection* aConnection,
                                              const gchar* aName,
                                              gpointer aUserData) {
  MOZ_ASSERT(aUserData);
  static_cast<MPRISServiceHandler*>(aUserData)->OnBusAcquired(aConnection,
                                                              aName);
}

void MPRISServiceHandler::OnNameAcquired(GDBusConnection* aConnection,
                                         const gchar* aName) {
  LOG("OnNameAcquired: %s", aName);
  mConnection = aConnection;
}

void MPRISServiceHandler::OnNameLost(GDBusConnection* aConnection,
                                     const gchar* aName) {
  LOG("OnNameLost: %s", aName);
  mConnection = nullptr;
  if (!mRootRegistrationId) {
    return;
  }

  if (g_dbus_connection_unregister_object(aConnection, mRootRegistrationId)) {
    mRootRegistrationId = 0;
  } else {
    // Note: Most code examples in the internet probably dont't even check the
    // result here, but
    // according to the spec it _can_ return false.
    LOG("Unable to unregister root object from within onNameLost!");
  }

  if (!mPlayerRegistrationId) {
    return;
  }

  if (g_dbus_connection_unregister_object(aConnection, mPlayerRegistrationId)) {
    mPlayerRegistrationId = 0;
  } else {
    // Note: Most code examples in the internet probably dont't even check the
    // result here, but
    // according to the spec it _can_ return false.
    LOG("Unable to unregister object from within onNameLost!");
  }
}

void MPRISServiceHandler::OnBusAcquired(GDBusConnection* aConnection,
                                        const gchar* aName) {
  GError* error = nullptr;
  LOG("OnBusAcquired: %s", aName);

  mRootRegistrationId = g_dbus_connection_register_object(
      aConnection, DBUS_MPRIS_OBJECT_PATH, mIntrospectionData->interfaces[0],
      &gInterfaceVTable, this, /* user_data */
      nullptr,                 /* user_data_free_func */
      &error);                 /* GError** */

  if (mRootRegistrationId == 0) {
    LOG("Failed at root registration: %s",
        error ? error->message : "Unknown Error");
    if (error) {
      g_error_free(error);
    }
    return;
  }

  mPlayerRegistrationId = g_dbus_connection_register_object(
      aConnection, DBUS_MPRIS_OBJECT_PATH, mIntrospectionData->interfaces[1],
      &gInterfaceVTable, this, /* user_data */
      nullptr,                 /* user_data_free_func */
      &error);                 /* GError** */

  if (mPlayerRegistrationId == 0) {
    LOG("Failed at object registration: %s",
        error ? error->message : "Unknown Error");
    if (error) {
      g_error_free(error);
    }
  }
}

bool MPRISServiceHandler::Open() {
  MOZ_ASSERT(!mInitialized);
  MOZ_ASSERT(NS_IsMainThread());
  GError* error = nullptr;
  gchar serviceName[256];

  InitIdentity();
  SprintfLiteral(serviceName, DBUS_MRPIS_SERVICE_NAME ".instance%d", getpid());
  mOwnerId =
      g_bus_own_name(G_BUS_TYPE_SESSION, serviceName,
                     // Enter a waiting queue until this service name is free
                     // (likely another FF instance is running/has been crashed)
                     G_BUS_NAME_OWNER_FLAGS_NONE, OnBusAcquiredStatic,
                     OnNameAcquiredStatic, OnNameLostStatic, this, nullptr);

  /* parse introspection data */
  mIntrospectionData = g_dbus_node_info_new_for_xml(introspection_xml, &error);

  if (!mIntrospectionData) {
    LOG("Failed at parsing XML Interface definition: %s",
        error ? error->message : "Unknown Error");
    if (error) {
      g_error_free(error);
    }
    return false;
  }

  mInitialized = true;
  return true;
}

MPRISServiceHandler::~MPRISServiceHandler() {
  MOZ_ASSERT(!mInitialized);  // Close hasn't been called!
}

void MPRISServiceHandler::Close() {
  gchar serviceName[256];
  SprintfLiteral(serviceName, DBUS_MRPIS_SERVICE_NAME ".instance%d", getpid());

  OnNameLost(mConnection, serviceName);

  if (mOwnerId != 0) {
    g_bus_unown_name(mOwnerId);
  }
  if (mIntrospectionData) {
    g_dbus_node_info_unref(mIntrospectionData);
  }

  mInitialized = false;
  MediaControlKeySource::Close();

  mImageFetchRequest.DisconnectIfExists();

  RemoveAllLocalImages();
  mMPRISMetadata.Clear();

  mCurrentImageUrl = EmptyString();
  mFetchingUrl = EmptyString();

  mNextImageIndex = 0;
}

bool MPRISServiceHandler::IsOpened() const { return mInitialized; }

void MPRISServiceHandler::InitIdentity() {
  nsresult rv;
  nsAutoCString appName;
  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1", &rv);

  MOZ_ASSERT(NS_SUCCEEDED(rv));
  rv = appInfo->GetVendor(mIdentity);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  rv = appInfo->GetName(appName);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  mIdentity.Append(' ');
  mIdentity.Append(appName);
}

const char* MPRISServiceHandler::Identity() const {
  MOZ_ASSERT(mInitialized);
  return mIdentity.get();
}

void MPRISServiceHandler::PressKey(mozilla::dom::MediaControlKey aKey) const {
  MOZ_ASSERT(mInitialized);
  LOG("Press %s", ToMediaControlKeyStr(aKey));
  EmitEvent(aKey);
}

void MPRISServiceHandler::SetPlaybackState(
    dom::MediaSessionPlaybackState aState) {
  LOG("SetPlaybackState");
  if (mPlaybackState == aState) {
    return;
  }

  MediaControlKeySource::SetPlaybackState(aState);

  if (!mConnection) {
    return;  // No D-Bus Connection, no event
  }

  GVariant* state = GetPlaybackStatus();

  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{sv}", "PlaybackStatus", state);

  GVariant* parameters = g_variant_new(
      "(sa{sv}as)", DBUS_MPRIS_PLAYER_INTERFACE, &builder, nullptr);
  GError* error = nullptr;
  if (!g_dbus_connection_emit_signal(mConnection, nullptr,
                                     DBUS_MPRIS_OBJECT_PATH,
                                     "org.freedesktop.DBus.Properties",
                                     "PropertiesChanged", parameters, &error)) {
    LOG("Failed at emitting MPRIS property changes for 'PlaybackStatus': %s",
        error ? error->message : "Unknown Error");
    if (error) {
      g_error_free(error);
    }
  }
}

GVariant* MPRISServiceHandler::GetPlaybackStatus() const {
  switch (GetPlaybackState()) {
    case dom::MediaSessionPlaybackState::Playing:
      return g_variant_new_string("Playing");
    case dom::MediaSessionPlaybackState::Paused:
      return g_variant_new_string("Paused");
    case dom::MediaSessionPlaybackState::None:
      return g_variant_new_string("Stopped");
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid Playback State");
      return nullptr;
  }
}

void MPRISServiceHandler::SetMediaMetadata(
    const dom::MediaMetadataBase& aMetadata) {
  // Reset the index of the next available image to be fetched in the artwork,
  // before checking the fetching process should be started or not. The image
  // fetching process could be skipped if the image being fetching currently is
  // in the artwork. If the current image fetching fails, the next availabe
  // candidate should be the first image in the latest artwork
  mNextImageIndex = 0;

  // No need to fetch a MPRIS image if
  // 1) MPRIS image is being fetched, and the one in fetching is in the artwork
  // 2) MPRIS image is not being fetched, and the one in use is in the artwork
  if (!mFetchingUrl.IsEmpty()) {
    if (mozilla::dom::IsImageIn(aMetadata.mArtwork, mFetchingUrl)) {
      LOG("No need to load MPRIS image. The one being processed is in the "
          "artwork");
      // Set MPRIS without the image first. The image will be loaded to MPRIS
      // asynchronously once it's fetched and saved into a local file
      SetMediaMetadataInternal(aMetadata);
      return;
    }
  } else if (!mCurrentImageUrl.IsEmpty()) {
    if (mozilla::dom::IsImageIn(aMetadata.mArtwork, mCurrentImageUrl)) {
      LOG("No need to load MPRIS image. The one in use is in the artwork");
      SetMediaMetadataInternal(aMetadata, false);
      return;
    }
  }

  // Set MPRIS without the image first then load the image to MPRIS
  // asynchronously
  SetMediaMetadataInternal(aMetadata);
  LoadImageAtIndex(mNextImageIndex++);
}

bool MPRISServiceHandler::EmitMetadataChanged() const {
  if (!mConnection) {
    LOG("No D-Bus Connection. Drop the update.");
    return false;
  }

  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{sv}", "Metadata", GetMetadataAsGVariant());

  GVariant* parameters = g_variant_new(
      "(sa{sv}as)", DBUS_MPRIS_PLAYER_INTERFACE, &builder, nullptr);
  GError* error = nullptr;
  if (!g_dbus_connection_emit_signal(mConnection, nullptr,
                                     DBUS_MPRIS_OBJECT_PATH,
                                     "org.freedesktop.DBus.Properties",
                                     "PropertiesChanged", parameters, &error)) {
    LOG("Failed at emitting MPRIS property changes for 'Metadata': %s:",
        error ? error->message : "Unknown Error");
    if (error) {
      g_error_free(error);
    }
    return false;
  }

  return true;
}

void MPRISServiceHandler::SetMediaMetadataInternal(
    const dom::MediaMetadataBase& aMetadata, bool aClearArtUrl) {
  mMPRISMetadata.UpdateFromMetadataBase(aMetadata);
  if (aClearArtUrl) {
    mMPRISMetadata.mArtUrl = EmptyCString();
  }
  EmitMetadataChanged();
}

void MPRISServiceHandler::LoadImageAtIndex(const size_t aIndex) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aIndex >= mMPRISMetadata.mArtwork.Length()) {
    LOG("Stop loading image to MPRIS. No available image");
    mImageFetchRequest.DisconnectIfExists();
    return;
  }

  const mozilla::dom::MediaImage& image = mMPRISMetadata.mArtwork[aIndex];

  if (!mozilla::dom::IsValidImageUrl(image.mSrc)) {
    LOG("Skip the image with invalid URL. Try next image");
    LoadImageAtIndex(mNextImageIndex++);
    return;
  }

  mImageFetchRequest.DisconnectIfExists();
  mFetchingUrl = image.mSrc;

  mImageFetcher = mozilla::MakeUnique<mozilla::dom::FetchImageHelper>(image);
  RefPtr<MPRISServiceHandler> self = this;
  mImageFetcher->FetchImage()
      ->Then(
          AbstractThread::MainThread(), __func__,
          [this, self](const nsCOMPtr<imgIContainer>& aImage) {
            LOG("The image is fetched successfully");
            mImageFetchRequest.Complete();

            uint32_t size = 0;
            char* data = nullptr;
            // Only used to hold the image data
            nsCOMPtr<nsIInputStream> inputStream;
            nsresult rv = mozilla::dom::GetEncodedImageBuffer(
                aImage, mMimeType, getter_AddRefs(inputStream), &size, &data);
            if (NS_FAILED(rv) || !inputStream || size == 0 || !data) {
              LOG("Failed to get the image buffer info. Try next image");
              LoadImageAtIndex(mNextImageIndex++);
              return;
            }

            if (SetImageToDisplay(data, size)) {
              mCurrentImageUrl = mFetchingUrl;
              LOG("The MPRIS image is updated to the image from: %s",
                  NS_ConvertUTF16toUTF8(mCurrentImageUrl).get());
            } else {
              LOG("Failed to set image to MPRIS");
              mCurrentImageUrl = EmptyString();
            }

            mFetchingUrl = EmptyString();
          },
          [this, self](bool) {
            LOG("Failed to fetch image. Try next image");
            mImageFetchRequest.Complete();
            mFetchingUrl = EmptyString();
            LoadImageAtIndex(mNextImageIndex++);
          })
      ->Track(mImageFetchRequest);
}

bool MPRISServiceHandler::SetImageToDisplay(const char* aImageData,
                                            uint32_t aDataSize) {
  if (!RenewLocalImageFile(aImageData, aDataSize)) {
    return false;
  }
  MOZ_ASSERT(mLocalImageFile);

  mMPRISMetadata.mArtUrl = nsCString("file://");
  mMPRISMetadata.mArtUrl.Append(mLocalImageFile->NativePath());

  LOG("The image file is created at %s", mMPRISMetadata.mArtUrl.get());
  return EmitMetadataChanged();
}

bool MPRISServiceHandler::RenewLocalImageFile(const char* aImageData,
                                              uint32_t aDataSize) {
  MOZ_ASSERT(aImageData);
  MOZ_ASSERT(aDataSize != 0);

  if (!InitLocalImageFile()) {
    LOG("Failed to create a new image");
    return false;
  }

  MOZ_ASSERT(mLocalImageFile);
  nsCOMPtr<nsIOutputStream> out;
  NS_NewLocalFileOutputStream(getter_AddRefs(out), mLocalImageFile,
                              PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  uint32_t written;
  nsresult rv = out->Write(aImageData, aDataSize, &written);
  if (NS_FAILED(rv) || written != aDataSize) {
    LOG("Failed to write an image file");
    RemoveAllLocalImages();
    return false;
  }

  return true;
}

static const char* GetImageFileExtension(const char* aMimeType) {
  MOZ_ASSERT(strcmp(aMimeType, IMAGE_PNG) == 0);
  return "png";
}

bool MPRISServiceHandler::InitLocalImageFile() {
  RemoveAllLocalImages();

  if (!InitLocalImageFolder()) {
    return false;
  }

  MOZ_ASSERT(mLocalImageFolder);
  MOZ_ASSERT(!mLocalImageFile);
  nsresult rv = mLocalImageFolder->Clone(getter_AddRefs(mLocalImageFile));
  if (NS_FAILED(rv)) {
    LOG("Failed to get the image folder");
    return false;
  }

  auto cleanup =
      MakeScopeExit([this, self = RefPtr<MPRISServiceHandler>(this)] {
        mLocalImageFile = nullptr;
      });

  // Create an unique file name to work around the file caching mechanism in the
  // Ubuntu. Once the image X specified by the filename Y is used in Ubuntu's
  // MPRIS, this pair will be cached. As long as the filename is same, even the
  // file content specified by Y is changed to Z, the image will stay unchanged.
  // The image shown in the Ubuntu's notification is still X instead of Z.
  // Changing the filename constantly works around this problem
  char filename[64];
  SprintfLiteral(filename, "%d_%d.%s", getpid(), gImageNumber++,
                 GetImageFileExtension(mMimeType.get()));

  rv = mLocalImageFile->Append(NS_ConvertUTF8toUTF16(filename));
  if (NS_FAILED(rv)) {
    LOG("Failed to create an image filename");
    return false;
  }

  rv = mLocalImageFile->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
  if (NS_FAILED(rv)) {
    LOG("Failed to create an image file");
    return false;
  }

  cleanup.release();
  return true;
}

bool MPRISServiceHandler::InitLocalImageFolder() {
  if (mLocalImageFolder && LocalImageFolderExists()) {
    return true;
  }

  nsresult rv = NS_GetSpecialDirectory(XRE_USER_APP_DATA_DIR,
                                       getter_AddRefs(mLocalImageFolder));
  if (NS_FAILED(rv) || !mLocalImageFolder) {
    LOG("Failed to get the image folder");
    return false;
  }

  auto cleanup =
      MakeScopeExit([this, self = RefPtr<MPRISServiceHandler>(this)] {
        mLocalImageFolder = nullptr;
      });

  rv = mLocalImageFolder->Append(u"firefox-mpris"_ns);
  if (NS_FAILED(rv)) {
    LOG("Failed to name an image folder");
    return false;
  }

  if (!LocalImageFolderExists()) {
    rv = mLocalImageFolder->Create(nsIFile::DIRECTORY_TYPE, 0700);
    if (NS_FAILED(rv)) {
      LOG("Failed to create an image folder");
      return false;
    }
  }

  cleanup.release();
  return true;
}

void MPRISServiceHandler::RemoveAllLocalImages() {
  if (!mLocalImageFolder || !LocalImageFolderExists()) {
    return;
  }

  nsresult rv = mLocalImageFolder->Remove(/* aRecursive */ true);
  if (NS_FAILED(rv)) {
    // It's ok to fail. The next removal is called when updating the
    // media-session image, or closing the MPRIS.
    LOG("Failed to remove images");
  }

  LOG("Abandon %s",
      mLocalImageFile ? mLocalImageFile->NativePath().get() : "nothing");
  mMPRISMetadata.mArtUrl = EmptyCString();
  mLocalImageFile = nullptr;
  mLocalImageFolder = nullptr;
}

bool MPRISServiceHandler::LocalImageFolderExists() {
  MOZ_ASSERT(mLocalImageFolder);

  bool exists;
  nsresult rv = mLocalImageFolder->Exists(&exists);
  return NS_SUCCEEDED(rv) && exists;
}

GVariant* MPRISServiceHandler::GetMetadataAsGVariant() const {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{sv}", "mpris:trackid",
                        g_variant_new("o", DBUS_MPRIS_TRACK_PATH));

  g_variant_builder_add(
      &builder, "{sv}", "xesam:title",
      g_variant_new_string(static_cast<const gchar*>(
          NS_ConvertUTF16toUTF8(mMPRISMetadata.mTitle).get())));

  g_variant_builder_add(
      &builder, "{sv}", "xesam:album",
      g_variant_new_string(static_cast<const gchar*>(
          NS_ConvertUTF16toUTF8(mMPRISMetadata.mAlbum).get())));

  GVariantBuilder artistBuilder;
  g_variant_builder_init(&artistBuilder, G_VARIANT_TYPE("as"));
  g_variant_builder_add(
      &artistBuilder, "s",
      static_cast<const gchar*>(
          NS_ConvertUTF16toUTF8(mMPRISMetadata.mArtist).get()));
  g_variant_builder_add(&builder, "{sv}", "xesam:artist",
                        g_variant_builder_end(&artistBuilder));

  if (!mMPRISMetadata.mArtUrl.IsEmpty()) {
    g_variant_builder_add(&builder, "{sv}", "mpris:artUrl",
                          g_variant_new_string(static_cast<const gchar*>(
                              mMPRISMetadata.mArtUrl.get())));
  }

  return g_variant_builder_end(&builder);
}

void MPRISServiceHandler::EmitEvent(mozilla::dom::MediaControlKey aKey) const {
  for (auto& listener : mListeners) {
    listener->OnActionPerformed(mozilla::dom::MediaControlAction(aKey));
  }
}

}  // namespace widget
}  // namespace mozilla
