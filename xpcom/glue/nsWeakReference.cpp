/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Scott Collins <scc@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// nsWeakReference.cpp

#include "nsWeakReference.h"
#include "nsCOMPtr.h"

nsresult
nsQueryReferent::operator()( const nsIID& aIID, void** answer ) const
  {
    nsresult status;
    if ( mWeakPtr )
      {
        if ( !NS_SUCCEEDED(status = mWeakPtr->QueryReferent(aIID, answer)) )
          *answer = 0;
      }
    else
      status = NS_ERROR_NULL_POINTER;

    if ( mErrorPtr )
      *mErrorPtr = status;
    return status;
  }

nsresult
nsGetWeakReference::operator()( const nsIID&, void** aResult ) const
  {
    nsresult status;
    // nsIWeakReference** result = &NS_STATIC_CAST(nsIWeakReference*, *aResult);
    *aResult = 0;

    if ( mRawPtr )
      {
        nsCOMPtr<nsISupportsWeakReference> factoryPtr = do_QueryInterface(mRawPtr, &status);
        NS_ASSERTION(factoryPtr, "Oops!  You're asking for a weak reference to an object that doesn't support that.");
        if ( factoryPtr )
          {
            nsIWeakReference* temp;
            status = factoryPtr->GetWeakReference(&temp);
            *aResult = temp;
          }
        // else, |status| has already been set by |do_QueryInterface|
      }
    else
      status = NS_ERROR_NULL_POINTER;

    if ( mErrorPtr )
      *mErrorPtr = status;
    return status;
  }


NS_COM nsIWeakReference*  // or else |already_AddRefed<nsIWeakReference>|
NS_GetWeakReference( nsISupports* aInstancePtr, nsresult* aErrorPtr )
  {
    void* result = 0;
    nsGetWeakReference(aInstancePtr, aErrorPtr)(NS_GET_IID(nsIWeakReference), &result);
    return NS_STATIC_CAST(nsIWeakReference*, result);
  }

NS_IMETHODIMP
nsSupportsWeakReference::GetWeakReference( nsIWeakReference** aInstancePtr )
  {
    if ( !aInstancePtr )
      return NS_ERROR_NULL_POINTER;

    if ( !mProxy )
      mProxy = new nsWeakReference(this);
    *aInstancePtr = mProxy;

    nsresult status;
    if ( !*aInstancePtr )
      status = NS_ERROR_OUT_OF_MEMORY;
    else
      {
        NS_ADDREF(*aInstancePtr);
        status = NS_OK;
      }

    return status;
  }

NS_IMETHODIMP_(nsrefcnt)
nsWeakReference::AddRef()
  {
    return ++mRefCount;
  }

NS_IMETHODIMP_(nsrefcnt)
nsWeakReference::Release()
  {
    nsrefcnt temp = --mRefCount;
    if ( !mRefCount )
      delete this;
    return temp;
  }

NS_IMETHODIMP
nsWeakReference::QueryInterface( const nsIID& aIID, void** aInstancePtr )
  {
    NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!");

    if ( !aInstancePtr )
      return NS_ERROR_NULL_POINTER;

    nsISupports* foundInterface;
    if ( aIID.Equals(NS_GET_IID(nsIWeakReference)) )
      foundInterface = NS_STATIC_CAST(nsIWeakReference*, this);
    else if ( aIID.Equals(NS_GET_IID(nsISupports)) )
      foundInterface = NS_STATIC_CAST(nsISupports*, this);
    else
      foundInterface = 0;

    nsresult status;
    if ( !foundInterface )
      status = NS_NOINTERFACE;
    else
      {
        NS_ADDREF(foundInterface);
        status = NS_OK;
      }

    *aInstancePtr = foundInterface;
    return status;
  }

NS_IMETHODIMP
nsWeakReference::QueryReferent( const nsIID& aIID, void** aInstancePtr )
  {
    return mReferent ? mReferent->QueryInterface(aIID, aInstancePtr) : NS_ERROR_NULL_POINTER;
  }
