/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* vim:set ts=8 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DBusRemoteClient.h"
#include "RemoteUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Base64.h"
#include "nsPrintfCString.h"

using mozilla::LogLevel;
static mozilla::LazyLogModule sRemoteLm("DBusRemoteClient");

DBusRemoteClient::DBusRemoteClient()
{
  mConnection = nullptr;
  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("DBusRemoteClient::DBusRemoteClient"));
}

DBusRemoteClient::~DBusRemoteClient()
{
  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("DBusRemoteClient::~DBusRemoteClient"));
  Shutdown();
}

nsresult
DBusRemoteClient::Init()
{
  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("DBusRemoteClient::Init"));

  if (mConnection)
    return NS_OK;

  mConnection = already_AddRefed<DBusConnection>(
    dbus_bus_get(DBUS_BUS_SESSION, nullptr));
  if (!mConnection)
    return NS_ERROR_FAILURE;

  dbus_connection_set_exit_on_disconnect(mConnection, false);

  return NS_OK;
}

void
DBusRemoteClient::Shutdown (void)
{
  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("DBusRemoteClient::Shutdown"));
  // This connection is owned by libdbus and we don't need to close it
  mConnection = nullptr;
}

nsresult
DBusRemoteClient::SendCommandLine (const char *aProgram, const char *aUsername,
                                const char *aProfile,
                                int32_t argc, char **argv,
                                const char* aDesktopStartupID,
                                char **aResponse, bool *aWindowFound)
{
  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("DBusRemoteClient::SendCommandLine"));

  int commandLineLength;
  char *commandLine = ConstructCommandLine(argc, argv, aDesktopStartupID,
                                           &commandLineLength);
  if (!commandLine)
    return NS_ERROR_FAILURE;

  nsresult rv = DoSendDBusCommandLine(aProgram, aProfile,
                                      commandLine, commandLineLength);
  free(commandLine);
  *aWindowFound = NS_SUCCEEDED(rv);

  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("DoSendDBusCommandLine returning 0x%" PRIx32 "\n",
                                       static_cast<uint32_t>(rv)));
  return rv;
}

bool
DBusRemoteClient::GetRemoteDestinationName(const char *aProgram,
                                           const char *aProfile,
                                           nsCString &aDestinationName)
{
  if(!aProfile || aProfile[0] == '\0') {
    // We don't have a profile name - search for active mozilla instances.
    RefPtr<DBusMessage> msg = already_AddRefed<DBusMessage>(
        dbus_message_new_method_call("org.freedesktop.DBus",
                                     "/org/freedesktop/DBus",
                                     "org.freedesktop.DBus",
                                     "ListNames"));
    if (!msg) {
      return false;
    }

    // send message and get a handle for a reply
    RefPtr<DBusMessage> reply = already_AddRefed<DBusMessage>(
        dbus_connection_send_with_reply_and_block(mConnection, msg, -1, nullptr));
    if (!reply) {
      return false;
    }

    char **interfaces;
    dbus_int32_t interfaceNums;
    if (!dbus_message_get_args(reply, nullptr, DBUS_TYPE_ARRAY,
                               DBUS_TYPE_STRING, &interfaces, &interfaceNums,
                               DBUS_TYPE_INVALID)) {
      return false;
    }

    nsAutoCString destinationTemplate;
    destinationTemplate = nsPrintfCString("org.mozilla.%s", aProgram);

    aDestinationName.SetLength(0);
    for (int i = 0; i < interfaceNums; i++) {
      if (strstr(interfaces[i], destinationTemplate.get())) {
        aDestinationName = interfaces[i];
        break;
      }
    }
    dbus_free_string_array(interfaces);

    return (!aDestinationName.IsEmpty());
  } else {
    // We have a profile name - just create the destination.
    // D-Bus names can contain only [a-z][A-Z][0-9]_
    // characters so adjust the profile string properly.
    nsAutoCString profileName;
    nsresult rv = mozilla::Base64Encode(nsAutoCString(aProfile), profileName);
    NS_ENSURE_SUCCESS(rv, false);
    profileName.ReplaceChar("+/=", '_');

    aDestinationName = nsPrintfCString("org.mozilla.%s.%s", aProgram,
                                                            profileName.get());
    if (aDestinationName.Length() > DBUS_MAXIMUM_NAME_LENGTH)
      aDestinationName.Truncate(DBUS_MAXIMUM_NAME_LENGTH);

    if (!dbus_validate_bus_name(aDestinationName.get(), nullptr)) {
      // We don't have a valid busName yet - try to create a default one.
      aDestinationName = nsPrintfCString("org.mozilla.%s.%s", aProgram,
                                                             "default");
      if (!dbus_validate_bus_name(aDestinationName.get(), nullptr)) {
        // We failed completelly to get a valid bus name - just quit
        // to prevent crash at dbus_bus_request_name().
        return false;
      }
    }

    return true;
  }
}

nsresult
DBusRemoteClient::DoSendDBusCommandLine(const char *aProgram, const char *aProfile,
                                        const char* aBuffer, int aLength)
{
  nsAutoCString destinationName;
  if (!GetRemoteDestinationName(aProgram, aProfile, destinationName))
    return NS_ERROR_FAILURE;

  nsAutoCString pathName;
  pathName = nsPrintfCString("/org/mozilla/%s/Remote", aProgram);

  nsAutoCString remoteInterfaceName;
  remoteInterfaceName = nsPrintfCString("org.mozilla.%s", aProgram);

  RefPtr<DBusMessage> msg = already_AddRefed<DBusMessage>(
      dbus_message_new_method_call(destinationName.get(),
                                   pathName.get(), // object to call on
                                   remoteInterfaceName.get(), // interface to call on
                                   "OpenURL")); // method name
  if (!msg) {
    return NS_ERROR_FAILURE;
  }

  // append arguments
  if (!dbus_message_append_args(msg, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                                &aBuffer, aLength, DBUS_TYPE_INVALID)) {
    return NS_ERROR_FAILURE;
  }

  // send message and get a handle for a reply
  DBusError err;
  dbus_error_init(&err);
  RefPtr<DBusMessage> reply = already_AddRefed<DBusMessage>(
      dbus_connection_send_with_reply_and_block(mConnection, msg, -1, &err));

  if (dbus_error_is_set(&err)) {
      dbus_error_free(&err);
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
