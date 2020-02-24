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
  void OnNetworkIDChanged() override;
  void OnLinkUp() override;
  void OnLinkDown() override;
  void OnLinkStatusKnown() override;
  void OnDnsSuffixListUpdated() override;

 private:
  virtual ~nsAndroidNetworkLinkService() = default;

  // Called when xpcom-shutdown-threads is received.
  nsresult Shutdown();

  // Sends the network event.
  void NotifyObservers(const char* aTopic, const char* aData);

  mozilla::Atomic<bool, mozilla::Relaxed> mStatusIsKnown;

  RefPtr<mozilla::net::NetlinkService> mNetlinkSvc;
};

#endif /* NSANDROIDNETWORKLINKSERVICE_H_ */
