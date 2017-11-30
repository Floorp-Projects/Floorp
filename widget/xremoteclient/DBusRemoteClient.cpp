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

nsresult
DBusRemoteClient::DoSendDBusCommandLine(const char *aProgram, const char *aProfile,
                                        const char* aBuffer, int aLength)
{
  if(!aProfile || aProfile[0] == '\0') {
    return NS_ERROR_INVALID_ARG;
  }

  // D-Bus names can contain only [a-z][A-Z][0-9]_
  // characters so adjust the profile string properly.
  nsAutoCString profileName;
  nsresult rv = mozilla::Base64Encode(nsAutoCString(aProfileName), profileName);
  NS_ENSURE_SUCCESS(rv, rv);
  profileName.ReplaceChar("+/=", '_');

  nsAutoCString destinationName;
  destinationName = nsPrintfCString("org.mozilla.%s.%s", aProgram, profileName.get());

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

