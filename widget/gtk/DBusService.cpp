/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <gdk/gdk.h>
#include "DBusService.h"
#include "nsAppRunner.h"
#include "mozilla/Unused.h"
#include "mozilla/GUniquePtr.h"
#include "mozilla/WidgetUtils.h"
#include <gio/gio.h>
#include "nsIObserverService.h"
#include "WidgetUtilsGtk.h"
#include "prproces.h"
#include "mozilla/XREAppData.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::widget;

DBusService::~DBusService() { StopFreedesktopListener(); }

bool DBusService::Init() { return StartFreedesktopListener(); }

void DBusService::Run() {
  GMainLoop* loop = g_main_loop_new(nullptr, false);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

// Mozilla has old GIO version in build roots
#define G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE GBusNameOwnerFlags(1 << 2)

#define DBUS_BUS_NAME_TEMPLATE "org.mozilla.%s"
#define DBUS_OBJECT_PATH_TEMPLATE "/org/mozilla/%s"

static const char* GetDBusBusName() {
  static const char* name = []() {
    nsAutoCString appName;
    gAppData->GetDBusAppName(appName);
    return ToNewCString(nsPrintfCString(DBUS_BUS_NAME_TEMPLATE,
                                        appName.get()));  // Intentionally leak
  }();
  return name;
}

static const char* GetDBusObjectPath() {
  static const char* path = []() {
    nsAutoCString appName;
    gAppData->GetDBusAppName(appName);
    return ToNewCString(nsPrintfCString(DBUS_OBJECT_PATH_TEMPLATE,
                                        appName.get()));  // Intentionally leak
  }();
  return path;
}

// See
// https://specifications.freedesktop.org/desktop-entry-spec/1.1/ar01s07.html
// for details
static const char* kIntrospectTemplate =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
    "1.0//EN\"\n"
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "<interface name='org.freedesktop.Application'>\n"
    "<method name='Activate'>\n"
    "  <arg type='a{sv}' name='platform_data' direction='in'/>\n"
    "  </method>\n"
    "  <method name='Open'>\n"
    "  <arg type='as' name='uris' direction='in'/>\n"
    "  <arg type='a{sv}' name='platform_data' direction='in'/>\n"
    "</method>\n"
    "<method name='ActivateAction'>\n"
    "  <arg type='s' name='action_name' direction='in'/>\n"
    "  <arg type='av' name='parameter' direction='in'/>\n"
    "  <arg type='a{sv}' name='platform_data' direction='in'/>\n"
    "</method>\n"
    "</interface>\n"
    "</node>\n";

bool DBusService::LaunchApp(const char* aCommand, const char** aURIList,
                            int aURIListLen) {
  nsAutoCString param(mAppFile);
  if (aCommand) {
    param.Append(" ");
    param.Append(aCommand);
  }
  for (int i = 0; aURIList && i < aURIListLen; i++) {
    param.Append(" ");
    GUniquePtr<char> escUri(g_shell_quote(aURIList[i]));
    param.Append(escUri.get());
  }

  char* argv[] = {strdup("/bin/sh"), strdup("-c"), strdup(param.get()),
                  nullptr};
  int ret =
      PR_CreateProcessDetached("/bin/sh", argv, nullptr, nullptr) != PR_FAILURE;
  for (auto str : argv) {
    free(str);
  }
  return ret;
}

// The Activate method is called when the application is started without
// files to open.
// Open :: (a{sv}) → ()
void DBusService::HandleFreedesktopActivate(GVariant* aParameters,
                                            GDBusMethodInvocation* aReply) {
  if (!LaunchApp(nullptr, nullptr, 0)) {
    g_dbus_method_invocation_return_error(aReply, G_DBUS_ERROR,
                                          G_DBUS_ERROR_FAILED,
                                          "Failed to run target application.");
    return;
  }
  g_dbus_method_invocation_return_value(aReply, nullptr);
}

// The Open method is called when the application is started with files.
// The array of strings is an array of URIs, in UTF-8.
// Open :: (as,a{sv}) → ()
void DBusService::HandleFreedesktopOpen(GVariant* aParameters,
                                        GDBusMethodInvocation* aReply) {
  RefPtr<GVariant> variant =
      dont_AddRef(g_variant_get_child_value(aParameters, 0));
  gsize uriNum = 0;
  GUniquePtr<const char*> uriArray(g_variant_get_strv(variant, &uriNum));
  if (!LaunchApp(nullptr, uriArray.get(), uriNum)) {
    g_dbus_method_invocation_return_error(aReply, G_DBUS_ERROR,
                                          G_DBUS_ERROR_FAILED,
                                          "Failed to run target application.");
    return;
  }
  g_dbus_method_invocation_return_value(aReply, nullptr);
}

// The ActivateAction method is called when Desktop Actions are activated.
// The action-name parameter is the name of the action.
// ActivateAction :: (s,av,a{sv}) → ()
void DBusService::HandleFreedesktopActivateAction(
    GVariant* aParameters, GDBusMethodInvocation* aReply) {
  const char* actionName;

  // aParameters is "(s,av,a{sv})" type
  RefPtr<GVariant> r = dont_AddRef(g_variant_get_child_value(aParameters, 0));
  if (!(actionName = g_variant_get_string(r, nullptr))) {
    g_dbus_method_invocation_return_error(
        aReply, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Wrong params!");
    return;
  }

  // TODO: Read av params and pass them to LaunchApp?

  // actionName matches desktop action defined in .desktop file.
  // We implement it for .desktop file shipped by flatpak
  // (taskcluster/docker/firefox-flatpak/org.mozilla.firefox.desktop)
  bool ret = false;
  if (!strcmp(actionName, "new-window")) {
    ret = LaunchApp(nullptr, nullptr, 0);
  } else if (!strcmp(actionName, "new-private-window")) {
    ret = LaunchApp("--private-window", nullptr, 0);
  } else if (!strcmp(actionName, "profile-manager-window")) {
    ret = LaunchApp("--ProfileManager", nullptr, 0);
  }
  if (!ret) {
    g_dbus_method_invocation_return_error(aReply, G_DBUS_ERROR,
                                          G_DBUS_ERROR_FAILED,
                                          "Failed to run target application.");
    return;
  }
  g_dbus_method_invocation_return_value(aReply, nullptr);
}

static void HandleMethodCall(GDBusConnection* aConnection, const gchar* aSender,
                             const gchar* aObjectPath,
                             const gchar* aInterfaceName,
                             const gchar* aMethodName, GVariant* aParameters,
                             GDBusMethodInvocation* aInvocation,
                             gpointer aUserData) {
  MOZ_ASSERT(aUserData);
  MOZ_ASSERT(NS_IsMainThread());

  if (strcmp("org.freedesktop.Application", aInterfaceName) != 0) {
    g_warning("DBusService: HandleMethodCall() wrong interface name %s",
              aInterfaceName);
    return;
  }
  if (strcmp("Activate", aMethodName) == 0) {
    static_cast<DBusService*>(aUserData)->HandleFreedesktopActivate(
        aParameters, aInvocation);
  } else if (strcmp("Open", aMethodName) == 0) {
    static_cast<DBusService*>(aUserData)->HandleFreedesktopOpen(aParameters,
                                                                aInvocation);
  } else if (strcmp("ActivateAction", aMethodName) == 0) {
    static_cast<DBusService*>(aUserData)->HandleFreedesktopActivateAction(
        aParameters, aInvocation);
  } else {
    g_warning("DBusService: HandleMethodCall() wrong method %s", aMethodName);
  }
}

static GVariant* HandleGetProperty(GDBusConnection* aConnection,
                                   const gchar* aSender,
                                   const gchar* aObjectPath,
                                   const gchar* aInterfaceName,
                                   const gchar* aPropertyName, GError** aError,
                                   gpointer aUserData) {
  MOZ_ASSERT(aUserData);
  MOZ_ASSERT(NS_IsMainThread());
  g_set_error(aError, G_IO_ERROR, G_IO_ERROR_FAILED,
              "%s:%s setting is not supported", aInterfaceName, aPropertyName);
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

void DBusService::OnBusAcquired(GDBusConnection* aConnection) {
  GUniquePtr<GError> error;
  mIntrospectionData = dont_AddRef(g_dbus_node_info_new_for_xml(
      kIntrospectTemplate, getter_Transfers(error)));
  if (!mIntrospectionData) {
    g_warning("DBusService: g_dbus_node_info_new_for_xml() failed! %s",
              error->message);
    return;
  }

  mRegistrationId = g_dbus_connection_register_object(
      aConnection, GetDBusObjectPath(), mIntrospectionData->interfaces[0],
      &gInterfaceVTable, this,  /* user_data */
      nullptr,                  /* user_data_free_func */
      getter_Transfers(error)); /* GError** */

  if (mRegistrationId == 0) {
    g_warning(
        "DBusService: g_dbus_connection_register_object() "
        "failed! %s",
        error->message);
    return;
  }
}

void DBusService::OnNameAcquired(GDBusConnection* aConnection) {
  mConnection = aConnection;
}

void DBusService::OnNameLost(GDBusConnection* aConnection) {
  mConnection = nullptr;
  if (!mRegistrationId) {
    return;
  }
  if (g_dbus_connection_unregister_object(aConnection, mRegistrationId)) {
    mRegistrationId = 0;
  }
}

bool DBusService::StartFreedesktopListener() {
  if (mDBusID) {
    // We're already connected so we don't need to reconnect
    return false;
  }

  mDBusID = g_bus_own_name(
      // if org.mozilla.Firefox is taken it means we're already running
      // so use G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE and quit.
      G_BUS_TYPE_SESSION, GetDBusBusName(), G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE,
      [](GDBusConnection* aConnection, const gchar*,
         gpointer aUserData) -> void {
        static_cast<DBusService*>(aUserData)->OnBusAcquired(aConnection);
      },
      [](GDBusConnection* aConnection, const gchar*,
         gpointer aUserData) -> void {
        static_cast<DBusService*>(aUserData)->OnNameAcquired(aConnection);
      },
      [](GDBusConnection* aConnection, const gchar*,
         gpointer aUserData) -> void {
        static_cast<DBusService*>(aUserData)->OnNameLost(aConnection);
      },
      this, nullptr);

  if (!mDBusID) {
    g_warning("DBusService: g_bus_own_name() failed!");
    return false;
  }

  return true;
}

void DBusService::StopFreedesktopListener() {
  OnNameLost(mConnection);
  if (mDBusID) {
    g_bus_unown_name(mDBusID);
    mDBusID = 0;
  }
  mIntrospectionData = nullptr;
}
