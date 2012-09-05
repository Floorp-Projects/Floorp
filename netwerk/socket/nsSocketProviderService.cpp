/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsISocketProvider.h"
#include "nsSocketProviderService.h"
#include "nsError.h"

////////////////////////////////////////////////////////////////////////////////

nsresult
nsSocketProviderService::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsISocketProviderService> inst = new nsSocketProviderService();
  if (!inst)
    rv = NS_ERROR_OUT_OF_MEMORY;
  else
    rv = inst->QueryInterface(aIID, aResult);
  return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSocketProviderService, nsISocketProviderService)

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsSocketProviderService::GetSocketProvider(const char         *type,
                                           nsISocketProvider **result)
{
  nsresult rv;
  nsAutoCString contractID(
          NS_LITERAL_CSTRING(NS_NETWORK_SOCKET_CONTRACTID_PREFIX) +
          nsDependentCString(type));

  rv = CallGetService(contractID.get(), result);
  if (NS_FAILED(rv)) 
      rv = NS_ERROR_UNKNOWN_SOCKET_TYPE;
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
