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

#if 0
nsresult
nsGetInterface::operator()( const nsIID& aIID, void** aInstancePtr ) const
	{
		nsCOMPtr<nsIInterfaceRequestor> factoryP = do_QueryInterface(mSource);
		NS_ASSERTION(factoryP, "Did you know you were calling |do_GetInterface()| on an object that doesn't support the |nsIInterfaceRequestor| interface?");

		nsresult status = factoryP ? factoryP->GetInterface(aIID, aInstancePtr) : NS_ERROR_NO_INTERFACE;
		if ( !NS_SUCCEEDED(status) )
			*aInstancePtr = 0;

		if ( mErrorPtr )
			*mErrorPtr = status;
		return status;
	}
#endif