/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsInterfaceRequestorAgg.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

class nsInterfaceRequestorAgg MOZ_FINAL : public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR

  nsInterfaceRequestorAgg(nsIInterfaceRequestor *aFirst,
                          nsIInterfaceRequestor *aSecond)
    : mFirst(aFirst)
    , mSecond(aSecond) {}

  nsCOMPtr<nsIInterfaceRequestor> mFirst, mSecond;
};

// XXX This needs to support threadsafe refcounting until we fix bug 243591.
NS_IMPL_THREADSAFE_ISUPPORTS1(nsInterfaceRequestorAgg, nsIInterfaceRequestor)

NS_IMETHODIMP
nsInterfaceRequestorAgg::GetInterface(const nsIID &aIID, void **aResult)
{
  nsresult rv = NS_ERROR_NO_INTERFACE;
  if (mFirst)
    rv = mFirst->GetInterface(aIID, aResult);
  if (mSecond && NS_FAILED(rv))
    rv = mSecond->GetInterface(aIID, aResult);
  return rv;
}

nsresult
NS_NewInterfaceRequestorAggregation(nsIInterfaceRequestor *aFirst,
                                    nsIInterfaceRequestor *aSecond,
                                    nsIInterfaceRequestor **aResult)
{
  *aResult = new nsInterfaceRequestorAgg(aFirst, aSecond);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
