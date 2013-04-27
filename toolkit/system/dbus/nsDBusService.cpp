/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDBusService.h"
#include "nsComponentManagerUtils.h"
#include "nsAutoPtr.h"

#include <glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

nsDBusService::nsDBusService() {
  mConnection = nullptr;
  mSingleClient = nullptr;
}

nsDBusService::~nsDBusService() {
  NS_ASSERTION(!mSingleClient, "Client failed to unregister");
  DropConnection();
  if (mReconnectTimer) {
    mReconnectTimer->Cancel();
  }
  gSingleton = nullptr;
}

NS_IMPL_ISUPPORTS1(nsDBusService, nsDBusService)
NS_DEFINE_STATIC_IID_ACCESSOR(nsDBusService, NS_DBUS_IID)

nsDBusService* nsDBusService::gSingleton = nullptr;

already_AddRefed<nsDBusService>
nsDBusService::Get() {
  if (!gSingleton) {
    gSingleton = new nsDBusService();
  }
  nsRefPtr<nsDBusService> ret = gSingleton;
  return ret.forget();
}
  
nsresult
nsDBusService::AddClient(DBusClient* client) {
  NS_ASSERTION(!mSingleClient, "Only one client supported right now");
  mSingleClient = client;
  nsresult rv = CreateConnection();
  if (NS_FAILED(rv)) {
    mSingleClient = nullptr;
  }
  return rv;
}

void
nsDBusService::RemoveClient(DBusClient* client) {
  NS_ASSERTION(mSingleClient == client, "Removing wrong client");
  mSingleClient = nullptr;
}
  
DBusPendingCall*
nsDBusService::SendWithReply(DBusClient* client, DBusMessage* message) {
  DBusPendingCall* reply = nullptr;
  if (mConnection) {
    if (!dbus_connection_send_with_reply(mConnection, message, &reply, -1)) {
      reply = nullptr;
    }
  }
  dbus_message_unref(message);
  return reply;
}

bool nsDBusService::HandleMessage(DBusMessage* message) {
  if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL,
                            "Disconnected")) {
    HandleDBusDisconnect();
    return false;
  }
  
  return mSingleClient && mSingleClient->HandleMessage(message);
}

static DBusHandlerResult dbus_filter(DBusConnection* connection,
                                     DBusMessage* message,
                                     void* user_data) {
  return static_cast<nsDBusService*>(user_data)->HandleMessage(message)
    ? DBUS_HANDLER_RESULT_HANDLED : DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void nsDBusService::DoTimerCallback(nsITimer *aTimer) {
  if (aTimer == mReconnectTimer.get()) {
    nsresult rv = CreateConnection();
    if (NS_SUCCEEDED(rv)) {
      mReconnectTimer->Cancel();
      mReconnectTimer = nullptr;
    }
  }
}

static void TimerCallback(nsITimer *aTimer, void *aClosure) {
  static_cast<nsDBusService*>(aClosure)->DoTimerCallback(aTimer);
}

void nsDBusService::DropConnection() {
  if (mConnection) {
    dbus_connection_remove_filter(mConnection, dbus_filter, this);
    if (mSingleClient) {
      mSingleClient->UnregisterWithConnection(mConnection);
    }
    dbus_connection_unref(mConnection);
    mConnection = nullptr;
  }
}

void nsDBusService::HandleDBusDisconnect() {
  DropConnection();

  nsresult rv;
  mReconnectTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv))
    return;
  rv = mReconnectTimer->InitWithFuncCallback(TimerCallback, this,
                                             5000, nsITimer::TYPE_REPEATING_SLACK);
  if (NS_FAILED(rv)) {
    mReconnectTimer = nullptr;
    return;
  }
}

nsresult nsDBusService::CreateConnection() {
  mConnection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
  if (!mConnection)
    return NS_ERROR_FAILURE;

  dbus_connection_set_exit_on_disconnect(mConnection, false);
  dbus_connection_setup_with_g_main(mConnection, NULL);

  if (!dbus_connection_add_filter(mConnection, dbus_filter, this, NULL))
    return NS_ERROR_FAILURE;

  mSingleClient->RegisterWithConnection(mConnection);
  return NS_OK;
}
