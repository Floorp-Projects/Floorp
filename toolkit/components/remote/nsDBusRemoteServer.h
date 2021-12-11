/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsDBusRemoteServer_h__
#define __nsDBusRemoteServer_h__

#include "nsRemoteServer.h"
#include "nsUnixRemoteServer.h"
#include "mozilla/DBusHelpers.h"

class nsDBusRemoteServer final : public nsRemoteServer,
                                 public nsUnixRemoteServer {
 public:
  nsDBusRemoteServer() : mConnection(nullptr), mAppName(nullptr) {}
  ~nsDBusRemoteServer() override { Shutdown(); }

  nsresult Startup(const char* aAppName, const char* aProfileName) override;
  void Shutdown() override;

  DBusHandlerResult HandleDBusMessage(DBusConnection* aConnection,
                                      DBusMessage* msg);
  void UnregisterDBusInterface(DBusConnection* aConnection);

 private:
  DBusHandlerResult OpenURL(DBusMessage* msg);
  DBusHandlerResult Introspect(DBusMessage* msg);

  // The connection is owned by DBus library
  RefPtr<DBusConnection> mConnection;
  nsCString mAppName;
  nsCString mPathName;
};

#endif  // __nsDBusRemoteServer_h__
