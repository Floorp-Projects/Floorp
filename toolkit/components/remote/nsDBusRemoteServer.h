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

#include <gio/gio.h>
#include "mozilla/RefPtr.h"
#include "mozilla/GRefPtr.h"

class nsDBusRemoteServer final : public nsRemoteServer,
                                 public nsUnixRemoteServer {
 public:
  ~nsDBusRemoteServer() override { Shutdown(); }

  nsresult Startup(const char* aAppName, const char* aProfileName) override;
  void Shutdown() override;

  void OnBusAcquired(GDBusConnection* aConnection);
  void OnNameAcquired(GDBusConnection* aConnection);
  void OnNameLost(GDBusConnection* aConnection);

  bool HandleOpenURL(const gchar* aInterfaceName, const gchar* aMethodName,
                     const nsACString& aParam);

 private:
  uint mDBusID = 0;
  uint mRegistrationId = 0;
  GDBusConnection* mConnection = nullptr;
  RefPtr<GDBusNodeInfo> mIntrospectionData;

  nsCString mAppName;
  nsCString mPathName;
};

#endif  // __nsDBusRemoteServer_h__
