/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
// #include "nsIWeakReference.h"

nsresult
nsQueryInterface::operator()( const nsIID& aIID, void** answer ) const
	{
		nsresult status;
		if ( mRawPtr )
			status = mRawPtr->QueryInterface(aIID, answer);
		else
			status = NS_ERROR_NULL_POINTER;
		
		if ( mErrorPtr )
			*mErrorPtr = status;
		return status;
	}

#ifdef NSCAP_FEATURE_FACTOR_DESTRUCTOR
nsCOMPtr_base::~nsCOMPtr_base()
	{
		if ( mRawPtr )
			NSCAP_RELEASE(mRawPtr);
	}
#endif

void
nsCOMPtr_base::assign_with_AddRef( nsISupports* rawPtr )
	{
    if ( rawPtr )
    	NSCAP_ADDREF(rawPtr);
    if ( mRawPtr )
      NSCAP_RELEASE(mRawPtr);
    mRawPtr = rawPtr;
	}

void
nsCOMPtr_base::assign_from_helper( const nsCOMPtr_helper& helper, const nsIID& iid )
	{
		nsISupports* newRawPtr;
		if ( !NS_SUCCEEDED( helper(iid, NSCAP_REINTERPRET_CAST(void**, &newRawPtr)) ) )
			newRawPtr = 0;
		if ( mRawPtr )
			NSCAP_RELEASE(mRawPtr);
		mRawPtr = newRawPtr;
	}


#if 0
void
nsCOMPtr_base::assign_with_QueryInterface( nsISupports* rawPtr, const nsIID& iid, nsresult* result )
  {
    nsresult status = NS_ERROR_NULL_POINTER;
    if ( !rawPtr || !NS_SUCCEEDED( status = rawPtr->QueryInterface(iid, NSCAP_REINTERPRET_CAST(void**, &rawPtr)) ) )
      rawPtr = 0;

    if ( mRawPtr )
      NSCAP_RELEASE(mRawPtr);

    mRawPtr = rawPtr;

    if ( result )
      *result = status;
  }

void
nsCOMPtr_base::assign_with_QueryReferent( nsIWeakReference* weakPtr, const nsIID& iid, nsresult* result )
  {
    nsresult status = NS_ERROR_NULL_POINTER;

    nsISupports* rawPtr;
    if ( !weakPtr || !NS_SUCCEEDED( status = weakPtr->QueryReferent(iid, NSCAP_REINTERPRET_CAST(void**, &rawPtr)) ) )
      rawPtr = 0;

    if ( mRawPtr )
      NSCAP_RELEASE(mRawPtr);

    mRawPtr = rawPtr;

    if ( result )
      *result = status;
  }
#endif

void**
nsCOMPtr_base::begin_assignment()
  {
    if ( mRawPtr )
    	{
	      NSCAP_RELEASE(mRawPtr);
	    	mRawPtr = 0;
    	}

    return NSCAP_REINTERPRET_CAST(void**, &mRawPtr);
  }
