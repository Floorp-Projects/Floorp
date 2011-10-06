/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Nokia.
 *
 * The Initial Developer of the Original Code is Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jeremias Bosch <jeremias.bosch@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMaemoNetworkManager.h"
#include "mozilla/ReentrantMonitor.h"

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
static bool gConnectionCallbackInvoked = false;

using namespace mozilla;

static ReentrantMonitor* gReentrantMonitor = nsnull;

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
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

    // When we are not connected, we are always disconnected.
    gInternalState = (CON_IC_STATUS_CONNECTED == status ?
                     InternalState_Connected : InternalState_Disconnected);

    gConnectionCallbackInvoked = PR_TRUE;
    mon.Notify();
  }

  NotifyNetworkLinkObservers();
}

bool
nsMaemoNetworkManager::OpenConnectionSync()
{
  if (NS_IsMainThread() || !gConnection)
    return PR_FALSE;

  // protect gInternalState.  This also allows us
  // to block and wait in this method on this thread
  // until our callback on the main thread.
  ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

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

bool
nsMaemoNetworkManager::IsConnected()
{
  return gInternalState == InternalState_Connected;
}

bool
nsMaemoNetworkManager::GetLinkStatusKnown()
{
  return gInternalState != InternalState_Invalid;
}

bool
nsMaemoNetworkManager::Startup()
{
  if (gConnection)
    return PR_TRUE;

  gReentrantMonitor = new ReentrantMonitor("MaemoAutodialer");
  if (!gReentrantMonitor)
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
    delete gReentrantMonitor;
    gReentrantMonitor = nsnull;
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

  if (gReentrantMonitor) {
    // notify anyone waiting
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);
    gInternalState = InternalState_Invalid;    
    mon.Notify();
  }
  
  // We are leaking the gReentrantMonitor because a race condition could occur. We need
  // a notification after xpcom-shutdown-threads so we can safely delete the monitor
}

