/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"

nsresult
nsQueryInterface::operator()( const nsIID& aIID, void** answer ) const
	{
		nsresult status;
		if ( mRawPtr )
			{
				status = mRawPtr->QueryInterface(aIID, answer);
#ifdef NSCAP_FEATURE_TEST_NONNULL_QUERY_SUCCEEDS
				NS_ASSERTION(NS_SUCCEEDED(status), "interface not found---were you expecting that?");
#endif
			}
		else
			status = NS_ERROR_NULL_POINTER;
		
		return status;
	}

nsresult
nsQueryInterfaceWithError::operator()( const nsIID& aIID, void** answer ) const
	{
		nsresult status;
		if ( mRawPtr )
			{
				status = mRawPtr->QueryInterface(aIID, answer);
#ifdef NSCAP_FEATURE_TEST_NONNULL_QUERY_SUCCEEDS
				NS_ASSERTION(NS_SUCCEEDED(status), "interface not found---were you expecting that?");
#endif
			}
		else
			status = NS_ERROR_NULL_POINTER;
		
		if ( mErrorPtr )
			*mErrorPtr = status;
		return status;
	}

void
nsCOMPtr_base::assign_with_AddRef( nsISupports* rawPtr )
	{
    if ( rawPtr )
    	NSCAP_ADDREF(this, rawPtr);
    assign_assuming_AddRef(rawPtr);
	}

void
nsCOMPtr_base::assign_from_qi( const nsQueryInterface qi, const nsIID& iid )
  {
    void* newRawPtr;
    if ( NS_FAILED( qi(iid, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<nsISupports*>(newRawPtr));
  }

void
nsCOMPtr_base::assign_from_qi_with_error( const nsQueryInterfaceWithError& qi, const nsIID& iid )
  {
    void* newRawPtr;
    if ( NS_FAILED( qi(iid, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<nsISupports*>(newRawPtr));
  }

void
nsCOMPtr_base::assign_from_gs_cid( const nsGetServiceByCID gs, const nsIID& iid )
  {
    void* newRawPtr;
    if ( NS_FAILED( gs(iid, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<nsISupports*>(newRawPtr));
  }

void
nsCOMPtr_base::assign_from_gs_cid_with_error( const nsGetServiceByCIDWithError& gs, const nsIID& iid )
  {
    void* newRawPtr;
    if ( NS_FAILED( gs(iid, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<nsISupports*>(newRawPtr));
  }

void
nsCOMPtr_base::assign_from_gs_contractid( const nsGetServiceByContractID gs, const nsIID& iid )
  {
    void* newRawPtr;
    if ( NS_FAILED( gs(iid, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<nsISupports*>(newRawPtr));
  }

void
nsCOMPtr_base::assign_from_gs_contractid_with_error( const nsGetServiceByContractIDWithError& gs, const nsIID& iid )
  {
    void* newRawPtr;
    if ( NS_FAILED( gs(iid, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<nsISupports*>(newRawPtr));
  }

void
nsCOMPtr_base::assign_from_helper( const nsCOMPtr_helper& helper, const nsIID& iid )
  {
    void* newRawPtr;
    if ( NS_FAILED( helper(iid, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<nsISupports*>(newRawPtr));
  }

void**
nsCOMPtr_base::begin_assignment()
  {
    assign_assuming_AddRef(0);
    return reinterpret_cast<void**>(&mRawPtr);
  }
