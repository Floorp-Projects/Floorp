/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"

nsresult
nsQueryInterface::operator()( const nsIID& aIID, void** answer ) const
	{
		nsresult status;
		if ( mRawPtr )
			{
				status = mRawPtr->QueryInterface(aIID, answer);
#ifdef NSCAP_FEATURE_TEST_NONNULL_QUERY_SUCCEEDS
				NS_WARN_IF_FALSE(NS_SUCCEEDED(status), "interface not found---were you expecting that?");
#endif
			}
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
			NSCAP_RELEASE(this, mRawPtr);
	}
#endif

void
nsCOMPtr_base::assign_with_AddRef( nsISupports* rawPtr )
	{
    if ( rawPtr )
    	NSCAP_ADDREF(this, rawPtr);
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
