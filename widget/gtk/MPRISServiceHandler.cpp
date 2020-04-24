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
#include "mozilla/Sprintf.h"
#include "nsIXULAppInfo.h"

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MPRISServiceHandler=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace widget {

enum class Method : uint8_t {
  eQuit,
  eRaise,
  eNext,
  ePrevious,
  ePause,
  ePlayPause,
  eStop,
  ePlay,
  eSeek,
  eSetPosition,
  eOpenUri,
  eUnknown
};

static inline Method GetMethod(const gchar* aMethodName) {
  const std::unordered_map<std::string, Method> map = {
      {"Quit", Method::eQuit},      {"Raise", Method::eRaise},
      {"Next", Method::eNext},      {"Previous", Method::ePrevious},
      {"Pause", Method::ePause},    {"PlayPause", Method::ePlayPause},
      {"Stop", Method::eStop},      {"Play", Method::ePlay},
      {"Seek", Method::eSeek},      {"SetPosition", Method::eSetPosition},
      {"OpenUri", Method::eOpenUri}};

  auto it = map.find(aMethodName);
  return (it == map.end() ? Method::eUnknown : it->second);
}

static void HandleMethodCall(GDBusConnection* aConnection, const gchar* aSender,
                             const gchar* aObjectPath,
                             const gchar* aInterfaceName,
                             const gchar* aMethodName, GVariant* aParameters,
                             GDBusMethodInvocation* aInvocation,
                             gpointer aUserData) {
  MOZ_ASSERT(aUserData);
  MOZ_ASSERT(NS_IsMainThread());
  MPRISServiceHandler* handler = static_cast<MPRISServiceHandler*>(aUserData);
  std::string error;

  switch (GetMethod(aMethodName)) {
    case Method::eUnknown:
      g_dbus_method_invocation_return_error(
          aInvocation, G_IO_ERROR, G_IO_ERROR_FAILED, "Invalid Method");
      return;
    case Method::eQuit:
      if (handler->CanQuit()) {
        handler->Quit();
      } else {
        error = "Cannot invoke Quit() when CanQuit() returns false";
      }
      break;
    case Method::eRaise:
      if (handler->CanRaise()) {
        handler->Raise();
      } else {
        error = "Cannot invoke Raise() when CanRaise() returns false";
      }
      break;
    case Method::eNext:
      if (handler->CanGoNext()) {
        handler->Next();
      } else {
        error = "Cannot invoke Next() when CanGoNext() returns false";
      }
      break;
    case Method::ePrevious:
      if (handler->CanGoPrevious()) {
        handler->Previous();
      } else {
        error = "Cannot invoke Previous() when CanGoPrevious() returns false";
      }
      break;
    case Method::ePause:
      if (handler->CanPause()) {
        handler->Pause();
      } else {
        error = "Cannot invoke Pause() when CanPause() returns false";
      }
      break;
    case Method::ePlayPause:
      // According to Spec this should only fail if canPause is false, but Play
      // may be forbidden due to CanPlay. This means in theory even though
      // CanPlay is false, this method would be able to Play something which
      // means when CanPause is false, CanPlay _has to be_ false as well.
      if (handler->CanPlay() && handler->CanPause()) {
        handler->PlayPause();
      } else {
        error =
            "Cannot invoke PlayPause() when either CanPlay() or CanPause() "
            "returns false";
      }
      break;
    case Method::eStop:
      handler->Stop();  // Stop is mandatory
      break;
    case Method::ePlay:
      if (handler->CanPlay()) {
        handler->Play();
      } else {
        error = "Cannot invoke Play() when CanPlay() returns false";
      }
      break;
    case Method::eSeek:
      if (handler->CanSeek()) {
        gint64 position;
        g_variant_get(aParameters, "(x)", &position);
        handler->Seek(position);
      } else {
        error = "Cannot invoke Seek() when CanSeek() returns false";
      }
      break;
    case Method::eSetPosition:
      if (handler->CanSeek()) {
        gchar* trackId;
        gint64 position;
        g_variant_get(aParameters, "(ox)", &trackId, &position);
        handler->SetPosition(trackId, position);
      } else {
        error = "Cannot invoke SetPosition() when CanSeek() returns false";
      }
      break;
    case Method::eOpenUri:
      gchar* uri;
      g_variant_get(aParameters, "(s)", &uri);
      if (!handler->OpenUri(uri)) {
        error = "Could not open URI";
      }
      break;
  }

  if (!error.empty()) {
    g_dbus_method_invocation_return_error(
        aInvocation, G_IO_ERROR, G_IO_ERROR_READ_ONLY, "%s", error.c_str());
  }
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
  eGetVolume,
  eGetPosition,
  eGetMinimumRate,
  eGetMaximumRate,
  eGetRate,
  eGetPlaybackStatus,
  eGetMetadata,
  eUnknown
};

static inline Property GetProperty(const gchar* aPropertyName) {
  const std::unordered_map<std::string, Property> map = {
      {"Identity", Property::eIdentity},
      {"HasTrackList", Property::eHasTrackList},
      {"CanRaise", Property::eCanRaise},
      {"CanQuit", Property::eCanQuit},
      {"SupportedUriSchemes", Property::eSupportedUriSchemes},
      {"SupportedMimeTypes", Property::eSupportedMimeTypes},
      {"CanGoNext", Property::eCanGoNext},
      {"CanGoPrevious", Property::eCanGoPrevious},
      {"CanPlay", Property::eCanPlay},
      {"CanPause", Property::eCanPause},
      {"CanSeek", Property::eCanSeek},
      {"CanControl", Property::eCanControl},
      {"Volume", Property::eGetVolume},
      {"Position", Property::eGetPosition},
      {"MinimumRate", Property::eGetMinimumRate},
      {"MaximumRate", Property::eGetMaximumRate},
      {"Rate", Property::eGetRate},
      {"PlaybackStatus", Property::eGetPlaybackStatus},
      {"Metadata", Property::eGetMetadata}};

  auto it = map.find(aPropertyName);
  return (it == map.end() ? Property::eUnknown : it->second);
}

static GVariant* HandleGetProperty(GDBusConnection* aConnection,
                                   const gchar* aSender,
                                   const gchar* aObjectPath,
                                   const gchar* aInterfaceName,
                                   const gchar* aPropertyName, GError** aError,
                                   gpointer aUserData) {
  MOZ_ASSERT(aUserData);
  MOZ_ASSERT(NS_IsMainThread());
  MPRISServiceHandler* handler = static_cast<MPRISServiceHandler*>(aUserData);

  switch (GetProperty(aPropertyName)) {
    case Property::eUnknown:
      g_set_error(aError, G_IO_ERROR, G_IO_ERROR_FAILED, "Unknown Property");
      return nullptr;
    case Property::eIdentity:
      return g_variant_new_string(handler->Identity());
    case Property::eHasTrackList:
      return g_variant_new_boolean(handler->HasTrackList());
    case Property::eCanRaise:
      return g_variant_new_boolean(handler->CanRaise());
    case Property::eCanQuit:
      return g_variant_new_boolean(handler->CanQuit());
    case Property::eSupportedUriSchemes:
      return handler->SupportedUriSchemes();
    case Property::eSupportedMimeTypes:
      return handler->SupportedMimeTypes();
    case Property::eCanGoNext:
      return g_variant_new_boolean(handler->CanGoNext());
    case Property::eCanGoPrevious:
      return g_variant_new_boolean(handler->CanGoPrevious());
    case Property::eCanPlay:
      return g_variant_new_boolean(handler->CanPlay());
    case Property::eCanPause:
      return g_variant_new_boolean(handler->CanPause());
    case Property::eCanSeek:
      return g_variant_new_boolean(handler->CanSeek());
    case Property::eCanControl:
      return g_variant_new_boolean(handler->CanControl());
    case Property::eGetVolume:
      return g_variant_new_double(handler->GetVolume());
    case Property::eGetPosition:
      return g_variant_new_int64(handler->GetPosition());
    case Property::eGetMinimumRate:
      return g_variant_new_double(handler->GetMinimumRate());
    case Property::eGetMaximumRate:
      return g_variant_new_double(handler->GetMaximumRate());
    case Property::eGetRate:
      return g_variant_new_double(handler->GetRate());
    case Property::eGetPlaybackStatus:
      if (GVariant* state = handler->GetPlaybackStatus()) {
        return state;
      }
      g_set_error(aError, G_IO_ERROR, G_IO_ERROR_FAILED,
                  "Invalid Playback Status");
      return nullptr;
    case Property::eGetMetadata:
      return handler->GetMetadataAsGVariant();
  }

  MOZ_ASSERT_UNREACHABLE("Switch Statement incomplete");
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
  MPRISServiceHandler* handler = static_cast<MPRISServiceHandler*>(aUserData);

  if (g_strcmp0(aPropertyName, "Volume") == 0) {
    if (!handler->SetVolume(g_variant_get_double(aValue))) {
      g_set_error(aError, G_IO_ERROR, G_IO_ERROR_FAILED,
                  "Could not set the Volume");
      return false;
    }
  } else if (g_strcmp0(aPropertyName, "Rate") == 0) {
    if (!handler->SetRate(g_variant_get_double(aValue))) {
      g_set_error(aError, G_IO_ERROR, G_IO_ERROR_FAILED,
                  "Could not set the Rate");
      return false;
    }
  } else {
    g_set_error(aError, G_IO_ERROR, G_IO_ERROR_FAILED, "Unknown Property");
    return false;
  }

  GVariantBuilder
      propertiesBuilder;  // a builder for the list of changed properties
  g_variant_builder_init(&propertiesBuilder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&propertiesBuilder, "{sv}", aPropertyName, aValue);

  return g_dbus_connection_emit_signal(
      aConnection, nullptr, aObjectPath, "org.freedesktop.DBus.Properties",
      "PropertiesChanged",
      g_variant_new("(sa{sv}as)", aInterfaceName, &propertiesBuilder, nullptr),
      aError);
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
  MediaControlKeysEventSource::Close();
}

bool MPRISServiceHandler::IsOpened() const { return mInitialized; }

bool MPRISServiceHandler::HasTrackList() { return false; }

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

GVariant* MPRISServiceHandler::SupportedUriSchemes() {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
  return g_variant_builder_end(&builder);
}

GVariant* MPRISServiceHandler::SupportedMimeTypes() {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
  return g_variant_builder_end(&builder);
}

constexpr bool MPRISServiceHandler::CanRaise() { return true; }

constexpr bool MPRISServiceHandler::CanQuit() { return false; }

void MPRISServiceHandler::Quit() {
  MOZ_ASSERT_UNREACHABLE("CanQuit is false, this method is not implemented");
}

bool MPRISServiceHandler::CanGoNext() const { return true; }

bool MPRISServiceHandler::CanGoPrevious() const { return true; }

bool MPRISServiceHandler::CanPlay() const { return true; }

bool MPRISServiceHandler::CanPause() const { return true; }

// We don't support Seeking or Setting/Getting the Position yet
bool MPRISServiceHandler::CanSeek() const { return false; }

bool MPRISServiceHandler::CanControl() const {
  return true;  // we don't support LoopStatus, Shuffle, Rate or Volume, but at
                // least KDE blocks Play/Pause when CanControl is false.
}

// We don't support getting the volume (yet) so return a dummy value.
double MPRISServiceHandler::GetVolume() const { return 1.0f; }

// we don't support setting the volume yet, so this is a no-op
bool MPRISServiceHandler::SetVolume(double aVolume) {
  if (aVolume > 1.0f || aVolume < 0.0f) {
    return false;
  }
  LOG("Volume set to %f", aVolume);
  return true;
}
int64_t MPRISServiceHandler::GetPosition() const { return 0; }

constexpr double MPRISServiceHandler::GetMinimumRate() { return 1.0f; }

constexpr double MPRISServiceHandler::GetMaximumRate() { return 1.0f; }

// Getting and Setting the Rate doesn't work yet, so it will be locked to 1.0
double MPRISServiceHandler::GetRate() const { return 1.0f; }

bool MPRISServiceHandler::SetRate(double aRate) {
  if (aRate > GetMaximumRate() || aRate < GetMinimumRate()) {
    return false;
  }

  LOG("Set Playback Rate to %f", aRate);
  return true;
}

void MPRISServiceHandler::SetPlaybackState(
    dom::MediaSessionPlaybackState aState) {
  LOG("SetPlaybackState");
  if (mPlaybackState == aState) {
    return;
  }

  MediaControlKeysEventSource::SetPlaybackState(aState);

  if (!mConnection) {
    return;  // No D-Bus Connection, no event
  }

  GVariant* state = GetPlaybackStatus();
  if (!state) {
    return;  // Invalid state
  }

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
  mMetadata = Some(aMetadata);
  LOG("Set MediaMetadata: title - %s, Artist - %s, Album - %s",
      NS_ConvertUTF16toUTF8(mMetadata->mTitle).get(),
      NS_ConvertUTF16toUTF8(mMetadata->mArtist).get(),
      NS_ConvertUTF16toUTF8(mMetadata->mAlbum).get());

  if (!mConnection) {
    LOG("No D-Bus Connection. Drop the update.");
    return;
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
  }
}

GVariant* MPRISServiceHandler::GetMetadataAsGVariant() const {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{sv}", "mpris:trackid",
                        g_variant_new("o", DBUS_MPRIS_TRACK_PATH));

  if (mMetadata.isSome()) {
    LOG("Get Metadata: title - %s, Artist - %s, Album - %s",
        NS_ConvertUTF16toUTF8(mMetadata->mTitle).get(),
        NS_ConvertUTF16toUTF8(mMetadata->mArtist).get(),
        NS_ConvertUTF16toUTF8(mMetadata->mAlbum).get());

    g_variant_builder_add(
        &builder, "{sv}", "xesam:title",
        g_variant_new_string(NS_ConvertUTF16toUTF8(mMetadata->mTitle).get()));
    GVariantBuilder artistBuilder;  // Artists is a list.
    g_variant_builder_init(&artistBuilder, G_VARIANT_TYPE("as"));
    g_variant_builder_add(&artistBuilder, "s",
                          NS_ConvertUTF16toUTF8(mMetadata->mArtist).get());
    g_variant_builder_add(&builder, "{sv}", "xesam:artist",
                          g_variant_builder_end(&artistBuilder));
  }
  return g_variant_builder_end(&builder);
}

