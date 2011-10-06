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

#endif /*NSDBUSSERVICE_H_*/
