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
 
#include "nsNetworkManagerListener.h"

#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsStringAPI.h"
#include "nsCRT.h"

// Define NetworkManager API constants. This avoids a dependency on
// NetworkManager-devel.
#define NM_DBUS_SERVICE                 "org.freedesktop.NetworkManager"
#define NM_DBUS_PATH                            "/org/freedesktop/NetworkManager"
#define NM_DBUS_INTERFACE                       "org.freedesktop.NetworkManager"
#define NM_DBUS_SIGNAL_STATE_CHANGE             "StateChange"
typedef enum NMState
{
        NM_STATE_UNKNOWN = 0,
        NM_STATE_ASLEEP,
        NM_STATE_CONNECTING,
        NM_STATE_CONNECTED,
        NM_STATE_DISCONNECTED
} NMState;

nsNetworkManagerListener::nsNetworkManagerListener() :
    mLinkUp(PR_TRUE), mNetworkManagerActive(PR_FALSE),
    mOK(PR_TRUE), mManageIOService(PR_TRUE)
{
}

nsNetworkManagerListener::~nsNetworkManagerListener() {
  if (mDBUS) {
    mDBUS->RemoveClient(this);
  }
}

NS_IMPL_ISUPPORTS1(nsNetworkManagerListener, nsINetworkLinkService)

nsresult
nsNetworkManagerListener::GetIsLinkUp(bool* aIsUp) {
  *aIsUp = mLinkUp;
  return NS_OK;
}

nsresult
nsNetworkManagerListener::GetLinkStatusKnown(bool* aKnown) {
  *aKnown = mNetworkManagerActive;
  return NS_OK;
}

nsresult
nsNetworkManagerListener::GetLinkType(PRUint32 *aLinkType)
{
  NS_ENSURE_ARG_POINTER(aLinkType);

  // XXX This function has not yet been implemented for this platform
  *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  return NS_OK;
}

nsresult
nsNetworkManagerListener::Init() {
  mDBUS = nsDBusService::Get();
  if (!mDBUS)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult rv = mDBUS->AddClient(this);
  if (NS_FAILED(rv)) {
    mDBUS = nsnull;
    return rv;
  }
  if (!mOK)
    return NS_ERROR_FAILURE;
  return NS_OK;
}

static void
NetworkStatusNotify(DBusPendingCall *pending, void* user_data) {
  DBusMessage* msg = dbus_pending_call_steal_reply(pending);
  if (!msg)
    return;
  if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_RETURN) {
    static_cast<nsNetworkManagerListener*>(user_data)->UpdateNetworkStatus(msg);
  }
  dbus_message_unref(msg);
}

void
nsNetworkManagerListener::RegisterWithConnection(DBusConnection* connection) {
  DBusError error;
  dbus_error_init(&error);
  
  dbus_bus_add_match(connection,
                     "type='signal',"
                     "interface='" NM_DBUS_INTERFACE "',"
                     "sender='" NM_DBUS_SERVICE "',"
                     "path='" NM_DBUS_PATH "'", &error);
  mOK = !dbus_error_is_set(&error);
  dbus_error_free(&error);
  if (!mOK)
    return;
  
  DBusMessage* msg =
    dbus_message_new_method_call(NM_DBUS_SERVICE, NM_DBUS_PATH,
                                 NM_DBUS_INTERFACE, "state");
  if (!msg) {
    mOK = PR_FALSE;
    return;
  }
  
  DBusPendingCall* reply = mDBUS->SendWithReply(this, msg);
  if (!reply) {
    mOK = PR_FALSE;
    return;
  }

  dbus_pending_call_set_notify(reply, NetworkStatusNotify, this, NULL);
  dbus_pending_call_unref(reply);
}

void
nsNetworkManagerListener::NotifyNetworkStatusObservers() {
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  if (!observerService)
    return;
    
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
nsNetworkManagerListener::UnregisterWithConnection(DBusConnection* connection) {
  mNetworkManagerActive = PR_FALSE;
  NotifyNetworkStatusObservers();
}

bool
nsNetworkManagerListener::HandleMessage(DBusMessage* message) {
  if (dbus_message_is_signal(message, NM_DBUS_INTERFACE,
                             NM_DBUS_SIGNAL_STATE_CHANGE)) {
    UpdateNetworkStatus(message);
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsNetworkManagerListener::UpdateNetworkStatus(DBusMessage* msg) {
  PRInt32 result;
  if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &result,
                             DBUS_TYPE_INVALID))
    return;

  mNetworkManagerActive = PR_TRUE;
  
  bool wasUp = mLinkUp;
  mLinkUp = result == NM_STATE_CONNECTED;
  if (wasUp == mLinkUp)
    return;

  NotifyNetworkStatusObservers();
}

