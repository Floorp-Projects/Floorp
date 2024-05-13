/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DBusService_h__
#define DBusService_h__

#include <glib.h>
#include <gio/gio.h>
#include "mozilla/RefPtr.h"
#include "mozilla/GRefPtr.h"

namespace mozilla::widget {

class DBusService final {
 public:
  explicit DBusService(const char* aAppFile) : mAppFile(aAppFile) {}
  ~DBusService();

  // nsBaseAppShell overrides:
  bool Init();
  void Run();

  void StartDBusListening();
  void StopDBusListening();

  static void DBusSessionSleepCallback(GDBusProxy* aProxy, gchar* aSenderName,
                                       gchar* aSignalName,
                                       GVariant* aParameters,
                                       gpointer aUserData);
  static void DBusTimedatePropertiesChangedCallback(GDBusProxy* aProxy,
                                                    gchar* aSenderName,
                                                    gchar* aSignalName,
                                                    GVariant* aParameters,
                                                    gpointer aUserData);
  static void DBusConnectClientResponse(GObject* aObject, GAsyncResult* aResult,
                                        gpointer aUserData);

  bool LaunchApp(const char* aCommand, const char** aURIList, int aURIListLen);

  void HandleFreedesktopActivate(GVariant* aParameters,
                                 GDBusMethodInvocation* aReply);
  void HandleFreedesktopOpen(GVariant* aParameters,
                             GDBusMethodInvocation* aReply);
  void HandleFreedesktopActivateAction(GVariant* aParameters,
                                       GDBusMethodInvocation* aReply);

  bool StartFreedesktopListener();
  void StopFreedesktopListener();

  void OnBusAcquired(GDBusConnection* aConnection);
  void OnNameAcquired(GDBusConnection* aConnection);
  void OnNameLost(GDBusConnection* aConnection);

 private:
  // The connection is owned by DBus library
  uint mDBusID = 0;
  uint mRegistrationId = 0;
  GDBusConnection* mConnection = nullptr;
  RefPtr<GDBusNodeInfo> mIntrospectionData;
  const char* mAppFile = nullptr;
};

}  // namespace mozilla::widget

#endif  // DBusService_h__
