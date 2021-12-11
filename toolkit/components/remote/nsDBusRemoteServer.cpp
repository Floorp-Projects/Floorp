/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDBusRemoteServer.h"

#include "nsPIDOMWindow.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Base64.h"
#include "nsIWidget.h"
#include "nsAppShellCID.h"
#include "nsPrintfCString.h"

#include "nsCOMPtr.h"

#include "nsGTKToolkit.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <dlfcn.h>

static const char* introspect_template =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
    "1.0//EN\"\n"
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    " <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "   <method name=\"Introspect\">\n"
    "     <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
    "   </method>\n"
    " </interface>\n"
    " <interface name=\"org.mozilla.%s\">\n"
    "   <method name=\"OpenURL\">\n"
    "     <arg name=\"url\" direction=\"in\" type=\"ay\"/>\n"
    "   </method>\n"
    " </interface>\n"
    "</node>\n";

DBusHandlerResult nsDBusRemoteServer::Introspect(DBusMessage* msg) {
  DBusMessage* reply;

  reply = dbus_message_new_method_return(msg);
  if (!reply) return DBUS_HANDLER_RESULT_NEED_MEMORY;

  nsAutoCString introspect_xml;
  introspect_xml = nsPrintfCString(introspect_template, mAppName.get());

  const char* message = introspect_xml.get();
  dbus_message_append_args(reply, DBUS_TYPE_STRING, &message,
                           DBUS_TYPE_INVALID);

  dbus_connection_send(mConnection, reply, nullptr);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult nsDBusRemoteServer::OpenURL(DBusMessage* msg) {
  DBusMessage* reply = nullptr;
  const char* commandLine;
  int length;

  if (!dbus_message_get_args(msg, nullptr, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                             &commandLine, &length, DBUS_TYPE_INVALID) ||
      length == 0) {
    nsAutoCString errorMsg;
    errorMsg = nsPrintfCString("org.mozilla.%s.Error", mAppName.get());
    reply = dbus_message_new_error(msg, errorMsg.get(), "Wrong argument");
  } else {
    guint32 timestamp = gtk_get_current_event_time();
    if (timestamp == GDK_CURRENT_TIME) {
      timestamp = guint32(g_get_monotonic_time() / 1000);
    }
    HandleCommandLine(commandLine, timestamp);
    reply = dbus_message_new_method_return(msg);
  }

  dbus_connection_send(mConnection, reply, nullptr);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult nsDBusRemoteServer::HandleDBusMessage(
    DBusConnection* aConnection, DBusMessage* msg) {
  NS_ASSERTION(mConnection == aConnection, "Wrong D-Bus connection.");

  const char* method = dbus_message_get_member(msg);
  const char* iface = dbus_message_get_interface(msg);

  if ((strcmp("Introspect", method) == 0) &&
      (strcmp("org.freedesktop.DBus.Introspectable", iface) == 0)) {
    return Introspect(msg);
  }

  nsAutoCString ourInterfaceName;
  ourInterfaceName = nsPrintfCString("org.mozilla.%s", mAppName.get());

  if ((strcmp("OpenURL", method) == 0) &&
      (strcmp(ourInterfaceName.get(), iface) == 0)) {
    return OpenURL(msg);
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void nsDBusRemoteServer::UnregisterDBusInterface(DBusConnection* aConnection) {
  NS_ASSERTION(mConnection == aConnection, "Wrong D-Bus connection.");
  // Not implemented
}

static DBusHandlerResult message_handler(DBusConnection* conn, DBusMessage* msg,
                                         void* user_data) {
  auto interface = static_cast<nsDBusRemoteServer*>(user_data);
  return interface->HandleDBusMessage(conn, msg);
}

static void unregister(DBusConnection* conn, void* user_data) {
  auto interface = static_cast<nsDBusRemoteServer*>(user_data);
  interface->UnregisterDBusInterface(conn);
}

static DBusObjectPathVTable remoteHandlersTable = {
    .unregister_function = unregister,
    .message_function = message_handler,
};

nsresult nsDBusRemoteServer::Startup(const char* aAppName,
                                     const char* aProfileName) {
  if (mConnection && dbus_connection_get_is_connected(mConnection)) {
    // We're already connected so we don't need to reconnect
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // Don't even try to start without any application/profile name
  if (!aAppName || aAppName[0] == '\0' || !aProfileName ||
      aProfileName[0] == '\0')
    return NS_ERROR_INVALID_ARG;

  mConnection =
      already_AddRefed<DBusConnection>(dbus_bus_get(DBUS_BUS_SESSION, nullptr));
  if (!mConnection) {
    return NS_ERROR_FAILURE;
  }
  dbus_connection_set_exit_on_disconnect(mConnection, false);
  dbus_connection_setup_with_g_main(mConnection, nullptr);

  mAppName = aAppName;
  ToLowerCase(mAppName);

  // D-Bus names can contain only [a-z][A-Z][0-9]_
  // characters so adjust the profile string properly.
  nsAutoCString profileName;
  nsresult rv =
      mozilla::Base64Encode(aProfileName, strlen(aProfileName), profileName);
  NS_ENSURE_SUCCESS(rv, rv);

  profileName.ReplaceChar("+/=-", '_');
  mAppName.ReplaceChar("+/=-", '_');

  nsAutoCString busName;
  busName =
      nsPrintfCString("org.mozilla.%s.%s", mAppName.get(), profileName.get());
  if (busName.Length() > DBUS_MAXIMUM_NAME_LENGTH)
    busName.Truncate(DBUS_MAXIMUM_NAME_LENGTH);

  static auto sDBusValidateBusName = (bool (*)(const char*, DBusError*))dlsym(
      RTLD_DEFAULT, "dbus_validate_bus_name");
  if (!sDBusValidateBusName) {
    return NS_ERROR_FAILURE;
  }

  // We don't have a valid busName yet - try to create a default one.
  if (!sDBusValidateBusName(busName.get(), nullptr)) {
    busName = nsPrintfCString("org.mozilla.%s.%s", mAppName.get(), "default");
    if (!sDBusValidateBusName(busName.get(), nullptr)) {
      // We failed completelly to get a valid bus name - just quit
      // to prevent crash at dbus_bus_request_name().
      return NS_ERROR_FAILURE;
    }
  }

  DBusError err;
  dbus_error_init(&err);
  dbus_bus_request_name(mConnection, busName.get(), DBUS_NAME_FLAG_DO_NOT_QUEUE,
                        &err);
  // The interface is already owned - there is another application/profile
  // instance already running.
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    mConnection = nullptr;
    return NS_ERROR_FAILURE;
  }

  mPathName = nsPrintfCString("/org/mozilla/%s/Remote", mAppName.get());
  static auto sDBusValidatePathName = (bool (*)(const char*, DBusError*))dlsym(
      RTLD_DEFAULT, "dbus_validate_path");
  if (!sDBusValidatePathName ||
      !sDBusValidatePathName(mPathName.get(), nullptr)) {
    mConnection = nullptr;
    return NS_ERROR_FAILURE;
  }
  if (!dbus_connection_register_object_path(mConnection, mPathName.get(),
                                            &remoteHandlersTable, this)) {
    mConnection = nullptr;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void nsDBusRemoteServer::Shutdown() {
  if (!mConnection) {
    return;
  }

  dbus_connection_unregister_object_path(mConnection, mPathName.get());

  // dbus_connection_unref() will be called by RefPtr here.
  mConnection = nullptr;
}
