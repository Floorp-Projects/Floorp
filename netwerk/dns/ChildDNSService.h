/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ChildDNSService_h
#define mozilla_net_ChildDNSService_h


#include "nsPIDNSService.h"
#include "nsIObserver.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "DNSRequestChild.h"
#include "nsHashKeys.h"
#include "nsClassHashtable.h"

namespace mozilla {
namespace net {

class ChildDNSService MOZ_FINAL
  : public nsPIDNSService
  , public nsIObserver
{
public:
  // AsyncResolve (and CancelAsyncResolve) can be called off-main
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSPIDNSSERVICE
  NS_DECL_NSIDNSSERVICE
  NS_DECL_NSIOBSERVER

  ChildDNSService();

  static ChildDNSService* GetSingleton();

  void NotifyRequestDone(DNSRequestChild *aDnsRequest);
private:
  virtual ~ChildDNSService();

  void MOZ_ALWAYS_INLINE GetDNSRecordHashKey(const nsACString &aHost,
                                             uint32_t aFlags,
                                             const nsACString &aNetworkInterface,
                                             nsIDNSListener* aListener,
                                             nsACString &aHashKey);

  bool mFirstTime;
  bool mOffline;
  bool mDisablePrefetch;

  // We need to remember pending dns requests to be able to cancel them.
  nsClassHashtable<nsCStringHashKey, nsTArray<nsRefPtr<DNSRequestChild>>> mPendingRequests;
  Mutex mPendingRequestsLock;
};

} // namespace net
} // namespace mozilla
#endif // mozilla_net_ChildDNSService_h
