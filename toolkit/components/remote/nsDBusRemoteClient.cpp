/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* vim:set ts=8 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDBusRemoteClient.h"
#include "RemoteUtils.h"
#include "mozilla/XREAppData.h"
#include "mozilla/Logging.h"
#include "mozilla/Base64.h"
#include "nsPrintfCString.h"
#include "mozilla/GUniquePtr.h"

#include <dlfcn.h>

#undef LOG
#ifdef MOZ_LOGGING
static mozilla::LazyLogModule sRemoteLm("nsDBusRemoteClient");
#  define LOG(str, ...) \
    MOZ_LOG(sRemoteLm, mozilla::LogLevel::Debug, (str, ##__VA_ARGS__))
#else
#  define LOG(...)
#endif

using namespace mozilla;

nsDBusRemoteClient::nsDBusRemoteClient() {
  LOG("nsDBusRemoteClient::nsDBusRemoteClient");
}

nsDBusRemoteClient::~nsDBusRemoteClient() {
  LOG("nsDBusRemoteClient::~nsDBusRemoteClient");
}

nsresult nsDBusRemoteClient::SendCommandLine(
    const char* aProgram, const char* aProfile, int32_t argc, char** argv,
    const char* aStartupToken, char** aResponse, bool* aWindowFound) {
  NS_ENSURE_TRUE(aProgram, NS_ERROR_INVALID_ARG);

  LOG("nsDBusRemoteClient::SendCommandLine");

  int commandLineLength;
  char* commandLine =
      ConstructCommandLine(argc, argv, aStartupToken, &commandLineLength);
  if (!commandLine) {
    LOG("  failed to create command line");
    return NS_ERROR_FAILURE;
  }

  nsresult rv =
      DoSendDBusCommandLine(aProgram, aProfile, commandLine, commandLineLength);
  free(commandLine);

  *aWindowFound = NS_SUCCEEDED(rv);

  LOG("DoSendDBusCommandLine %s", NS_SUCCEEDED(rv) ? "OK" : "FAILED");
  return rv;
}

bool nsDBusRemoteClient::GetRemoteDestinationName(const char* aProgram,
                                                  const char* aProfile,
                                                  nsCString& aDestinationName) {
  // We have a profile name - just create the destination.
  // D-Bus names can contain only [a-z][A-Z][0-9]_
  // characters so adjust the profile string properly.
  nsAutoCString profileName;
  nsresult rv = mozilla::Base64Encode(nsAutoCString(aProfile), profileName);
  NS_ENSURE_SUCCESS(rv, false);

  mozilla::XREAppData::SanitizeNameForDBus(profileName);

  aDestinationName =
      nsPrintfCString("org.mozilla.%s.%s", aProgram, profileName.get());
  if (aDestinationName.Length() > DBUS_MAXIMUM_NAME_LENGTH)
    aDestinationName.Truncate(DBUS_MAXIMUM_NAME_LENGTH);

  static auto sDBusValidateBusName = (bool (*)(const char*, DBusError*))dlsym(
      RTLD_DEFAULT, "dbus_validate_bus_name");
  if (!sDBusValidateBusName) {
    LOG("  failed to get dbus_validate_bus_name()");
    return false;
  }

  if (!sDBusValidateBusName(aDestinationName.get(), nullptr)) {
    // We don't have a valid busName yet - try to create a default one.
    aDestinationName =
        nsPrintfCString("org.mozilla.%s.%s", aProgram, "default");
    if (!sDBusValidateBusName(aDestinationName.get(), nullptr)) {
      // We failed completelly to get a valid bus name - just quit
      // to prevent crash at dbus_bus_request_name().
      LOG("  failed to validate profile DBus name");
      return false;
    }
  }

  return true;
}

nsresult nsDBusRemoteClient::DoSendDBusCommandLine(const char* aProgram,
                                                   const char* aProfile,
                                                   const char* aBuffer,
                                                   int aLength) {
  LOG("nsDBusRemoteClient::DoSendDBusCommandLine()");

  nsAutoCString appName(aProgram);
  mozilla::XREAppData::SanitizeNameForDBus(appName);

  nsAutoCString destinationName;
  if (!GetRemoteDestinationName(appName.get(), aProfile, destinationName)) {
    LOG("  failed to get remote destination name");
    return NS_ERROR_FAILURE;
  }

  nsAutoCString pathName;
  pathName = nsPrintfCString("/org/mozilla/%s/Remote", appName.get());

  static auto sDBusValidatePathName = (bool (*)(const char*, DBusError*))dlsym(
      RTLD_DEFAULT, "dbus_validate_path");
  if (!sDBusValidatePathName ||
      !sDBusValidatePathName(pathName.get(), nullptr)) {
    LOG("  failed to validate path name");
    return NS_ERROR_FAILURE;
  }

  nsAutoCString remoteInterfaceName;
  remoteInterfaceName = nsPrintfCString("org.mozilla.%s", appName.get());

  LOG("  DBus destination: %s\n", destinationName.get());
  LOG("  DBus path: %s\n", pathName.get());
  LOG("  DBus interface: %s\n", remoteInterfaceName.get());

  RefPtr<GDBusProxy> proxy = dont_AddRef(g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr,
      destinationName.get(), pathName.get(), remoteInterfaceName.get(), nullptr,
      nullptr));
  if (!proxy) {
    LOG("  failed to create DBus proxy");
    return NS_ERROR_FAILURE;
  }

  GUniquePtr<GError> error;
  RefPtr<GVariant> reply = dont_AddRef(g_dbus_proxy_call_sync(
      proxy, "OpenURL",
      g_variant_new_from_data(G_VARIANT_TYPE("(ay)"), aBuffer, aLength, true,
                              nullptr, nullptr),
      G_DBUS_CALL_FLAGS_NONE, -1, nullptr, getter_Transfers(error)));
#ifdef MOZ_LOGGING
  if (!reply) {
    LOG("  failed to OpenURL: %s", error->message);
  }
#endif

  return reply ? NS_OK : NS_ERROR_FAILURE;
}
