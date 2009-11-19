/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MaemoAutodial.
 *
 * The Initial Developer of the Original Code is
 * Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jeremias Bosch <jeremias.bosch@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMaemoNetworkManager.h"
#include "mozilla/Monitor.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <conic/conicconnection.h>
#include <conic/conicconnectionevent.h>
#include <conicstatisticsevent.h>

#include <stdio.h>
#include <unistd.h>

#include "nsIOService.h"
#include "nsIObserverService.h"
#include "nsIOService.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"

#include "nsINetworkLinkService.h"

enum InternalState
{
  InternalState_Invalid = -1,
  InternalState_Disconnected,
  InternalState_Connected
};

static InternalState gInternalState = InternalState_Invalid;
static ConIcConnection* gConnection = nsnull;
static PRBool gConnectionCallbackInvoked = PR_FALSE;

using namespace mozilla;

static Monitor* gMonitor = nsnull;

static void NotifyNetworkLinkObservers()
{
  nsCOMPtr<nsIIOService> ioService = do_GetService("@mozilla.org/network/io-service;1");
  if (!ioService)
    return;

  ioService->SetOffline(gInternalState != InternalState_Connected);
}

static void
connection_event_callback(ConIcConnection *aConnection,
                          ConIcConnectionEvent *aEvent,
                          gpointer aUser_data)
{
  ConIcConnectionStatus status = con_ic_connection_event_get_status(aEvent);
  {
    MonitorAutoEnter mon(*gMonitor);

    // When we are not connected, we are always disconnected.
    gInternalState = (CON_IC_STATUS_CONNECTED == status ?
                     InternalState_Connected : InternalState_Disconnected);

    gConnectionCallbackInvoked = PR_TRUE;
    mon.Notify();
  }

  NotifyNetworkLinkObservers();
}

PRBool
nsMaemoNetworkManager::OpenConnectionSync()
{
  if (NS_IsMainThread() || !gConnection)
    return PR_FALSE;

  // protect gInternalState.  This also allows us
  // to block and wait in this method on this thread
  // until our callback on the main thread.
  MonitorAutoEnter mon(*gMonitor);

  gConnectionCallbackInvoked = PR_FALSE;

  if (!con_ic_connection_connect(gConnection,
                                 CON_IC_CONNECT_FLAG_NONE))
    g_error("openConnectionSync: Error while connecting. %p \n",
            (void*) PR_GetCurrentThread());

  while (!gConnectionCallbackInvoked)
    mon.Wait();

  if (gInternalState == InternalState_Connected)
    return PR_TRUE;

  return PR_FALSE;
}

void
nsMaemoNetworkManager::CloseConnection()
{
  if (gConnection)
    con_ic_connection_disconnect(gConnection);
}

PRBool
nsMaemoNetworkManager::IsConnected()
{
  return gInternalState == InternalState_Connected;
}

PRBool
nsMaemoNetworkManager::GetLinkStatusKnown()
{
  return gInternalState != InternalState_Invalid;
}

PRBool
nsMaemoNetworkManager::Startup()
{
  if (gConnection)
    return PR_TRUE;

  gMonitor = new Monitor("MaemoAutodialer");
  if (!gMonitor)
    return PR_FALSE;

  DBusError error;
  dbus_error_init(&error);

  DBusConnection* dbusConnection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
  NS_ASSERTION(dbusConnection, "Error when connecting to the session bus");

  dbus_connection_setup_with_g_main(dbusConnection, nsnull);

  // grab a connection:
  gConnection = con_ic_connection_new();
  NS_ASSERTION(gConnection, "Error when creating connection");
  if (!gConnection) {
    delete gMonitor;
    gMonitor = nsnull;
    return PR_FALSE;
  }

  g_signal_connect(G_OBJECT(gConnection),
                   "connection-event",
                   G_CALLBACK(connection_event_callback),
                   nsnull);
  
  g_object_set(G_OBJECT(gConnection),
               "automatic-connection-events",
               PR_TRUE,
               nsnull);
  return PR_TRUE;
}

void
nsMaemoNetworkManager::Shutdown()
{
  gConnection = nsnull;

  if (gMonitor) {
    // notify anyone waiting
    MonitorAutoEnter mon(*gMonitor);
    gInternalState = InternalState_Invalid;    
    mon.Notify();
  }
  
  // We are leaking the gMonitor because a race condition could occur. We need
  // a notification after xpcom-shutdown-threads so we can safely delete the monitor
}

