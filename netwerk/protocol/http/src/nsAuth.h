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

#ifndef _nsAuth_h_
#define _nsAuth_h_

/* 
	The nsAuth struct holds all the information about a site authentication.
	The username, password are needed here since a URI may not contain this
	information and the user may have entered those thru a interactive dialog
	box.

	-Gagan Saksena  08/17/1999
*/

#include "nsIURI.h"
#include "nsCOMPtr.h"

class nsAuth
{
public:
	// Constructor and Destructor
    // TODO add stuff for realm and user/pass as well!!
	nsAuth(nsIURI* iURI, 
            const char* authString, 
            const char* username = 0,
            const char* password = 0,
            const char* realm = 0);
	virtual ~nsAuth();

	char* 		encodedString;
	char* 		password; 
	char* 		realm; 
	char* 		username;
	// When we do proxy authentication this would be a union for the same.
	nsCOMPtr<nsIURI> 	uri;
};

#endif // _nsAuth_h_
