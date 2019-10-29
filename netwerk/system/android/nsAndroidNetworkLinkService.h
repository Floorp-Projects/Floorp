/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSANDROIDNETWORKLINKSERVICE_H_
#define NSANDROIDNETWORKLINKSERVICE_H_

#include "nsINetworkLinkService.h"
#include "nsIObserver.h"
#include "../netlink/NetlinkService.h"
#include "mozilla/TimeStamp.h"

class nsAndroidNetworkLinkService
    : public nsINetworkLinkService,
      public nsIObserver,
      public mozilla::net::NetlinkServiceListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINETWORKLINKSERVICE
  NS_DECL_NSIOBSERVER

  nsAndroidNetworkLinkService();

  nsresult Init();

  void OnNetworkChanged() override;
  void OnLinkUp() override;
  void OnLinkDown() override;
  void OnLinkStatusKnown() override;

 private:
  virtual ~nsAndroidNetworkLinkService() = default;

  // Called when xpcom-shutdown-threads is received.
  nsresult Shutdown();

  // Sends the network event.
  void SendEvent(const char* aEventID);

  mozilla::Atomic<bool, mozilla::Relaxed> mStatusIsKnown;

  RefPtr<mozilla::net::NetlinkService> mNetlinkSvc;

  // Time stamp of last NS_NETWORK_LINK_DATA_CHANGED event
  mozilla::TimeStamp mNetworkChangeTime;
};

#endif /* NSANDROIDNETWORKLINKSERVICE_H_ */