void MPRISServiceHandler::EmitEvent(mozilla::dom::MediaControlKeysEvent event) {
  for (auto& listener : mListeners) {
    listener->OnKeyPressed(event);
  }
}

void MPRISServiceHandler::Raise() {
  LOG("Raise");
  EmitEvent(mozilla::dom::MediaControlKeysEvent::eFocus);
}

void MPRISServiceHandler::Next() {
  LOG("Next");
  EmitEvent(mozilla::dom::MediaControlKeysEvent::eNextTrack);
}

void MPRISServiceHandler::Previous() {
  LOG("Previous");
  EmitEvent(mozilla::dom::MediaControlKeysEvent::ePrevTrack);
}

void MPRISServiceHandler::Pause() {
  LOG("Pause");
  EmitEvent(mozilla::dom::MediaControlKeysEvent::ePause);
}

void MPRISServiceHandler::PlayPause() {
  LOG("PlayPause");
  EmitEvent(mozilla::dom::MediaControlKeysEvent::ePlayPause);
}

void MPRISServiceHandler::Stop() {
  LOG("Stop");
  EmitEvent(mozilla::dom::MediaControlKeysEvent::eStop);
}

void MPRISServiceHandler::Play() {
  LOG("Play");
  EmitEvent(mozilla::dom::MediaControlKeysEvent::ePlay);
}

// Caution, Seek can really be negative, like -1000000 during testing
void MPRISServiceHandler::Seek(int64_t aOffset) {
  LOG("Seek(%" PRId64 ")", aOffset);
}

// The following two methods are untested as my Desktop Widget didn't issue
// these calls.
void MPRISServiceHandler::SetPosition(char* aTrackId, int64_t aPosition) {
  LOG("SetPosition(%s, %" PRId64 ")", aTrackId, aPosition);
}

bool MPRISServiceHandler::OpenUri(char* aUri) {
  LOG("OpenUri(%s)", aUri);
  return false;
}

}  // namespace widget
}  // namespace mozilla
