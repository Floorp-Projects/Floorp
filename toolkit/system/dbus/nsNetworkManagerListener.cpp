/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNetworkManagerListener.h"

#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsStringAPI.h"
#include "nsCRT.h"

// Define NetworkManager API constants. This avoids a dependency on
// NetworkManager-devel.
#define NM_DBUS_SERVICE                 "org.freedesktop.NetworkManager"
#define NM_DBUS_PATH                    "/org/freedesktop/NetworkManager"
#define NM_DBUS_INTERFACE               "org.freedesktop.NetworkManager"
#define NM_DBUS_SIGNAL_STATE_CHANGE     "StateChange" /* Deprecated in 0.7.x */
#define NM_DBUS_SIGNAL_STATE_CHANGED    "StateChanged"

#define NM_STATE_CONNECTED_OLD    3 /* Before NM 0.9.0 */
#define NM_STATE_CONNECTED_LOCAL  50
#define NM_STATE_CONNECTED_SITE   60
#define NM_STATE_CONNECTED_GLOBAL 70

nsNetworkManagerListener::nsNetworkManagerListener()
  : mLinkUp(true), mNetworkManagerActive(false),
    mOK(true)
{
}

nsNetworkManagerListener::~nsNetworkManagerListener()
{
  if (mDBUS) {
    mDBUS->RemoveClient(this);
  }
}

NS_IMPL_ISUPPORTS1(nsNetworkManagerListener, nsINetworkLinkService)

nsresult
nsNetworkManagerListener::GetIsLinkUp(bool* aIsUp)
{
  *aIsUp = mLinkUp;
  return NS_OK;
}

nsresult
nsNetworkManagerListener::GetLinkStatusKnown(bool* aKnown)
{
  *aKnown = mNetworkManagerActive;
  return NS_OK;
}

nsresult
nsNetworkManagerListener::GetLinkType(uint32_t *aLinkType)
{
  NS_ENSURE_ARG_POINTER(aLinkType);

  // XXX This function has not yet been implemented for this platform
  *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  return NS_OK;
}

nsresult
nsNetworkManagerListener::Init()
{
  mDBUS = nsDBusService::Get();
  if (!mDBUS) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = mDBUS->AddClient(this);
  if (NS_FAILED(rv)) {
    mDBUS = nullptr;
    return rv;
  }
  if (!mOK) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

static void
NetworkStatusNotify(DBusPendingCall *pending, void* user_data)
{
  DBusMessage* msg = dbus_pending_call_steal_reply(pending);
  if (!msg) {
    return;
  }
  if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_RETURN) {
    static_cast<nsNetworkManagerListener*>(user_data)->UpdateNetworkStatus(msg);
  }
  dbus_message_unref(msg);
}

void
nsNetworkManagerListener::RegisterWithConnection(DBusConnection* connection)
{
  DBusError error;
  dbus_error_init(&error);

  dbus_bus_add_match(connection,
                     "type='signal',"
                     "interface='" NM_DBUS_INTERFACE "',"
                     "sender='" NM_DBUS_SERVICE "',"
                     "path='" NM_DBUS_PATH "'", &error);
  mOK = !dbus_error_is_set(&error);
  dbus_error_free(&error);
  if (!mOK) {
    return;
  }
  DBusMessage* msg =
    dbus_message_new_method_call(NM_DBUS_SERVICE, NM_DBUS_PATH,
                                 NM_DBUS_INTERFACE, "state");
  if (!msg) {
    mOK = false;
    return;
  }

  DBusPendingCall* reply = mDBUS->SendWithReply(this, msg);
  if (!reply) {
    mOK = false;
    return;
  }

  dbus_pending_call_set_notify(reply, NetworkStatusNotify, this, NULL);
  dbus_pending_call_unref(reply);
}

void
nsNetworkManagerListener::NotifyNetworkStatusObservers() {
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  if (!observerService) {
    return;
  }
  const PRUnichar* status;
  if (mNetworkManagerActive) {
    status = mLinkUp ? NS_LITERAL_STRING(NS_NETWORK_LINK_DATA_UP).get()
                     : NS_LITERAL_STRING(NS_NETWORK_LINK_DATA_DOWN).get();
  } else {
    status = NS_LITERAL_STRING(NS_NETWORK_LINK_DATA_UNKNOWN).get();
  }

  observerService->NotifyObservers(static_cast<nsISupports*>(this),
                                   NS_NETWORK_LINK_TOPIC, status);
}

void
nsNetworkManagerListener::UnregisterWithConnection(DBusConnection* connection)
{
  mNetworkManagerActive = false;
  NotifyNetworkStatusObservers();
}

bool
nsNetworkManagerListener::HandleMessage(DBusMessage* message)
{
  if (dbus_message_is_signal(message, NM_DBUS_INTERFACE,
                             NM_DBUS_SIGNAL_STATE_CHANGE) ||
      dbus_message_is_signal(message, NM_DBUS_INTERFACE,
                             NM_DBUS_SIGNAL_STATE_CHANGED)) {
    UpdateNetworkStatus(message);
    return true;
  }
  return false;
}

void
nsNetworkManagerListener::UpdateNetworkStatus(DBusMessage* msg)
{
  int32_t result;
  if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &result,
                             DBUS_TYPE_INVALID)) {
    return;
  }

  mNetworkManagerActive = true;

  bool wasUp = mLinkUp;
  mLinkUp = result == NM_STATE_CONNECTED_OLD ||
            result == NM_STATE_CONNECTED_LOCAL ||
            result == NM_STATE_CONNECTED_SITE ||
            result == NM_STATE_CONNECTED_GLOBAL;
  if (wasUp == mLinkUp) {
    return;
  }

  NotifyNetworkStatusObservers();
}

