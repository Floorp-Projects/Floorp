/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#ifndef _nsBasicAuth_h_
#define _nsBasicAuth_h_

#include "nsISupports.h"

/* 
	The nsBasicAuth class converts a username:password string
	to a Base-64 encoded version. If you want to do other kind
	of encoding (MD5, Digest) there should really be a super 
	class that does the Authenticate function. Will add that later...

	-Gagan Saksena  08/17/1999
*/

class nsIURI;
class nsBasicAuth : public nsISupports
{

public:

	// Constructor and Destructor
	nsBasicAuth();
	virtual ~nsBasicAuth();

    // Functions from nsISupports
    NS_DECL_ISUPPORTS

	// Authenticate-- the actual method
	static nsresult Authenticate(
		nsIURI* iUri, 
		const char* i_Challenge, // "Basic realm='....'" 
		const char* i_UserPassString,  // username:password
		char** o_Output);

private:

};

#endif // _nsBasicAuth_h_
