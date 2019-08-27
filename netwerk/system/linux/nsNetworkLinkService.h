/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set et sw=2 ts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSNETWORKLINKSERVICE_LINUX_H_
#define NSNETWORKLINKSERVICE_LINUX_H_

#include "nsINetworkLinkService.h"
#include "nsIObserver.h"
#include "../netlink/NetlinkService.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Atomics.h"

class nsNetworkLinkService : public nsINetworkLinkService,
                             public nsIObserver,
                             public mozilla::net::NetlinkServiceListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINETWORKLINKSERVICE
  NS_DECL_NSIOBSERVER

  nsNetworkLinkService();
  nsresult Init();

  void OnNetworkChanged() override;
  void OnLinkUp() override;
  void OnLinkDown() override;
  void OnLinkStatusKnown() override;

 private:
  virtual ~nsNetworkLinkService() = default;

  // Called when xpcom-shutdown-threads is received.
  nsresult Shutdown();

  // Sends the network event.
  void SendEvent(const char* aEventID);

  mozilla::Atomic<bool, mozilla::Relaxed> mStatusIsKnown;

  RefPtr<mozilla::net::NetlinkService> mNetlinkSvc;
};

#endif /* NSNETWORKLINKSERVICE_LINUX_H_ */
