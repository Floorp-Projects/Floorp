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

#ifndef _nsIHTTPInstance_h_
#define _nsIHTTPInstance_h_

#include "nsIProtocolInstance.h"

typedef enum {
    HTTP_ZERO_NINE, // HTTP/0.9
    HTTP_ONE_ZERO,  // HTTP/1.0
    HTTP_ONE_ONE    // HTTP/1.1
} HTTPVersion;

/* 
    The nsIHTTPInstance class is the interface to an intance
    of the HTTP that acts on a per URL basis.

    There are a ton of things that an instance should hold
    but due to the lack of time I am letting those things
    be used from the old code base. Someday someone will
    step in and get rid of old code and put things here
    like socket, other connection data, etc. 

    -Gagan Saksena 03/06/99
*/

class nsIHTTPInstance : public nsIProtocolInstance
{

public:
    
#if 0 // TODO Past N2 things. As much as I want it now.... so much
    // to do and so little resources. Argh...!
    // Will have to write nsIHTTPRequest and nsIHTTPResponse classes
    // both derived from nsIHeader. The response's setheader should
    // return NS_ERROR_ARE_YOU_NUTS?

    // TODO think if the Request/Response should move out in 
    // its own interface. Would other protocols like to 
    // share this interface?

    /*
        Request functions-
        These functions set/get parameters on the outbound request and may only
        be set before Load() function gets called. Calling them after 
        the Load() method will result in a NS_ERROR_ALREADY_CONNECTED
    */
    NS_IMETHOD          GetRequest(nsIHTTPRequest* *o_Request) const = 0;
    NS_IMETHOD          SetRequest(nsIHTTPRequest* i_Request) = 0;

    /* 
        Response funtion. A call to this implicitly calls Load() on this
        protocol instance. Thats why it's not const.
    */
    NS_IMETHOD          GetResponse(nsIHTTPResponse* *o_Response) = 0;
#endif // Past N2 things...

    // All request functions before the Load gets called. These 
    // should move to nsIHTTPRequest

    /*
        I am not describing each since most are obvious and then
        it would be better to describe them in nsIHTTPRequest
        For most of these headers there is a default that gets
        set in a default HTTP request which can be modified from the 
        nsIHTTPHandler. 

        This is definitely not the complete set. But nsIHTTPRequest will
        have it. TODO...
    */
    NS_IMETHOD              SetAccept(const char* i_AcceptHeader) = 0;
    //NS_IMETHOD            SetAcceptType() = 0;
    NS_IMETHOD              SetCookie(const char* i_Cookie) = 0;
    NS_IMETHOD              SetUserAgent(const char* i_UserAgent) = 0;
    NS_IMETHOD              SetHTTPVersion(HTTPVersion i_Version = HTTP_ONE_ONE) = 0;

    // All response functions which if called, will call Load implicitly.
    // Should move these to nsIHTTPResponse.

    NS_IMETHOD_(PRInt32)    GetContentLength(void) const = 0;
    NS_IMETHOD              GetContentType(const char* *o_Type) const = 0;
    //NS_IMETHOD_(PRTime)   GetDate(void) const = 0;
    NS_IMETHOD_(PRInt32)    GetResponseStatus(void) const = 0;
    NS_IMETHOD              GetResponseString(const char* *o_String) const = 0;
    NS_IMETHOD              GetServer(const char* *o_String) const = 0;

    static const nsIID& GetIID() { 
        // {843D1020-D0DF-11d2-B013-006097BFC036}
        static const nsIID NS_IHTTPInstance_IID = 
            { 0x843d1020, 0xd0df, 0x11d2, { 0xb0, 0x13, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };

		return NS_IHTTPInstance_IID; 
	};

};

//Possible errors
//#define NS_ERROR_WHATEVER NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 400);

#endif /* _nsIHTTPInstance_h_ */
