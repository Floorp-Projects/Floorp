/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#ifndef _nsAuth_h_
#define _nsAuth_h_

/* 
	The nsAuth struct holds all the information about a site authentication.
	The username, password are needed here since a URI may not contain this
	information and the user may have entered those thru a interactive dialog
	box.

	-Gagan Saksena  08/17/1999
*/

class nsIURI;

struct nsAuth 
{

	// Constructor and Destructor
	nsAuth(nsIURI* iURI);
	virtual ~nsAuth();

	char* 		encodedString;
	char* 		password; 
	char* 		realm; 
	char* 		username;
	// When we do proxy authentication this would be a union for the same.
	nsIURI* 	uri;
};

#endif // _nsAuth_h_
