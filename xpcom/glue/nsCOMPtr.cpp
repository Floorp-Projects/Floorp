/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
