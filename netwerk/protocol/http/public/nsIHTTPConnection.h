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

#ifndef _nsIHTTPConnection_h_
#define _nsIHTTPConnection_h_

#include "nsIProtocolConnection.h"
#include "nsHTTPEnums.h"
class nsIHTTPEventSink;
/* 
    The nsIHTTPConnection class is the interface to an intance
    of the HTTP that acts on a per URL basis. This used to be the
    nsIHTTPInstance class when nsIProtocolConnection used to be 
    correctly called nsIProtocolInstance. If this confuses you
    don't blame me :-)

    -Gagan Saksena 03/06/99
*/

class nsIHTTPConnection : public nsIProtocolConnection
{

public:
    
    /*
        Request functions-
        These functions set/get parameters on the outbound request and may only
        be set before Load() function gets called. Calling them after 
        the Load() method will result in a NS_ERROR_ALREADY_CONNECTED
    */
    NS_IMETHOD          GetRequestHeader(const char* i_Header, const char* *o_Value) const = 0;
    NS_IMETHOD          SetRequestHeader(const char* i_Header, const char* i_Value) = 0;

    NS_IMETHOD          SetRequestMethod(HTTPMethod i_Method=HM_GET) = 0;

    /* 
        Response funtions. A call to any of these implicitly calls Load() on this
        protocol instance. 
    */
    NS_IMETHOD          GetResponseHeader(const char* i_Header, const char* *o_Value) = 0;
    
    NS_IMETHOD          GetResponseStatus(PRInt32* o_Status) = 0;
    
    NS_IMETHOD          GetResponseString(const char* *o_String) = 0;

    NS_IMETHOD          EventSink(nsIHTTPEventSink* *o_EventSink) const = 0;

    static const nsIID& GetIID() { 
        // {843D1020-D0DF-11d2-B013-006097BFC036}
        static const nsIID NS_IHTTPConnection_IID = 
            { 0x843d1020, 0xd0df, 0x11d2, { 0xb0, 0x13, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };

		return NS_IHTTPConnection_IID; 
	};

};

//Possible errors
//#define NS_ERROR_WHATEVER NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 400);

#endif /* _nsIHTTPConnection_h_ */
