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

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

nsresult
nsGetInterface::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
	nsresult status;

	if ( mSource )
		{
			nsCOMPtr<nsIInterfaceRequestor> factoryPtr = do_QueryInterface(mSource, &status);
			NS_ASSERTION(factoryPtr, "Did you know you were calling |do_GetInterface()| on an object that doesn't support the |nsIInterfaceRequestor| interface?");

			if ( factoryPtr )
				status = factoryPtr->GetInterface(aIID, aInstancePtr);

			if ( !NS_SUCCEEDED(status) )
				*aInstancePtr = 0;
		}
	else
		status = NS_ERROR_NULL_POINTER;

	if ( mErrorPtr )
		*mErrorPtr = status;
	return status;
}
