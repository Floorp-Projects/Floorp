/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/DNSListenerProxy.h"
#include "nsICancelable.h"

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(DNSListenerProxy)
NS_IMPL_RELEASE(DNSListenerProxy)
NS_INTERFACE_MAP_BEGIN(DNSListenerProxy)
  NS_INTERFACE_MAP_ENTRY(nsIDNSListener)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(DNSListenerProxy)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
DNSListenerProxy::OnLookupComplete(nsICancelable* aRequest,
                                   nsIDNSRecord* aRecord, nsresult aStatus) {
  RefPtr<DNSListenerProxy> self = this;
  nsCOMPtr<nsICancelable> request = aRequest;
  nsCOMPtr<nsIDNSRecord> record = aRecord;
  return mTargetThread->Dispatch(
      NS_NewRunnableFunction("DNSListenerProxy::OnLookupComplete",
                             [self, request, record, aStatus]() {
                               Unused << self->mListener->OnLookupComplete(
                                   request, record, aStatus);
                               self->mListener = nullptr;
                             }),
      NS_DISPATCH_NORMAL);
}

}  // namespace net
}  // namespace mozilla
