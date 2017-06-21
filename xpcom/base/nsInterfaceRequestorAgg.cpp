/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsInterfaceRequestorAgg.h"
#include "nsIInterfaceRequestor.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"

class nsInterfaceRequestorAgg final : public nsIInterfaceRequestor
{
public:
  // XXX This needs to support threadsafe refcounting until we fix bug 243591.
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR

  nsInterfaceRequestorAgg(nsIInterfaceRequestor* aFirst,
                          nsIInterfaceRequestor* aSecond,
                          nsIEventTarget* aConsumerTarget = nullptr)
    : mFirst(aFirst)
    , mSecond(aSecond)
    , mConsumerTarget(aConsumerTarget)
  {
    if (!mConsumerTarget) {
      mConsumerTarget = GetCurrentThreadEventTarget();
    }
  }

private:
  ~nsInterfaceRequestorAgg();

  nsCOMPtr<nsIInterfaceRequestor> mFirst, mSecond;
  nsCOMPtr<nsIEventTarget> mConsumerTarget;
};

NS_IMPL_ISUPPORTS(nsInterfaceRequestorAgg, nsIInterfaceRequestor)

NS_IMETHODIMP
nsInterfaceRequestorAgg::GetInterface(const nsIID& aIID, void** aResult)
{
  nsresult rv = NS_ERROR_NO_INTERFACE;
  if (mFirst) {
    rv = mFirst->GetInterface(aIID, aResult);
  }
  if (mSecond && NS_FAILED(rv)) {
    rv = mSecond->GetInterface(aIID, aResult);
  }
  return rv;
}

nsInterfaceRequestorAgg::~nsInterfaceRequestorAgg()
{
  NS_ProxyRelease(
    "nsInterfaceRequestorAgg::mFirst", mConsumerTarget, mFirst.forget());
  NS_ProxyRelease(
    "nsInterfaceRequestorAgg::mSecond", mConsumerTarget, mSecond.forget());
}

nsresult
NS_NewInterfaceRequestorAggregation(nsIInterfaceRequestor* aFirst,
                                    nsIInterfaceRequestor* aSecond,
                                    nsIInterfaceRequestor** aResult)
{
  *aResult = new nsInterfaceRequestorAgg(aFirst, aSecond);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsresult
NS_NewInterfaceRequestorAggregation(nsIInterfaceRequestor* aFirst,
                                    nsIInterfaceRequestor* aSecond,
                                    nsIEventTarget* aTarget,
                                    nsIInterfaceRequestor** aResult)
{
  *aResult = new nsInterfaceRequestorAgg(aFirst, aSecond, aTarget);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aResult);
  return NS_OK;
}
