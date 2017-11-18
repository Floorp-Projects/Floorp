/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsDBusRemoteService_h__
#define __nsDBusRemoteService_h__

#include "nsIRemoteService.h"
#include "mozilla/DBusHelpers.h"
#include "nsString.h"

class nsDBusRemoteService final : public nsIRemoteService
{
public:
  // We will be a static singleton, so don't use the ordinary methods.
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREMOTESERVICE

  nsDBusRemoteService()
    : mConnection(nullptr)
    , mAppName(nullptr)
    { }

  DBusHandlerResult HandleDBusMessage(DBusConnection *aConnection, DBusMessage *msg);
  void UnregisterDBusInterface(DBusConnection *aConnection);

private:
  ~nsDBusRemoteService() { }

  DBusHandlerResult OpenURL(DBusMessage *msg);
  DBusHandlerResult Introspect(DBusMessage *msg);

  // The connection is owned by DBus library
  RefPtr<DBusConnection>  mConnection;
  nsCString               mAppName;
};

#endif // __nsDBusRemoteService_h__
