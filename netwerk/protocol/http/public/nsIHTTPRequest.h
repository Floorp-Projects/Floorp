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

#ifndef _nsIHTTPRequest_h_
#define _nsIHTTPRequest_h_

#include "nsIHTTPCommonHeaders.h"
// #include "nsIHTTPHandler.h" 
#include "nsHTTPEnums.h"
/* 
    The nsIHTTPRequest class is the interface to an intance
    of the HTTP request that gets created on a per URL/connection basis.

    -Gagan Saksena 03/06/99
*/

class nsIHTTPRequest : public nsIHTTPCommonHeaders
{

public:
    
    NS_IMETHOD              SetAccept(const char* i_AcceptHeader) = 0;
    NS_IMETHOD              SetUserAgent(const char* i_UserAgent) = 0;
    NS_IMETHOD              SetHTTPVersion(HTTPVersion i_Version = HTTP_ONE_ONE) = 0;

    static const nsIID& GetIID() { 
        // {A4FD6E61-FE7B-11d2-B019-006097BFC036}
        static const nsIID NS_IHTTPRequest_IID = 
        { 0xa4fd6e61, 0xfe7b, 0x11d2, { 0xb0, 0x19, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };

		return NS_IHTTPRequest_IID; 
	};

};

//Possible errors- place holder
//#define NS_ERROR_WHATEVER NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 400);

#endif /* _nsIHTTPRequest_h_ */
