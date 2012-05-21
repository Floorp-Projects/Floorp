/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    gConnectionCallbackInvoked = true;
    mon.Notify();
  }

  NotifyNetworkLinkObservers();
}

bool
nsMaemoNetworkManager::OpenConnectionSync()
{
  if (NS_IsMainThread() || !gConnection)
    return false;

  // protect gInternalState.  This also allows us
  // to block and wait in this method on this thread
  // until our callback on the main thread.
  ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

  gConnectionCallbackInvoked = false;

  if (!con_ic_connection_connect(gConnection,
                                 CON_IC_CONNECT_FLAG_NONE))
    g_error("openConnectionSync: Error while connecting. %p \n",
            (void*) PR_GetCurrentThread());

  while (!gConnectionCallbackInvoked)
    mon.Wait();

  if (gInternalState == InternalState_Connected)
    return true;

  return false;
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
    return true;

  gReentrantMonitor = new ReentrantMonitor("MaemoAutodialer");
  if (!gReentrantMonitor)
    return false;

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
    return false;
  }

  g_signal_connect(G_OBJECT(gConnection),
                   "connection-event",
                   G_CALLBACK(connection_event_callback),
                   nsnull);
  
  g_object_set(G_OBJECT(gConnection),
               "automatic-connection-events",
               true,
               nsnull);
  return true;
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

