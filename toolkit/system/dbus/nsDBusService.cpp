/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Original Author: Robert O'Callahan (rocallahan@novell.com)
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDBusService.h"
#include "nsComponentManagerUtils.h"

#include <glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

nsDBusService::nsDBusService() {
  mConnection = nsnull;
  mSingleClient = nsnull;
}

nsDBusService::~nsDBusService() {
  NS_ASSERTION(!mSingleClient, "Client failed to unregister");
  DropConnection();
  if (mReconnectTimer) {
    mReconnectTimer->Cancel();
  }
  gSingleton = nsnull;
}

NS_IMPL_ISUPPORTS1(nsDBusService, nsDBusService)
NS_DEFINE_STATIC_IID_ACCESSOR(nsDBusService, NS_DBUS_IID)

nsDBusService* nsDBusService::gSingleton = nsnull;

already_AddRefed<nsDBusService>
nsDBusService::Get() {
  if (!gSingleton) {
    gSingleton = new nsDBusService();
  }
  NS_IF_ADDREF(gSingleton);
  return gSingleton;
}
  
nsresult
nsDBusService::AddClient(DBusClient* client) {
  NS_ASSERTION(!mSingleClient, "Only one client supported right now");
  mSingleClient = client;
  nsresult rv = CreateConnection();
  if (NS_FAILED(rv)) {
    mSingleClient = nsnull;
  }
  return rv;
}

void
nsDBusService::RemoveClient(DBusClient* client) {
  NS_ASSERTION(mSingleClient == client, "Removing wrong client");
  mSingleClient = nsnull;
}
  
DBusPendingCall*
nsDBusService::SendWithReply(DBusClient* client, DBusMessage* message) {
  DBusPendingCall* reply = nsnull;
  if (mConnection) {
    if (!dbus_connection_send_with_reply(mConnection, message, &reply, -1)) {
      reply = nsnull;
    }
  }
  dbus_message_unref(message);
  return reply;
}

bool nsDBusService::HandleMessage(DBusMessage* message) {
  if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL,
                            "Disconnected")) {
    HandleDBusDisconnect();
    return PR_FALSE;
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
      mReconnectTimer = nsnull;
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
    mConnection = nsnull;
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
    mReconnectTimer = nsnull;
    return;
  }
}

nsresult nsDBusService::CreateConnection() {
  mConnection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
  if (!mConnection)
    return NS_ERROR_FAILURE;

  dbus_connection_set_exit_on_disconnect(mConnection, PR_FALSE);
  dbus_connection_setup_with_g_main(mConnection, NULL);

  if (!dbus_connection_add_filter(mConnection, dbus_filter, this, NULL))
    return NS_ERROR_FAILURE;

  mSingleClient->RegisterWithConnection(mConnection);
  return NS_OK;
}
