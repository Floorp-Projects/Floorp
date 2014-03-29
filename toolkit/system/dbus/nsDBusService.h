/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDBUSSERVICE_H_
#define NSDBUSSERVICE_H_

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>

#include "nsITimer.h"
#include "nsCOMPtr.h"

class DBusClient {
public:
  virtual void RegisterWithConnection(DBusConnection* connection) = 0;
  virtual void UnregisterWithConnection(DBusConnection* connection) = 0;
  virtual bool HandleMessage(DBusMessage* msg) = 0;
};

#define NS_DBUS_IID \
  { 0xba4f79b7, 0x0d4c, 0x4b3a, { 0x8a, 0x85, 0x6f, 0xb3, 0x0d, 0xce, 0xf5, 0x11 } }

/**
 * The nsDBusService component interfaces with DBUS to communicate with daemons
 * in systems supporting DBUS. It links dynamically to the DBUS libraries so
 * will not load on systems without those libraries ... but that's harmless.
 *
 * Currently the only daemon we communicate with is NetworkManager. We listen
 * for NetworkManager state changes; we set nsIOService's offline status to
 * FALSE when NetworkManager reports NM_STATE_CONNECTED, and to TRUE otherwise.
 * We also solicit the current status from NetworkManager when this component
 * gets loaded.
 * 
 * In the future we could extend this class to talk to other daemons.
 * 
 * Currently all communication is asynchronous. This isn't hard to implement
 * and avoids blocking our main thread.
 */
class nsDBusService : public nsISupports
{
public:
  nsDBusService();
  virtual ~nsDBusService();

  NS_DECL_ISUPPORTS
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DBUS_IID)

  static already_AddRefed<nsDBusService> Get();
  
  nsresult AddClient(DBusClient* client);
  void RemoveClient(DBusClient* client);

  DBusPendingCall* SendWithReply(DBusClient* client, DBusMessage* message);

  // these should be private but we need to call them from static functions
  bool HandleMessage(DBusMessage* message);
  void DoTimerCallback(nsITimer* aTimer);

private:
  nsresult CreateConnection();
  void DropConnection();
  void HandleDBusDisconnect();

  static nsDBusService* gSingleton;
  
  DBusConnection*    mConnection;
  nsCOMPtr<nsITimer> mReconnectTimer;
  DBusClient*        mSingleClient;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsDBusService, NS_DBUS_IID)

#endif /*NSDBUSSERVICE_H_*/
