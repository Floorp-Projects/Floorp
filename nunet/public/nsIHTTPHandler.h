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

#ifndef _nsIHTTPHandler_h_
#define _nsIHTTPHandler_h_

#include "nsIProtocolHandler.h"

/* 
    The nsIHTTPHandler class is the bare minimum interface expected for 
    an HTTP handler. The set of interfaces this handler is derived from decides
    the expected behaviour. For example another HTTPHandler may decide
    not to use proxies or to support proxy connections, in which case
    it will not derive from nsIProxy.

    -Gagan Saksena 02/25/99
*/

class nsIHTTPHandler : public nsIProtocolHandler//, public nsIProxy 
// TODO should also have caching interfaces
// as well as security stuff, etc.
{

public:

    /*
        GetDefaultPort returns the default port associated with this 
        protocol. 
    */
    NS_METHOD_(PRInt32)    GetDefaultPort(void) const 
    {
        static const PRInt32 defaultPort = 80;
        return defaultPort;
    };    

    /* 
        The GetScheme function uniquely identifies the scheme this handler 
		is associated with. 
    */
    NS_METHOD              GetScheme(const char* *o_Scheme) const
    {
        static const char* scheme = "http";
        *o_Scheme = scheme;
        return NS_OK;
    };


    static const nsIID& GetIID() { 
        // {A3BE3AF0-CD2D-11d2-B013-006097BFC036}
        static const nsIID NS_IHTTPHandler_IID = 
            { 0xa3be3af0, 0xcd2d, 0x11d2, { 0xb0, 0x13, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };
		return NS_IHTTPHandler_IID; 
	};

protected:

};

//Possible errors
#define NS_ERROR_BAD_REQUEST    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 200);

// Create (or get) HTTP Handler
extern NS_METHOD CreateOrGetHTTPHandler(nsIHTTPHandler* *o_HTTPHandler);

#endif /* _nsIHTTPHandler_h_ */
