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
 *   Scott Collins <scc@netscape.com>
 */

#include "nsCOMPtr.h"

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
    assign_assuming_AddRef(rawPtr);
	}

void
nsCOMPtr_base::assign_from_helper( const nsCOMPtr_helper& helper, const nsIID& iid )
	{
		nsISupports* newRawPtr;
		if ( !NS_SUCCEEDED( helper(iid, NS_REINTERPRET_CAST(void**, &newRawPtr)) ) )
			newRawPtr = 0;
    assign_assuming_AddRef(newRawPtr);
	}

void**
nsCOMPtr_base::begin_assignment()
  {
    assign_assuming_AddRef(0);
    return NS_REINTERPRET_CAST(void**, &mRawPtr);
  }
