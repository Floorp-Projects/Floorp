/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/DNSListenerProxy.h"
#include "nsICancelable.h"
#include "nsIEventTarget.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(DNSListenerProxy,
                  nsIDNSListener,
                  nsIDNSListenerProxy)

NS_IMETHODIMP
DNSListenerProxy::OnLookupComplete(nsICancelable* aRequest,
                                   nsIDNSRecord* aRecord,
                                   nsresult aStatus)
{
  nsRefPtr<OnLookupCompleteRunnable> r =
    new OnLookupCompleteRunnable(mListener, aRequest, aRecord, aStatus);
  return mTargetThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
DNSListenerProxy::OnLookupCompleteRunnable::Run()
{
  mListener->OnLookupComplete(mRequest, mRecord, mStatus);
  return NS_OK;
}

NS_IMETHODIMP
DNSListenerProxy::GetOriginalListener(nsIDNSListener **aOriginalListener)
{
  NS_IF_ADDREF(*aOriginalListener = mListener);
  return NS_OK;
}

} // namespace net
} // namespace mozilla
