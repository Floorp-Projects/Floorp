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

#ifndef _nsIProtocolHandler_h_
#define _nsIProtocolHandler_h_

#include "nsISupports.h"
#include "nsICoolURL.h"

/* 
    The nsIProtocolHandler class is a common interface to all protocols 
	being loaded dynamically. This base interface is required to be 
	derived from, for all protocols that expect to be loaded dynamically.

    Currently this just has functions to identify itself and return a 
	ProtocolInstance for the specified URL. I will add more things after 
	I find out more "common" elements for our two extreme cases HTTP and 
	IMAP/SMTP

    -Gagan Saksena

*/

class nsIProtocolHandler : public nsISupports
{

public:

    /*
        GetDefaultPort returns the default port associated with this 
        protocol. 
    */
    NS_IMETHOD_(PRInt32)    GetDefaultPort(void) const = 0;    

    /* 
        The GetProtocolInstance function. This function returns a per-URL 
		instance of a dummy class nsIProtocolInstance which could be used 
		for querying and setting per-URL properties of the connection. A 
		protocol implementor may decide to reuse the same connection and
        hence supply the same protocol instance to handle the specified URL.
    */
    NS_IMETHOD              GetProtocolInstance(nsICoolURL* i_URL, 
						    	nsIProtocolInstance* *o_Instance) = 0;

    /* 
        The GetScheme function uniquely identifies the scheme this handler 
		is associated with. 
    */
    NS_IMETHOD              GetScheme(const char* *o_Scheme) const = 0;

    static const nsIID& GetIID() { 
        // {63E10A11-CD1F-11d2-B013-006097BFC036}
        static const nsIID NS_IProtocolHandler_IID = 
            { 0x63e10a11, 0xcd1f, 0x11d2, { 0xb0, 0x13, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };
		return NS_IProtocolHandler_IID; 
	};

};

// Base Prog ID! Looks like a url but don't be fooled by its looks... it can kreate ya.
#define NS_COMPONENT_NETSCAPE_NETWORK_PROTOCOLS "component://netscape/network/protocols&name="

#endif /* _nsIProtocolHandler_h_ */
