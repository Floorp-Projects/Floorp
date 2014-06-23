/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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

protected:
  virtual ~nsNetworkManagerListener();

private:
  void NotifyNetworkStatusObservers();

  nsCOMPtr<nsDBusService> mDBUS;
  bool                    mLinkUp;
  bool                    mNetworkManagerActive;
  bool                    mOK;
};

#endif /*NSNETWORKMANAGERLISTENER_H_*/
