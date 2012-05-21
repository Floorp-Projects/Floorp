/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

nsresult
nsGetInterface::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
  nsresult status;

  if ( mSource )
  {
    nsCOMPtr<nsIInterfaceRequestor> factoryPtr = do_QueryInterface(mSource, &status);

    if ( factoryPtr )
      status = factoryPtr->GetInterface(aIID, aInstancePtr);
    else
      status = NS_ERROR_NO_INTERFACE;
  }
  else
    status = NS_ERROR_NULL_POINTER;

  if ( NS_FAILED(status) )
    *aInstancePtr = 0;
  if ( mErrorPtr )
    *mErrorPtr = status;
  return status;
}
