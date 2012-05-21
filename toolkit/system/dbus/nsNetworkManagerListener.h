/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSNETWORKMANAGERLISTENER_H_
#define NSNETWORKMANAGERLISTENER_H_

#include "nsINetworkLinkService.h"
#include "nsDBusService.h"

class nsNetworkManagerListener : public nsINetworkLinkService,
                                 public DBusClient {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKLINKSERVICE

  nsNetworkManagerListener();
  virtual ~nsNetworkManagerListener();

  nsresult Init();

  virtual void RegisterWithConnection(DBusConnection* connection);
  virtual void UnregisterWithConnection(DBusConnection* connection);
  virtual bool HandleMessage(DBusMessage* msg);

  /**
   * This gets called when NetworkManager sends us a StateChange signal,
   * or when we receive a reply to our inquiry.
   * The message contains the current NMState.
   */
  void UpdateNetworkStatus(DBusMessage* message);

private:
  void NotifyNetworkStatusObservers();

  nsCOMPtr<nsDBusService> mDBUS;
  bool                    mLinkUp;
  bool                    mNetworkManagerActive;
  bool                    mOK;
  bool                    mManageIOService;
};

#endif /*NSNETWORKMANAGERLISTENER_H_*/
