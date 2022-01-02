/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ChildDNSService_h
#define mozilla_net_ChildDNSService_h

#include "DNSServiceBase.h"
#include "nsPIDNSService.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "DNSRequestChild.h"
#include "DNSRequestParent.h"
#include "nsHashKeys.h"
#include "nsClassHashtable.h"

namespace mozilla {
namespace net {

class TRRServiceParent;

class ChildDNSService final : public DNSServiceBase, public nsPIDNSService {
 public:
  // AsyncResolve (and CancelAsyncResolve) can be called off-main
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSPIDNSSERVICE
  NS_DECL_NSIDNSSERVICE
  NS_DECL_NSIOBSERVER

  ChildDNSService();

  static already_AddRefed<ChildDNSService> GetSingleton();

  void NotifyRequestDone(DNSRequestSender* aDnsRequest);

 private:
  virtual ~ChildDNSService() = default;

  void MOZ_ALWAYS_INLINE GetDNSRecordHashKey(
      const nsACString& aHost, const nsACString& aTrrServer, uint16_t aType,
      const OriginAttributes& aOriginAttributes, uint32_t aFlags,
      uintptr_t aListenerAddr, nsACString& aHashKey);
  nsresult AsyncResolveInternal(const nsACString& hostname, uint16_t type,
                                uint32_t flags, nsIDNSResolverInfo* aResolver,
                                nsIDNSListener* listener,
                                nsIEventTarget* target_,
                                const OriginAttributes& aOriginAttributes,
                                nsICancelable** result);
  nsresult CancelAsyncResolveInternal(
      const nsACString& aHostname, uint16_t aType, uint32_t aFlags,
      nsIDNSResolverInfo* aResolver, nsIDNSListener* aListener,
      nsresult aReason, const OriginAttributes& aOriginAttributes);

  bool mODoHActivated = false;

  // We need to remember pending dns requests to be able to cancel them.
  nsClassHashtable<nsCStringHashKey, nsTArray<RefPtr<DNSRequestSender>>>
      mPendingRequests;
  Mutex mPendingRequestsLock{"DNSPendingRequestsLock"};
  RefPtr<TRRServiceParent> mTRRServiceParent;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ChildDNSService_h
