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

#ifndef _nsIHTTPResponse_h_
#define _nsIHTTPResponse_h_

#include "nsIHTTPCommonHeaders.h"

/* 
    The nsIHTTPResponse class is the interface to the response
    associated with a request. 

    -Gagan Saksena 03/06/99
*/

class nsIHTTPResponse : public nsIHTTPCommonHeaders
{

public:

    NS_IMETHOD              GetContentLength(PRInt32* o_Value) const = 0;
    NS_IMETHOD              GetStatus(PRUint32* o_Value) const = 0;
    NS_IMETHOD              GetStatusString(char* *o_String) const = 0;
    NS_IMETHOD              GetServer(char* *o_String) const = 0;

    static const nsIID& GetIID() { 
        // {A4FD6E60-FE7B-11d2-B019-006097BFC036}
        static const nsIID NS_IHTTPResponse_IID = 
            { 0xa4fd6e60, 0xfe7b, 0x11d2, { 0xb0, 0x19, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };

		return NS_IHTTPResponse_IID; 
	};

};

//Possible errors- place holder
//#define NS_ERROR_WHATEVER NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 400);

#endif /* _nsIHTTPResponse_h_ */
